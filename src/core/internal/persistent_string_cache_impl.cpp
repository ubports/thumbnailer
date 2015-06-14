/*
 * Copyright (C) 2015 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <core/internal/persistent_string_cache_impl.h>

#include <core/internal/persistent_string_cache_stats.h>

#include <leveldb/cache.h>
#include <leveldb/write_batch.h>

#include <iomanip>
#include <system_error>

/*
    We have three tables and two secondary indexes in the DB:

    - Key -> Value
      The Values table maps keys to values.

    - Key -> <Access time, Expiry time, Size>
      The Data table maps keys to the access time, expire time,
      and entry size. (Size is the sum of key, value, and metadata sizes.)

    - Key -> Metadata
      The Metadata table maps keys to metadata for the entry.
      If no metadata exists for an entry, there is no row
      in the table.

    - <Access time, Key> -> Size
      The Atime index provides access in order of oldest-to-newest access time.
      This allows efficient trimming based on LRU order.

    - <Expiry time, Key> -> Size
      The Etime index provides access in order of soonest-to-latest expiry time.
      This allows efficient trimming of expired entries. For lru_only,
      no entry is added to this index, and the corresponding expiry time in the
      Data table is 0. For lru_ttl, only entries that actually
      do have an expiry time are added.

    The tables and indexes each map to a different region of the leveldb based on a prefix.
    Tuple entries are separated by spaces. Entries are sorted in lexicographical order
    by the DB; to ensure correct numerical comparison for the secondary indexes,
    times are in milliseconds since the epoch, with fixed-width width zero padding
    and 13 decimal digits. (That works out to more than 316 years past the epoch.)

    Some examples to illustrate how it hangs together with lru_ttl. (Note that,
    in reality, all four tables really sit inside the single leveldb table, separated
    by the prefixes of their keys. They are shown as separate tables below to
    make things easier to read.)

    At time 0010, insert Bjarne -> Stroustrup, expires 1010,
    at time 0020, insert Andy   -> Koenig,     expires 2020,
    at time 0030, insert Scott  -> Meyers,     does not expire
    at time 0040, insert Stan   -> Lippman,    expires 1040

    Values:                         Data:

    Key     | Value                 Key     | Access time | Expiry time | Size
    --------+-----------            --------+---------------------------------
    AAndy   | Koenig                BAndy   |      20     |    2020     |  10
    ABjarne | Stroustrup            BBjarne |      10     |    1010     |  16
    AScott  | Meyers                BScott  |      30     |       0     |  11
    AStan   | Lippman               BStan   |      40     |    1040     |  11


    Atime index:                    Etime index:

    Key                   | Size    Key                   | Size
    ----------------------+-----    ----------------------+-----
    D0000000000010 Bjarne |  16     E0000000001010 Bjarne |  16
    D0000000000020 Andy   |  10     E0000000001040 Stan   |  11
    D0000000000030 Scott  |  11     E0000000002020 Andy   |  10
    D0000000000040 Stan   |  11

    Note that, because the expiry time for Scott is infinite, no entry appears in the Etime index.

    At time 100, we call get("Bjarne"). This updates the Data table and Atime index with the new access time.
    (The Values table and Etime index are unchanged.)

    Values:                         Data:

    Key     | Value                 Key     | Access time | Expiry time | Size
    --------+-----------            --------+---------------------------------
    AAndy   | Koenig                BAndy   |      20     |    2020     |  10
    ABjarne | Stroustrup            BBjarne |     100     |    1010     |  16
    AScott  | Meyers                BScott  |      30     |       0     |  11
    AStan   | Lippman               BStan   |      40     |    1040     |  11


    Atime index:                    Etime index:

    Key                   | Size    Key                   | Size
    ----------------------+-----    ----------------------+-----
    D0000000000020 Andy   |  10     E0000000001010 Bjarne |  16
    D0000000000030 Scott  |  11     E0000000001040 Stan   |  11
    D0000000000040 Stan   |  11     E0000000002020 Andy   |  10
    D0000000000100 Bjarne |  16

    In other words, the Atime and Etime indexes are always sorted in earliest-to-latest order
    of expiry; this allows us to efficiently trim the cache once it is full. The sizes are
    stored redundantly so we can efficiently determine the point at which we have removed
    enough entries in order to make room for a new one.

    If this example were to use lru_only, the Etime index would remain empty, and the expiry times
    in the Data table would all be chrono::duration_cast<chrono::milliseconds>(chrono::time_point()).count().
    That value typically is zero (but this is not guaranteed by the standard).

    The Metadata table simply maps each key to its metadata. If no metadata exists
    for an entry, there is no corresponding row in the table. For example, if Andy
    and Scott had metadata, but Stan and Bjarne didn't, we'd have:

    Metadata table:

    Key     | Metadata
    --------+----------
    CAndy   |  <data>
    CScott  |  <data>
*/

using namespace std;

