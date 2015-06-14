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

#pragma once

#include <core/internal/cache_event_indexes.h>
#include <core/persistent_string_cache.h>

#include <leveldb/db.h>

#include <mutex>
#include <sstream>

namespace core
{

namespace internal
{

class PersistentStringCacheStats;

class PersistentStringCacheImpl
{
public:
    PersistentStringCacheImpl(std::string const& cache_path,
                              int64_t max_size_in_bytes,
                              core::CacheDiscardPolicy policy,
                              PersistentStringCache* pimpl = nullptr);
    PersistentStringCacheImpl(std::string const& cache_path, PersistentStringCache* pimpl = nullptr);

    PersistentStringCacheImpl(PersistentStringCacheImpl const&) = delete;
    PersistentStringCacheImpl& operator=(PersistentStringCacheImpl const&) = delete;

    PersistentStringCacheImpl(PersistentStringCacheImpl&&) = delete;
    PersistentStringCacheImpl& operator=(PersistentStringCacheImpl&&) = delete;

    ~PersistentStringCacheImpl();

    bool get(std::string const& key, std::string& value) const;
    bool get(std::string const& key, std::string& value, std::string* metadata) const;
    bool get_metadata(std::string const& key, std::string& metadata) const;
    bool contains_key(std::string const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    core::PersistentCacheStats stats() const;

    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             std::string const& value,
             std::string const* metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value_data,
             int64_t value_size,
             char const* metadata_data,
             int64_t metadata_size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool get_or_put(std::string const& key, std::string& value, PersistentStringCache::Loader load_func);
    bool get_or_put(std::string const& key,
                    std::string& value,
                    std::string* metadata,
                    PersistentStringCache::Loader load_func);
    bool put_metadata(std::string const& key, std::string const& metadata);
    bool put_metadata(std::string const& key, char const* metadata, int64_t metadata_size);
    bool take(std::string const& key, std::string& value);
    bool take(std::string const& key, std::string& value, std::string* metadata);
    bool invalidate(std::string const& key);
    void invalidate(std::vector<std::string> const& keys);
    template<typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<std::string> const& keys);
    void invalidate();
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats() noexcept;
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();
    void set_handler(CacheEvent events, PersistentStringCache::EventCallback cb);

private:
    // Simple struct to serialize/deserialize a data tuple.
    // For the stringified representation, fields are separated
    // by a space.

    struct DataTuple
    {
        int64_t atime;  // Last access time, msec since the epoch
        int64_t etime;  // Expiry time, msec since the epoch
        int64_t size;   // Size in bytes

        DataTuple(int64_t at, int64_t et, int64_t s) noexcept
            : atime(at)
            , etime(et)
            , size(s)
        {
        }

        DataTuple() noexcept
            : DataTuple(0, 0, 0)
        {
        }

        DataTuple(std::string const& s) noexcept
        {
            std::istringstream is(s);
            is >> atime >> etime >> size;
            assert(!is.bad());
        }

        DataTuple(DataTuple const&) = default;
        DataTuple(DataTuple&&) = default;

        DataTuple& operator=(DataTuple const&) = default;
        DataTuple& operator=(DataTuple&&) = default;

        std::string to_string() const
        {
            std::ostringstream os;
            os << atime << " " << etime << " " << size;
            return os.str();
        }
    };

    void init_stats();
    void init_db(leveldb::Options options);
    bool cache_is_new() const;
    void write_version();
    void check_version();
    void read_settings();
    void write_settings();
    void read_stats();
    void write_stats();
    bool read_dirty_flag() const;
    void write_dirty_flag(bool is_dirty);
    DataTuple get_data(std::string const& key, bool& found) const;
    bool get_value_and_metadata(std::string const& key,
                                DataTuple& data,
                                std::string& value,
                                std::string* metadata) const;
    void batch_delete(std::string const& key, DataTuple const& data, leveldb::WriteBatch& batch);
    void delete_entry(std::string const& key, DataTuple const& data);
    void delete_at_least(int64_t bytes_needed, std::string const& skip_key = "");
    void call_handler(std::string const& key, core::internal::CacheEventIndex event) const;

    std::string make_message(leveldb::Status const& s, std::string const& msg) const;
    std::string make_message(std::string const& msg) const;
    void throw_if_error(leveldb::Status const& s, std::string const& msg) const;
    void throw_logic_error(std::string const& msg) const;
    void throw_invalid_argument(std::string const& msg) const;
    void throw_corrupt_error(std::string const& msg) const;

    PersistentStringCache* pimpl_;                 // Back-pointer to owning pimpl.
    std::unique_ptr<leveldb::Cache> block_cache_;  // Must be defined *before* db_!
    std::unique_ptr<leveldb::DB> db_;
    std::shared_ptr<PersistentStringCacheStats> stats_;

    std::array<PersistentStringCache::EventCallback, static_cast<unsigned>(CacheEventIndex::END_)>
        handlers_;

    mutable std::recursive_mutex mutex_;
};

}  // namespace internal

}  // namespace core