namespace core
{

namespace internal
{

namespace
{

static string const class_name = "PersistentStringCache";  // For exception messages

static leveldb::WriteOptions write_options;
static leveldb::ReadOptions read_options;

// Schema version. If the way things are written to leveldb changes, the
// schema version here must be changed, too. If an existing cache is opened
// with a different schema version, the cache is simply thrown away, so
// it will automatically be re-created using the latest schema.

static int const SCHEMA_VERSION = 2;  // Increment whenever schema changes!

// Prefixes to divide the key space into logical tables/indexes.
// All prefixes must have length 1. The end prefix must be
// the "one past the end" value for the prefix range. For example,
// "B" is the first prefix that can't be an entry in the Values table.
//
// Do not change the prefix without also checking that ALL_BEGIN and
// ALL_END are still correct!

static string const VALUES_BEGIN = "A";
static string const VALUES_END = "B";

static string const DATA_BEGIN = "B";
static string const DATA_END = "C";

static string const METADATA_BEGIN = "C";
static string const METADATA_END = "D";

static string const ATIME_BEGIN = "D";
static string const ATIME_END = "E";

static string const ETIME_BEGIN = "E";
static string const ETIME_END = "F";

// We store the stats so they are not lost across process re-starts.

static string const STATS_BEGIN = "X";
static string const STATS_END = "Y";

// The settings range stores data about the cache itself, such as
// max size and expiration policy. The prefix for this
// range must be outside the range [ALL_BEGIN..ALL_END).
// The schema version is there so we can change the way things are written into leveldb
// and detect when an old cache is opened with a newer version.

static string const SETTINGS_BEGIN = "Y";
static string const SETTINGS_END = "Z";

// This key stores a dirty flag that we set after successful open
// and clear in the destructor. This allows to detect, on start-up
// if we shut down cleanly.

static string const DIRTY_FLAG = "!DIRTY";

// These span the entire range of keys in all tables and stats (except settings and dirty flag).

static string const ALL_BEGIN = VALUES_BEGIN;  // Must be lowest prefix for all tables and indexes, incl stats.
static string const ALL_END = SETTINGS_BEGIN;  // Must be highest prefix for all tables and indexes, incl stats.

static string const SETTINGS_MAX_SIZE = SETTINGS_BEGIN + "MAX_SIZE";
static string const SETTINGS_POLICY = SETTINGS_BEGIN + "POLICY";
static string const SETTINGS_SCHEMA_VERSION = SETTINGS_BEGIN + "SCHEMA_VERSION";

static string const STATS_VALUES = STATS_BEGIN + "VALUES";

// Simple struct to serialize/deserialize a time-key tuple.
// For the stringified representation, time and key are
// separated by a space.

struct TimeKeyTuple
{
    int64_t time;  // msec since the epoch
    string key;

    TimeKeyTuple(int64_t t, string const& s)
        : time(t)
        , key(s)
    {
    }

    TimeKeyTuple(string const& s)
    {
        auto pos = s.find(' ');
        assert(pos != string::npos);
        string t(s.substr(0, pos));
        istringstream is(t);
        is >> time;
        assert(!is.bad());
        key = s.substr(pos + 1);
    }

    TimeKeyTuple(TimeKeyTuple const&) = default;
    TimeKeyTuple(TimeKeyTuple&&) = default;

    TimeKeyTuple& operator=(TimeKeyTuple const&) = default;
    TimeKeyTuple& operator=(TimeKeyTuple&&) = default;

    string to_string() const
    {
        // We zero-fill the time, so entries collate lexicographically in old-to-new order.
        ostringstream os;
        os << setfill('0') << setw(13) << time << " " << key;
        return os.str();
    }
};

// Key creation methods. These methods return the key into the corresponding
// table or index with the correct prefix and with tuple keys concatenated
// with a space separator.

string k_data(string const& key)
{
    return DATA_BEGIN + key;
}

string k_metadata(string const& key)
{
    return METADATA_BEGIN + key;
}

string k_atime_index(int64_t atime, string const& key)
{
    return ATIME_BEGIN + TimeKeyTuple(atime, key).to_string();
}

string k_etime_index(int64_t etime, string const& key)
{
    return ETIME_BEGIN + TimeKeyTuple(etime, key).to_string();
}

// Little helpers to get milliseconds since the epoch.

int64_t ticks(chrono::time_point<chrono::system_clock> tp)
{
    return chrono::duration_cast<chrono::milliseconds>(tp.time_since_epoch()).count();
}

int64_t now_ticks()
{
    return ticks(chrono::system_clock::now());
}

// Usually zero, but the standard doesn't guarantee this.

static auto const clock_origin = ticks(chrono::system_clock::time_point());

int64_t epoch_ticks()
{
    return clock_origin;
}

typedef std::unique_ptr<leveldb::Iterator> IteratorUPtr;

#ifndef NDEBUG

// For assertions, so we can verify that num_entries_ matches the sum of entries in the histogram.

int64_t hist_sum(PersistentCacheStats::Histogram const& h) noexcept
{
    int64_t size = 0;
    for (auto num : h)
    {
        size += num;
    }
    return size;
}

#endif

}  // namespace

void PersistentStringCacheImpl::init_stats()
{
    int64_t num = 0;
    int64_t size = 0;

    // If we shut down cleanly last time, read the saved stats values.
    bool is_dirty = read_dirty_flag();
    if (!is_dirty)
    {
        read_stats();
    }
    else
    {
        // We didn't shut down cleanly or the cache is new.
        // Run over the Atime index (it's smaller than the Data table)
        // and count the number of entries and bytes, and initialize
        // the histogram.
        IteratorUPtr it(db_->NewIterator(read_options));
        leveldb::Slice const atime_prefix(ATIME_BEGIN);
        it->Seek(atime_prefix);
        while (it->Valid() && it->key().starts_with(atime_prefix))
        {
            ++num;
            auto bytes = stoll(it->value().ToString());
            size += bytes;
            stats_->hist_increment(bytes);
            it->Next();
        }
        throw_if_error(it->status(), "cannot initialize cache");
        stats_->num_entries_ = num;
        stats_->cache_size_ = size;
    }
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
}

// Open existing database or create an empty one.

PersistentStringCacheImpl::PersistentStringCacheImpl(string const& cache_path,
                                                     int64_t max_size_in_bytes,
                                                     CacheDiscardPolicy policy,
                                                     PersistentStringCache* pimpl)
    : pimpl_(pimpl)
    , stats_(make_shared<PersistentStringCacheStats>())
{
    stats_->cache_path_ = cache_path;
    if (max_size_in_bytes < 1)
    {
        throw_invalid_argument("invalid max_size_in_bytes (" + to_string(max_size_in_bytes) + "): value must be > 0");
    }
    stats_->max_cache_size_ = max_size_in_bytes;
    stats_->policy_ = policy;

    leveldb::Options options;
    options.create_if_missing = true;

    // For small caches, reduce memory consumption by reducing the size of the internal block cache.
    // The block cache size is at least 512 kB. For caches 5-80 MB, it is 10% of the nominal cache size.
    // For caches > 80 MB, the block cache is left at the default of 8 MB.
    size_t block_cache_size = max_size_in_bytes / 10;
    if (block_cache_size < 512 * 1024)
    {
        block_cache_size = 512 * 1024;
    }
    if (block_cache_size < 8 * 1024 * 1024)
    {
        block_cache_.reset(leveldb::NewLRUCache(block_cache_size));
        options.block_cache = block_cache_.get();
    }

    init_db(options);

    if (cache_is_new())
    {
        write_version();
        write_settings();
        write_stats();
    }
    else
    {
        check_version();  // Wipes DB if version doesn't match.
        read_settings();

        // For an already-existing cache, check that size and policy match.
        if (stats_->max_cache_size_ != max_size_in_bytes)
        {
            throw_logic_error(string("existing cache opened with different max_size_in_bytes (") +
                              to_string(max_size_in_bytes) + "), existing size = " +
                              to_string(stats_->max_cache_size_));
        }
        if (stats_->policy_ != policy)
        {
            auto to_string = [](CacheDiscardPolicy p)
            {
                return p == core::CacheDiscardPolicy::lru_only ? "lru_only" : "lru_ttl";
            };
            string msg = string("existing cache opened with different policy (") + to_string(policy) +
                         "), existing policy = " + to_string(stats_->policy_);
            throw_logic_error(msg);
        }
    }

    init_stats();
    write_dirty_flag(true);
}

// Open existing database.

PersistentStringCacheImpl::PersistentStringCacheImpl(string const& cache_path, PersistentStringCache* pimpl)
    : pimpl_(pimpl)
    , stats_(make_shared<PersistentStringCacheStats>())
{
    stats_->cache_path_ = cache_path;

    init_db(leveldb::Options());  // Throws if DB doesn't exist.

    check_version();  // Wipes DB if version doesn't match.
    read_settings();

    init_stats();
    write_dirty_flag(true);
}

PersistentStringCacheImpl::~PersistentStringCacheImpl()
{
    try
    {
        write_stats();
        write_dirty_flag(false);
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        cerr << make_message(string("~PersistentStringCacheImpl(): ") + e.what()) << endl;
    }
    catch (...)
    {
        cerr << make_message("~PersistentStringCacheImpl(): unknown exception") << endl;
    }
    // LCOV_EXCL_STOP
}

bool PersistentStringCacheImpl::get(string const& key, string& value) const
{
    return get(key, value, nullptr);
}

bool PersistentStringCacheImpl::get(string const& key, string& value, string* metadata) const
{
    if (key.empty())
    {
        throw_invalid_argument("get(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    DataTuple dt;
    bool found = get_value_and_metadata(key, dt, value, metadata);
    if (!found)
    {
        stats_->inc_misses();
        call_handler(key, CacheEventIndex::miss);
        return false;
    }

    // Don't return expired entry.
    int64_t new_atime = now_ticks();
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime <= new_atime)
    {
        call_handler(key, CacheEventIndex::miss);
        stats_->inc_misses();
        return false;
    }

    leveldb::WriteBatch batch;

    batch.Delete(k_atime_index(dt.atime, key));  // Delete old atime entry
    dt.atime = new_atime;
    dt.size = key.size() + value.size();
    if (metadata)
    {
        dt.size += metadata->size();
    }
    batch.Put(data_key, dt.to_string());
    batch.Put(k_atime_index(dt.atime, key), to_string(dt.size));

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "put()");

    stats_->inc_hits();
    call_handler(key, CacheEventIndex::get);
    return true;
}

bool PersistentStringCacheImpl::get_metadata(string const& key, string& metadata) const
{
    if (key.empty())
    {
        throw_invalid_argument("get_metadata(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    bool found;
    auto dt = get_data(data_key, found);
    if (!found)
    {
        return false;
    }

    // Don't return expired entry.
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime <= now_ticks())
    {
        return false;
    }
    auto s = db_->Get(read_options, k_metadata(key), &metadata);
    throw_if_error(s, "get_metadata()");
    return !s.IsNotFound();
}

bool PersistentStringCacheImpl::contains_key(string const& key) const
{
    if (key.empty())
    {
        throw_invalid_argument("contains_key(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    bool found;
    auto dt = get_data(data_key, found);
    if (!found)
    {
        return false;
    }
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime <= now_ticks())
    {
        return false;  // Expired entries are not returned.
    }
    return true;
}

int64_t PersistentStringCacheImpl::size() const noexcept
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    return stats_->num_entries_;
}

int64_t PersistentStringCacheImpl::size_in_bytes() const noexcept
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    return stats_->cache_size_;
}

int64_t PersistentStringCacheImpl::max_size_in_bytes() const noexcept
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    return stats_->max_cache_size_;
}

int64_t PersistentStringCacheImpl::disk_size_in_bytes() const
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    leveldb::Range everything(ALL_BEGIN, SETTINGS_END);
    array<uint64_t, 1> sizes = {{0}};
    db_->GetApproximateSizes(&everything, 1, sizes.data());
    return sizes[0];
}

CacheDiscardPolicy PersistentStringCacheImpl::discard_policy() const noexcept
{
    return stats_->policy_;  // Immutable
}

PersistentCacheStats PersistentStringCacheImpl::stats() const
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    // We make a copy here so values can't change underneath the caller.
    return PersistentCacheStats(make_shared<PersistentStringCacheStats>(*stats_));
}

bool PersistentStringCacheImpl::put(string const& key,
                                    string const& value,
                                    chrono::time_point<chrono::system_clock> expiry_time)
{
    return put(key, value.data(), value.size(), nullptr, 0, expiry_time);
}

bool PersistentStringCacheImpl::put(string const& key,
                                    char const* value_data,
                                    int64_t value_size,
                                    chrono::time_point<chrono::system_clock> expiry_time)
{
    return put(key, value_data, value_size, nullptr, 0, expiry_time);
}

bool PersistentStringCacheImpl::put(string const& key,
                                    string const& value,
                                    string const* metadata,
                                    chrono::time_point<chrono::system_clock> expiry_time)
{
    assert(metadata);
    return put(key,
               value.data(), value.size(),
               metadata->data(), metadata->size(),
               expiry_time);
}

bool PersistentStringCacheImpl::put(string const& key,
                                    char const* value_data,
                                    int64_t value_size,
                                    char const* metadata_data,
                                    int64_t metadata_size,
                                    chrono::time_point<chrono::system_clock> expiry_time)
{
    if (key.empty())
    {
        throw_invalid_argument("put(): key must be non-empty");
    }
    if (!value_data)
    {
        throw_invalid_argument("put(): value must not be nullptr");
    }
    if (value_size < 0)
    {
        throw_invalid_argument("put(): invalid negative value size: " + to_string(value_size));
    }
    if (metadata_data && metadata_size < 0)
    {
        throw_invalid_argument("put(): invalid negative metadata size: " + to_string(metadata_size));
    }

    int64_t new_size = key.size() + value_size;
    if (metadata_data)
    {
        new_size += metadata_size;
    }
    if (new_size > stats_->max_cache_size_)
    {
        throw_logic_error(string("put(): cannot add ") + to_string(new_size) +
                          "-byte record to cache with maximum size of " + to_string(stats_->max_cache_size_));
    }

    auto etime = ticks(expiry_time);
    if (stats_->policy_ == CacheDiscardPolicy::lru_only && etime != epoch_ticks())
    {
        throw_logic_error(string("put(): policy is lru_only, but expiry_time (") + to_string(etime) +
                          ") is not infinite");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    auto atime = now_ticks();
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && etime != epoch_ticks() && etime <= atime)
    {
        return false;  // Already expired, so don't add it.
    }

    // The entry may or may not exist already.
    // Work out how many bytes of space we need.
    int64_t bytes_needed = new_size;

    string prefixed_key = k_data(key);
    bool found;
    auto old_data = get_data(prefixed_key, found);
    if (found)
    {
        bytes_needed = max(new_size - old_data.size, int64_t(0));  // new_size could be < old size
    }
    auto avail_bytes = stats_->max_cache_size_ - stats_->cache_size_;

    // Make room to add or replace the entry.
    if (bytes_needed > avail_bytes)
    {
        delete_at_least(bytes_needed - avail_bytes, key);  // Don't delete the entry about to be updated!
    }

    leveldb::WriteBatch batch;

    // Update the Data table.
    DataTuple new_meta(atime, etime, new_size);
    batch.Put(prefixed_key, new_meta.to_string());

    // Add or replace the entry in the Values table.
    prefixed_key[0] = VALUES_BEGIN[0];  // Avoid string copy.
    batch.Put(prefixed_key, leveldb::Slice(value_data, value_size));

    // Update metadata.
    prefixed_key[0] = METADATA_BEGIN[0];  // Avoid string copy.
    batch.Delete(prefixed_key);           // In case there was metadata previously.
    if (metadata_data)
    {
        batch.Put(prefixed_key, leveldb::Slice(metadata_data, metadata_size));
    }

    // Update the Atime index.
    string atime_key = k_atime_index(atime, key);
    if (found)
    {
        batch.Delete(k_atime_index(old_data.atime, key));
    }
    batch.Put(atime_key, to_string(new_size));

    // Update the Etime index.
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl)
    {
        if (found && old_data.etime != epoch_ticks())
        {
            batch.Delete(k_etime_index(old_data.etime, key));
        }
        // Etime index is not written to for non-expiring entries.
        if (etime != epoch_ticks())
        {
            batch.Put(k_etime_index(etime, key), to_string(new_size));
        }
    }

    // Write the batch.
    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "put()");

    // Update cache size and number of entries;
    stats_->cache_size_ = stats_->cache_size_ - old_data.size + new_size;
    stats_->hist_increment(new_size);
    if (!found)
    {
        ++stats_->num_entries_;
    }
    else
    {
        stats_->hist_decrement(old_data.size);
    }
    assert(stats_->num_entries_ >= 0);
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
    assert(stats_->cache_size_ >= 0);
    assert(stats_->cache_size_ <= stats_->max_cache_size_);
    assert(stats_->cache_size_ == 0 || stats_->num_entries_ != 0);
    assert(stats_->num_entries_ == 0 || stats_->cache_size_ != 0);

    call_handler(key, CacheEventIndex::put);

    return true;
}

bool PersistentStringCacheImpl::get_or_put(string const& key, string& value, PersistentStringCache::Loader load_func)
{
    return get_or_put(key, value, nullptr, load_func);
}

bool PersistentStringCacheImpl::get_or_put(string const& key,
                                           string& value,
                                           string* metadata,
                                           PersistentStringCache::Loader load_func)
{
    if (key.empty())
    {
        throw_invalid_argument("get_or_put(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    // Call the normal get() here, so the hit/miss counters and callbacks are correct.
    if (get(key, value, metadata))
    {
        return true;
    }

    try
    {
        load_func(key, *pimpl_);  // Expected to put the value.
    }
    catch (std::exception const& e)
    {
        throw runtime_error(make_message(string("get_or_put(): load_func exception: ") + e.what()));
    }
    catch (...)
    {
        throw runtime_error(make_message("get_or_put(): load_func: unknown exception"));
    }

    // We go for the raw DB here, to avoid counting an extra hit or miss.
    DataTuple dt;
    bool loaded = get_value_and_metadata(key, dt, value, metadata);
    return loaded;
}

bool PersistentStringCacheImpl::put_metadata(std::string const& key, std::string const& metadata)
{
    return put_metadata(key, metadata.data(), metadata.size());
}

bool PersistentStringCacheImpl::put_metadata(std::string const& key, const char* metadata, int64_t metadata_size)
{
    if (key.empty())
    {
        throw_invalid_argument("put_metadata(): key must be non-empty");
    }
    if (!metadata)
    {
        throw_invalid_argument("put_metadata(): metadata must not be nullptr");
    }
    if (metadata_size < 0)
    {
        throw_invalid_argument("put_metadata(): invalid negative size: " + to_string(metadata_size));
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    bool found;
    auto dt = get_data(data_key, found);
    if (!found)
    {
        return false;
    }

    int64_t old_meta_size = 0;
    IteratorUPtr it(db_->NewIterator(read_options));
    string metadata_key = k_metadata(key);
    it->Seek(metadata_key);
    if (it->Valid() && it->key().ToString() == metadata_key)
    {
        old_meta_size = it->value().size();
    }
    int64_t new_meta_size = metadata_size;
    if (dt.size - old_meta_size + new_meta_size > stats_->max_cache_size_)
    {
        throw_logic_error(string("put_metadata(): cannot add ") + to_string(new_meta_size) +
                          "-byte metadata: record size (" + to_string(dt.size - old_meta_size + new_meta_size) +
                          ") exceeds maximum cache size of " + to_string(stats_->max_cache_size_));
    }
    int64_t original_size = dt.size;
    dt.size = dt.size - old_meta_size + new_meta_size;

    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime <= now_ticks())
    {
        return false;  // Entry has expired.
    }

    // The new entry may be larger than the old one. If so, we may need to
    // evict some other entries to make room. However, we exclude this
    // record from trimming so we don't end up trimming the entry
    // that's about to be modified.
    if (new_meta_size > old_meta_size)
    {
        auto avail_bytes = stats_->max_cache_size_ - stats_->cache_size_;
        int64_t bytes_needed = new_meta_size - old_meta_size;
        if (bytes_needed > avail_bytes)
        {
            bytes_needed = min(bytes_needed, avail_bytes);
            delete_at_least(bytes_needed, key);  // Don't delete the entry about to be updated!
        }
    }

    leveldb::WriteBatch batch;

    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks())
    {
        it->Seek(k_etime_index(dt.etime, key));
        assert(it->Valid());
        assert(it->key().ToString() == k_etime_index(dt.etime, key));
        batch.Put(it->key(), to_string(dt.size));  // Update Etime index with new size (expiry time is not modified).
    }

    batch.Put(data_key, dt.to_string());                               // Update data.
    batch.Put(metadata_key, leveldb::Slice(metadata, metadata_size));  // Update metadata.

    it->Seek(k_atime_index(dt.atime, key));
    assert(it->Valid());
    assert(it->key().ToString() == k_atime_index(dt.atime, key));
    batch.Put(it->key(), to_string(dt.size));  // Update Atime index with new size (access time is not modified).

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "put_metadata(): batch write error");

    stats_->cache_size_ = stats_->cache_size_ - old_meta_size + new_meta_size;
    stats_->hist_increment(dt.size);
    stats_->hist_decrement(original_size);

    assert(stats_->num_entries_ >= 0);
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
    assert(stats_->cache_size_ >= 0);
    assert(stats_->cache_size_ <= stats_->max_cache_size_);
    assert(stats_->cache_size_ == 0 || stats_->num_entries_ != 0);
    assert(stats_->num_entries_ == 0 || stats_->cache_size_ != 0);

    return true;
}

bool PersistentStringCacheImpl::take(string const& key, string& value)
{
    return take(key, value, nullptr);
}

bool PersistentStringCacheImpl::take(string const& key, string& value, string* metadata)
{
    if (key.empty())
    {
        throw_invalid_argument("take(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    DataTuple dt;
    string val;
    bool found = get_value_and_metadata(key, dt, val, metadata);
    if (!found)
    {
        stats_->inc_misses();
        call_handler(key, CacheEventIndex::miss);
        return false;
    }

    // Delete the entry whether it expired or not. Seeing that we have just done
    // a lot of work finding it, we may as well finish the job.
    delete_entry(key, dt);

    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime <= now_ticks())
    {
        stats_->inc_misses();
        call_handler(key, CacheEventIndex::invalidate);
        call_handler(key, CacheEventIndex::miss);
        return false;  // Expired entries are hidden.
    }
    stats_->inc_hits();
    value = move(val);
    call_handler(key, CacheEventIndex::get);
    call_handler(key, CacheEventIndex::invalidate);
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
    return true;
}

bool PersistentStringCacheImpl::invalidate(string const& key)
{
    if (key.empty())
    {
        throw_invalid_argument("invalidate(): key must be non-empty");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string prefixed_key = k_data(key);
    bool found;
    auto dt = get_data(prefixed_key, found);
    if (!found)
    {
        return false;
    }

    // Delete the entry whether it expired or not. Seeing that we have just done
    // a lot of work finding it, we may as well finish the job.
    delete_entry(key, dt);

    call_handler(key, CacheEventIndex::invalidate);
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && dt.etime != epoch_ticks() && dt.etime < now_ticks())
    {
        return false;  // Expired entries are hidden.
    }
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
    return true;
}

void PersistentStringCacheImpl::invalidate(vector<string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template<typename It>
void PersistentStringCacheImpl::invalidate(It begin, It end)
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    leveldb::WriteBatch batch;

    for (auto&& it = begin; it < end; ++it)
    {
        if (it->empty())
        {
            continue;
        }
        bool found;
        auto dt = get_data(k_data(*it), found);
        if (!found)
        {
            continue;
        }
        batch_delete(*it, dt, batch);

        // Update cache size and entries.
        stats_->hist_decrement(dt.size);
        stats_->cache_size_ -= dt.size;
        assert(stats_->cache_size_ >= 0);
        assert(stats_->cache_size_ <= stats_->max_cache_size_);
        --stats_->num_entries_;
        assert(stats_->num_entries_ >= 0);
        assert(stats_->num_entries_ == hist_sum(stats_->hist_));
        assert(stats_->cache_size_ == 0 || stats_->num_entries_ != 0);
        assert(stats_->num_entries_ == 0 || stats_->cache_size_ != 0);

        call_handler(*it, CacheEventIndex::invalidate);
    }

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "invalidate(): batch write error");
}

void PersistentStringCacheImpl::invalidate(initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

void PersistentStringCacheImpl::invalidate()
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    {
        int64_t count = 0;

        leveldb::WriteBatch batch;
        int64_t const batch_size = 1000;

        PersistentStringCache::EventCallback cb =
            handlers_[static_cast<underlying_type<CacheEventIndex>::type>(CacheEventIndex::invalidate)];

        IteratorUPtr it(db_->NewIterator(read_options));
        it->Seek(ALL_BEGIN);
        leveldb::Slice const atime_prefix = ATIME_BEGIN;
        leveldb::Slice const all_end = ALL_END;
        while (it->Valid() && it->key().compare(all_end) < 0)
        {
            auto key = it->key();
            batch.Delete(key);
            if (cb && key.starts_with(atime_prefix))
            {
                TimeKeyTuple atk(key.ToString().substr(1));
                --stats_->num_entries_;
                auto size = stoll(it->value().ToString());
                stats_->cache_size_ -= size;
                call_handler(atk.key, CacheEventIndex::invalidate);
            }
            if (++count == batch_size)
            {
                auto s = db_->Write(write_options, &batch);
                throw_if_error(s, "invalidate(): batch write error");
                batch.Clear();
                count = 0;
            }
            it->Next();
        }
        throw_if_error(it->status(), "invalidate(): iterator error");

        if (count != 0)
        {
            auto s = db_->Write(write_options, &batch);
            throw_if_error(s, "invalidate(): final batch write error");
        }
    }  // Close batch

    stats_->num_entries_ = 0;
    stats_->hist_clear();
    stats_->cache_size_ = 0;
}

bool PersistentStringCacheImpl::touch(string const& key, chrono::time_point<chrono::system_clock> expiry_time)
{
    if (key.empty())
    {
        throw_invalid_argument("touch(): key must be non-empty");
    }

    int64_t new_etime = ticks(expiry_time);

    if (stats_->policy_ == CacheDiscardPolicy::lru_only && new_etime != epoch_ticks())
    {
        throw_logic_error(string("touch(): policy is lru_only, but expiry_time (") + to_string(new_etime) +
                          ") is not infinite");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    string data_key = k_data(key);
    bool found;
    auto dt = get_data(data_key, found);
    if (!found)
    {
        return false;
    }

    int64_t now = now_ticks();
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl && new_etime != epoch_ticks() && new_etime <= now)
    {
        return false;  // New expiry time is already older than the time now.
    }

    leveldb::WriteBatch batch;

    string size = to_string(dt.size);
    batch.Delete(k_atime_index(dt.atime, key));  // Delete old Atime index entry.
    batch.Put(k_atime_index(now, key), size);    // Write new Atime index entry.

    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl)
    {
        batch.Delete(k_etime_index(dt.etime, key));  // Delete old Etime index entry.
        if (new_etime != epoch_ticks())
        {
            batch.Put(k_etime_index(new_etime, key), size);  // Write new Etime index entry.
        }
    }
    dt.atime = now;
    dt.etime = new_etime;
    batch.Put(data_key, dt.to_string());  // Write new data.

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "touch(): batch write error");

    call_handler(key, CacheEventIndex::touch);

    return true;
}

void PersistentStringCacheImpl::clear_stats() noexcept
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    stats_->clear();
    write_stats();
}

void PersistentStringCacheImpl::resize(int64_t size_in_bytes)
{
    if (size_in_bytes < 1)
    {
        throw_invalid_argument("resize(): invalid size_in_bytes (" + to_string(size_in_bytes) + "): value must be > 0");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    if (size_in_bytes < stats_->max_cache_size_)
    {
        trim_to(size_in_bytes);
        db_->CompactRange(nullptr, nullptr);  // Avoid bulk deletions slowing down subsequent accesses.
    }

    auto s = db_->Put(write_options, SETTINGS_MAX_SIZE, to_string(size_in_bytes));
    throw_if_error(s, "resize(): cannot write max size");
    stats_->max_cache_size_ = size_in_bytes;
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
}

void PersistentStringCacheImpl::trim_to(int64_t used_size_in_bytes)
{
    if (used_size_in_bytes < 0)
    {
        throw_invalid_argument("trim_to(): invalid used_size_in_bytes (" + to_string(used_size_in_bytes) +
                               "): value must be >= 0");
    }
    if (used_size_in_bytes > stats_->max_cache_size_)
    {
        throw_logic_error(string("trim_to(): invalid used_size_in_bytes (") + to_string(used_size_in_bytes) +
                          "): value must be <= max_size_in_bytes (" + to_string(stats_->max_cache_size_) + ")");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    if (used_size_in_bytes < stats_->cache_size_)
    {
        delete_at_least(stats_->cache_size_ - used_size_in_bytes);
    }
    assert(stats_->num_entries_ == hist_sum(stats_->hist_));
}

void PersistentStringCacheImpl::compact()
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    db_->CompactRange(nullptr, nullptr);
}

void PersistentStringCacheImpl::set_handler(CacheEvent events, PersistentStringCache::EventCallback cb)
{
    static constexpr auto limit = underlying_type<CacheEvent>::type(CacheEvent::END_);

    auto evs = underlying_type<CacheEvent>::type(events);
    if (evs == 0 || evs > limit - 1)
    {
        throw_invalid_argument("set_handler(): invalid events (" + to_string(evs) +
                               "): value must be in the range [1.." + to_string(limit - 1) + "]");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    static constexpr auto index_limit = underlying_type<CacheEventIndex>::type(CacheEventIndex::END_);
    for (underlying_type<CacheEventIndex>::type i = 0; i < index_limit; ++i)
    {
        if ((evs >> i) & 1)
        {
            handlers_[i] = cb;
        }
    }
}

void PersistentStringCacheImpl::init_db(leveldb::Options options)
{
#ifndef NDEBUG
    options.paranoid_checks = true;
    read_options.verify_checksums = true;
#endif

    leveldb::DB* db;
    auto s = leveldb::DB::Open(options, stats_->cache_path_, &db);
    throw_if_error(s, "cannot open or create cache");
    db_.reset(db);
}

bool PersistentStringCacheImpl::cache_is_new() const
{
    string val;
    auto s = db_->Get(read_options, SETTINGS_SCHEMA_VERSION, &val);
    throw_if_error(s, "cannot read schema version");
    return s.IsNotFound();
}

void PersistentStringCacheImpl::write_version()
{
    auto s = db_->Put(write_options, SETTINGS_SCHEMA_VERSION, to_string(SCHEMA_VERSION));
    throw_if_error(s, "cannot read schema version");
}

// Check if the version of the DB matches the expected version.
// Pre: Version exists in the DB.
// If the version can be read and make sense as a number, but
// differs from the expected version, wipe the data (but
// not the settings).
// If the version can be read, but doesn't parse as a number, throw.
// If the version matches the expected version, do nothing.

void PersistentStringCacheImpl::check_version()
{
    // Check schema version.
    string val = "not found";
    auto s = db_->Get(read_options, SETTINGS_SCHEMA_VERSION, &val);
    throw_if_error(s, "cannot read schema version");
    assert(!s.IsNotFound());

    int old_version;
    try
    {
        old_version = stoi(val);
    }
    catch (std::exception const&)
    {
        throw_corrupt_error("check_version(): bad version: \"" + val + "\"");
    }
    if (old_version != SCHEMA_VERSION)
    {
        // Wipe all tables and stats (but not settings).
        leveldb::WriteBatch batch;
        IteratorUPtr it(db_->NewIterator(read_options));

        it->Seek(ALL_BEGIN);
        leveldb::Slice const all_end(ALL_END);
        while (it->Valid() && it->key().compare(ALL_END) < 0)
        {
            batch.Delete(it->key());
            it->Next();
        }

        // Note: any migration of settings for newer versions should happen here.

        // Write new schema version.
        batch.Put(SETTINGS_SCHEMA_VERSION, to_string(SCHEMA_VERSION));
        s = db_->Write(write_options, &batch);
        throw_if_error(s, string("cannot clear DB after version mismatch, old version = ") + to_string(old_version) +
                              ", new version = " + to_string(SCHEMA_VERSION));

        stats_->num_entries_ = 0;
        stats_->hist_clear();
        stats_->cache_size_ = 0;
    }
}

void PersistentStringCacheImpl::read_settings()
{
    // Note: Loose error checking here. If someone deliberately
    //       corrupts the settings table by writing garbage values for valid
    //       keys, that's just too bad. We can't protect against Machiavelli.

    string val;

    auto s = db_->Get(read_options, SETTINGS_MAX_SIZE, &val);
    throw_if_error(s, "read_settings(): cannot read max size");
    stats_->max_cache_size_ = stoll(val);

    s = db_->Get(read_options, SETTINGS_POLICY, &val);
    throw_if_error(s, "read_settings(): cannot read policy");
    stats_->policy_ = static_cast<CacheDiscardPolicy>(stoi(val));
}

void PersistentStringCacheImpl::write_settings()
{
    leveldb::WriteBatch batch;

    batch.Put(SETTINGS_MAX_SIZE, to_string(stats_->max_cache_size_));
    batch.Put(SETTINGS_POLICY, to_string(static_cast<int>(stats_->policy_)));

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "write_settings()");
}

void PersistentStringCacheImpl::read_stats()
{
    string val;
    auto s = db_->Get(read_options, STATS_VALUES, &val);
    throw_if_error(s, "read_stats()");
    stats_->deserialize(val);
}

void PersistentStringCacheImpl::write_stats()
{
    auto s = db_->Put(write_options, STATS_VALUES, stats_->serialize());
    throw_if_error(s, "write_stats()");
}

bool PersistentStringCacheImpl::read_dirty_flag() const
{
    string dirty;
    auto s = db_->Get(read_options, DIRTY_FLAG, &dirty);
    if (s.IsNotFound())
    {
        return true;
    }
    return dirty != "0";
}

void PersistentStringCacheImpl::write_dirty_flag(bool is_dirty)
{
    auto s = db_->Put(write_options, DIRTY_FLAG, is_dirty ? "1" : "0");
    throw_if_error(s, "write_dirty_flag()");
}

PersistentStringCacheImpl::DataTuple PersistentStringCacheImpl::get_data(string const& key, bool& found) const
{
    // mutex_ must be locked here!

    assert(key[0] == DATA_BEGIN[0]);

    string val;
    auto s = db_->Get(read_options, key, &val);
    throw_if_error(s, "get_data(): cannot read data");
    if (!s.IsNotFound())
    {
        found = true;
        return DataTuple(val);
    }
    found = false;
    return DataTuple();
}

bool PersistentStringCacheImpl::get_value_and_metadata(string const& key,
                                                       DataTuple& data,
                                                       string& value,
                                                       string* metadata) const
{
    // mutex_ must be locked here!

    // Note: key is the un-prefixed key!
    string prefixed_key = k_data(key);

    IteratorUPtr it(db_->NewIterator(read_options));
    it->Seek(prefixed_key);
    throw_if_error(it->status(), "get_value_and_metadata(): iterator error");
    assert(it->Valid());
    if (it->key() != leveldb::Slice(prefixed_key))
    {
        return false;
    }

    data = DataTuple(it->value().ToString());
    prefixed_key[0] = VALUES_BEGIN[0];  // Avoid string copy.
    it->Seek(prefixed_key);
    assert(it->Valid() && it->key().compare(prefixed_key) == 0);
    value = it->value().ToString();
    if (metadata)
    {
        prefixed_key[0] = METADATA_BEGIN[0];  // Avoid string copy.
        it->Seek(prefixed_key);
        if (it->key().compare(prefixed_key) == 0)
        {
            *metadata = it->value().ToString();
        }
        else
        {
            metadata->clear();  // Metadata may have been there previously, but isn't now.
        }
    }
    return true;
}

void PersistentStringCacheImpl::batch_delete(string const& key, DataTuple const& data, leveldb::WriteBatch& batch)
{
    // mutex_ must be locked here!

    string prefixed_key = k_data(key);
    batch.Delete(prefixed_key);                    // Delete data.
    prefixed_key[0] = VALUES_BEGIN[0];             // Avoid string copy.
    batch.Delete(prefixed_key);                    // Delete value.
    prefixed_key[0] = METADATA_BEGIN[0];           // Avoid string copy.
    batch.Delete(prefixed_key);                    // Delete metadata
    batch.Delete(k_atime_index(data.atime, key));  // Delete atime index
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl)
    {
        string etime_key = k_etime_index(data.etime, key);
        batch.Delete(etime_key);
    }
}

void PersistentStringCacheImpl::delete_entry(string const& key, DataTuple const& data)
{
    // mutex_ must be locked here!

    leveldb::WriteBatch batch;
    batch_delete(key, data, batch);
    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "delete_entry()");

    // Update cache size and entries.
    stats_->hist_decrement(data.size);
    stats_->cache_size_ -= data.size;
    assert(stats_->cache_size_ >= 0);
    assert(stats_->cache_size_ <= stats_->max_cache_size_);
    --stats_->num_entries_;
    assert(stats_->num_entries_ >= 0);
    assert(stats_->cache_size_ == 0 || stats_->num_entries_ != 0);
    assert(stats_->num_entries_ == 0 || stats_->cache_size_ != 0);
}

void PersistentStringCacheImpl::delete_at_least(int64_t bytes_needed, string const& skip_key)
{
    // mutex_ must be locked here!

    assert(bytes_needed > 0);
    assert(bytes_needed <= stats_->cache_size_);

    int64_t deleted_bytes = 0;
    int64_t deleted_entries = 0;

    leveldb::WriteBatch batch;

    // Step 1: Delete all expired entries.
    if (stats_->policy_ == CacheDiscardPolicy::lru_ttl)
    {
        auto now_time = now_ticks();
        IteratorUPtr it(db_->NewIterator(read_options));
        leveldb::Slice const etime_prefix(ETIME_BEGIN);
        it->Seek(etime_prefix);
        while (it->Valid())
        {
            if (!it->key().starts_with(etime_prefix))
            {
                break;
            }
            string etime_key = it->key().ToString();
            TimeKeyTuple ek(etime_key.substr(1));  // Strip prefix to create the etime/key tuple.
            if (!skip_key.empty() && ek.key == skip_key)
            {
                // Too hard to hit with a test because the entry must expire
                // in between put_metadata() having decided that it's still
                // unexpired and now_time taken above.
                // LCOV_EXCL_START
                it->Next();
                continue;  // This entry must not be deleted (see put_metadata()).
                // LCOV_EXCL_STOP
            }
            if (ek.time > now_time)
            {
                break;  // Anything past this point has not expired yet.
            }

            string prefixed_key = k_data(ek.key);
            string val;
            auto s = db_->Get(read_options, prefixed_key, &val);
            throw_if_error(s, "delete_at_least: cannot read data");
            DataTuple dt(move(val));

            int64_t size = stoll(it->value().ToString());
            deleted_bytes += size;
            bytes_needed -= size;
            ++deleted_entries;
            batch_delete(ek.key, dt, batch);

            --stats_->num_entries_;
            ++stats_->ttl_evictions_;
            stats_->hist_decrement(size);
            stats_->cache_size_ -= size;
            call_handler(ek.key, CacheEventIndex::evict_ttl);

            it->Next();
        }
        throw_if_error(it->status(), "delete_at_least(): expiry iterator error");
    }  // Close iterator.

    if (deleted_entries)
    {
        // Need to commit the batch here, otherwise what follows will not see the changes made above.
        auto s = db_->Write(write_options, &batch);
        throw_if_error(s, "delete_at_least(): expiry write error");
        batch.Clear();
    }

    // Step 2: If we still need more room, delete entries in LRU order until we have enough room.
    if (bytes_needed > 0)
    {
        // Run over the Atime index and delete in old-to-new order.
        IteratorUPtr it(db_->NewIterator(read_options));
        leveldb::Slice const atime_prefix(ATIME_BEGIN);
        it->Seek(atime_prefix);
        while (it->Valid() && bytes_needed > 0 && it->key().starts_with(atime_prefix))
        {
            TimeKeyTuple atk(it->key().ToString().substr(1));  // Strip prefix to create the atime/key tuple.
            if (!skip_key.empty() && atk.key == skip_key)
            {
                it->Next();
                continue;  // This entry must not be deleted (see put_metadata()).
            }

            int64_t size = stoll(it->value().ToString());
            deleted_bytes += size;
            bytes_needed -= size;
            ++deleted_entries;

            string data_string;
            string prefixed_key = k_data(atk.key);
            auto s = db_->Get(read_options, prefixed_key, &data_string);
            assert(!s.IsNotFound());
            throw_if_error(s, "delete_at_least()");
            DataTuple dt(move(data_string));
            batch_delete(atk.key, dt, batch);

            --stats_->num_entries_;
            ++stats_->lru_evictions_;
            stats_->hist_decrement(size);
            stats_->cache_size_ -= size;
            call_handler(atk.key, CacheEventIndex::evict_lru);

            it->Next();
        }
        throw_if_error(it->status(), "delete_at_least(): LRU iterator error");
        assert(deleted_bytes > 0);
        assert(bytes_needed <= 0);
    }

    auto s = db_->Write(write_options, &batch);
    throw_if_error(s, "delete_at_least(): LRU write error");

    assert(stats_->cache_size_ >= 0);
    assert(stats_->num_entries_ >= 0);
    assert(stats_->cache_size_ == 0 || stats_->num_entries_ != 0);
    assert(stats_->num_entries_ == 0 || stats_->cache_size_ != 0);
}

void PersistentStringCacheImpl::call_handler(string const& key, CacheEventIndex event_index) const
{
    // mutex_ must be locked here!

    typedef underlying_type<CacheEventIndex>::type IndexType;
    auto handler = handlers_[static_cast<IndexType>(event_index)];
    if (handler)
    {
        try
        {
            IndexType index = static_cast<IndexType>(event_index);
            handler(key, static_cast<CacheEvent>(1 << index), stats_);
        }
        catch (...)
        {
            // Ignored
        }
    }
}

string PersistentStringCacheImpl::make_message(leveldb::Status const& s, string const& msg) const
{
    return class_name + ": " + msg + ": " + s.ToString() + " (cache_path: " + stats_->cache_path_ + ")";
}

string PersistentStringCacheImpl::make_message(string const& msg) const
{
    return class_name + ": " + msg + " (cache_path: " + stats_->cache_path_ + ")";
}

void PersistentStringCacheImpl::throw_if_error(leveldb::Status const& s, string const& msg) const
{
    if (!s.ok() && !s.IsNotFound())
    {
        if (s.IsCorruption())
        {
            throw system_error(666, generic_category(), make_message(s, msg));  // LCOV_EXCL_LINE
        }
        throw runtime_error(make_message(s, msg));
    }
}

void PersistentStringCacheImpl::throw_logic_error(string const& msg) const
{
    throw logic_error(make_message(msg));
}

void PersistentStringCacheImpl::throw_invalid_argument(string const& msg) const
{
    throw invalid_argument(make_message(msg));
}

void PersistentStringCacheImpl::throw_corrupt_error(string const& msg) const
{
    throw system_error(666, generic_category(), make_message(msg));
}

}  // namespace internal

}  // namespace core
