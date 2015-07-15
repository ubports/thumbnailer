/*
 * Copyright (C) 2015 Canonical Ltd.
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

#include <core/cache_discard_policy.h>
#include <core/cache_events.h>
#include <core/optional.h>
#include <core/persistent_cache_stats.h>

namespace core
{

namespace internal
{

class PersistentStringCacheImpl;

}  // namespace internal

/**
\brief A cache of key-value pairs with persistent storage.

PersistentStringCache provides a cache of key-value pairs with a backing store.
It is intended for caching arbitrary (possibly large) amounts of data,
such as might be needed by a web browser cache. The cache scales to large
numbers (hundreds of thousands) of entries and is very fast. (Typically,
the performance-limiting factor is the I/O bandwidth to disk.)

The cache is robust in the face of crashes and power loss. After a
re-start, it is guaranteed to be in a consistent state with correct
data. Some number of updates that were made just prior to a power loss
or kernel crash can be lost; however, if just the calling process
crashes, no updates are lost.

A cache has a maximum size (which can be changed at any time). Once
the cache reaches its maximum size, when adding an entry,
the cache automatically discards enough entries to make room for the new entry.

Keys can be (possibly binary) strings of size &gt; 0. Values can be
(possibly binary) strings including the empty string.

Entries maintain an access time, which is used to keep them in
least-recently-used (LRU) order. In addition, entries can have
an optional expiry time. (If no expiry time is specified, infinite
expiry time is assumed.)

\note This class is thread-safe; you can call member functions from
different threads without any synchronization. Thread-safety is
provided for convenience, not performance. Calling concurrently
into the cache from multiple threads will not yield improved performance.

### Discard Policy

The cache provides two different discard policies, `lru_ttl`
and `lru_only`.

For `lru_ttl`, the discard policy of the cache is to first delete all
entries that have expired. If this does not free sufficient space
to make room for a new entry, the cache then deletes entries in oldest
to newest (LRU) order until sufficient space is available. This
deletion in LRU order may delete entries that have an
expiry time, but have not expired yet, as well as entries with
infinite expiry time.

For `lru_only`, entries do not maintain an expiry time and
are therefore discarded strictly in LRU order.

Access and expiry times are recorded with millisecond granularity.
To indicate infinite expiry time, use the defaulted parameter value or
`chrono::system_clock::time_point()`.

### Error reporting

Methods throw `std::runtime_error` if the underlying database
(leveldb) reports an error. If leveldb detects database corruption, the code
throws `std::system_error` with with a 666 error code. To recover
from this error, remove all files in the cache directory.
Other errors are indicated by throwing `std::logic_error` or
`std::invalid_argument` as appropriate.

### Additional data

Besides storing key-value pairs, the cache allows you to add arbitrary
extra data to each entry. This is useful, for example, to maintain
metadata (such as HTTP header details) for the entries in the cache.

\warning It is not possible to distinguish between "no metadata was ever added"
and "empty metadata was added and retrieved". Do not use the metadata in such
a way that you rely the difference between "metadata not there" and
"metadata is the empty string".

### Performance

Some rough performance figures, taken on an Intel Ivy Bridge i7-3770K 3.5 GHz
with 16 GB RAM, appear below. Records are filled with random data to make them non-compressible.

After filling the cache, the code performs cache lookups using random
keys, with an 80% hit probability. On a miss, it inserts a new
random record. This measures the typical steady-state behavior:
whenever a cache miss happens, the caller fetches the data and
inserts a new record into the cache.

Setting     | Value
----------  | ------
Cache size  | 100 MB
# Records   | ~5120
Record size | 20 kB, normal distribution, stddev = 7000

Running the test With a 7200 rpm spinning disk produces:

Parameter    | Value
-------------|------------
Reads        | 30.9 MB/sec
Writes       | 7.0 MB/sec
Records/sec  | 1995

Running the test With an Intel 256 GB SSD produces:

Parameter    | Value
-------------|------------
Reads        | 112.6 MB/sec
Writes       | 25.7 MB/sec
Records/sec  | 7112

\note When benchmarking, make sure to compile in release mode. In debug
mode, a number of expensive assertions are turned on.

\note Also be aware that leveldb uses Snappy compression beneath the
covers. This means that, if test data is simply filled with
a fixed byte pattern, you will measure artificially high performance.
*/

class PersistentStringCache
{
public:
    /**
    Convenience typedef for the return type of open().
    */
    typedef std::unique_ptr<PersistentStringCache> UPtr;

    /**
    \brief Simple pair of value and metadata.
    */
    struct Data
    {
        /**
        \brief Stores the value of an entry.
        */
        std::string value;

        /**
        \brief Stores the metadata of an entry. If no metadata exists for an entry,
        `metadata` is returned as the empty string when it is retrieved.
        */
        std::string metadata;
    };

    /** @name Copy and Assignment
    Cache instances are not copyable, but can be moved.
    \note The constructors are private. Use one of the open()
    static member functions to create or open a cache.
    */

    //{@
    PersistentStringCache(PersistentStringCache const&) = delete;
    PersistentStringCache& operator=(PersistentStringCache const&) = delete;

    PersistentStringCache(PersistentStringCache&&);
    PersistentStringCache& operator=(PersistentStringCache&&);
    //@}

    /**
    Destroys the instance.
    */
    ~PersistentStringCache();

    /** @name Creation Methods
    */

    //{@

    /**
    \brief Creates or opens a PersistentStringCache.

    If no cache exists on disk, it will be created;
    otherwise, the pre-existing cache contents are used.

    An existing cache can be opened only if `max_size_in_bytes` and `policy` have
    the same values they had when the cache was last closed.

    \param cache_path The path to a directory in which to store the cache. The contents
    of this directory are exlusively owned by the cache; do not create additional files
    or directories there. The directory need not exist when creating a new cache.
    \param max_size_in_bytes The maximum size in bytes for the cache.
    \param policy The discard policy for the cache (`lru_only` or `lru_ttl`). The discard
    policy cannot be changed once a cache has been created.

    The size of an entry is the sum of the sizes of its key, value, and metadata.
    The maximum size of the cache is the sum of the sizes of all its entries.

    \return A <code>unique_ptr</code> to the instance.
    \throws invalid_argument `max_size_in_bytes` is &lt; 1.
    \throws logic_error `max_size_in_bytes` or `policy` do not match the settings of a pre-existing cache.
    */
    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);

    /**
    \brief Opens an existing PersistentStringCache.
    \param cache_path The path to a directory containing the existing cache.
    \return A <code>unique_ptr</code> to the instance.
    */
    static UPtr open(std::string const& cache_path);

    //@}

    /** @name Accessors
    */

    //{@

    /**
    \brief Returns the value of an entry in the cache, provided the entry has not expired.
    \param key The key for the entry.
    \return A null value if the entry could not be retrieved; the value of the entry, otherwise.
    \throws invalid_argument `key` is the empty string.
    \note This operation updates the access time of the entry.
    */
    Optional<std::string> get(std::string const& key) const;

    /**
    \brief Returns the data for an entry in the cache, provided the entry has not expired.
    \param key The key for the entry.
    \return A null value if the entry could not be retrieved; the data of the entry, otherwise.
    If no metadata exists, `Data::metadata` is set to the empty string.
    \throws invalid_argument `key` is the empty string.
    \note This operation updates the access time of the entry.
    */
    Optional<Data> get_data(std::string const& key) const;

    /**
    \brief Returns the metadata for an entry in the cache, provided the entry has not expired.
    \param key The key for the entry.
    \return A null value if the entry could not be retrieved; the metadata of the entry, otherwise.
    \throws invalid_argument `key` is the empty string.
    \note This operation does _not_ update the access time of the entry.
    \see touch()
    */
    Optional<std::string> get_metadata(std::string const& key) const;

    /**
    \brief Tests if an (unexpired) entry is in the cache.
    \param key The key for the entry.
    \return `true` if the entry is in the cache; `false` otherwise.
    \throws invalid_argument `key` is the empty string.
    \note This operation does _not_ update the access time of the entry.
    \see touch()
    */
    bool contains_key(std::string const& key) const;

    /**
    \brief Returns the number of entries in the cache.
    \return The total number of entries in the cache.
    \note The returned count includes possibly expired entries.
    */
    int64_t size() const noexcept;

    /**
    \brief Returns the number of bytes consumed by entries in the cache.
    \return The total number of bytes in the cache.
    \note The returned size includes possibly expired entries.
    */
    int64_t size_in_bytes() const noexcept;

    /**
    \brief Returns the maximum size of the cache in bytes.
    \return The maximum number of bytes that can be stored in the cache.
    \see resize()
    */
    int64_t max_size_in_bytes() const noexcept;

    /**
    \brief Returns an estimate of the disk space consumed by the cache.
    \return The approximate number of bytes used by the cache on disk.
    \note The returned size may be smaller than the eventual size
    if there are updates to the cache that have not yet been written to disk.
    */
    int64_t disk_size_in_bytes() const;

    /**
    \brief Returns the discard policy of the cache.
    \return The discard policy (`lru_only` or `lru_ttl`).
    */
    CacheDiscardPolicy discard_policy() const noexcept;

    /**
    \brief Returns statistics for the cache.

    The returned statistics are persistent and are restored the next
    time an existing cache is opened. Call clear_stats() to explicitly
    reset the statistics counters and time stamps to zero.
    \return An object that provides accessors to statistics and settings.
    \see clear_stats()
    */
    PersistentCacheStats stats() const;

    //@}

    /** @name Modifiers
    */

    //{@

    /**
    \brief Adds or updates an entry.

    If an entry with the given key does not exist in the cache, it is added
    (possibly evicting a number of expired and/or older entries). If the entry
    still exists (whether expired or not), it is updated with the new value
    (and possibly expiry time).

    This operation deletes any metadata associated with the entry.

    \return `true` if the entry was added or updated. `false` if the policy
    is `lru_ttl` and `expiry_time` is in the past.

    \throws invalid_argument `key` is the empty string.
    \throws logic_error The size of the entry exceeds the maximum cache size.
    \throws logic_error The cache policy is `lru_only` and a non-infinite expiry time was provided.
    */
    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Adds or updates an entry.

    \note This overload is provided to avoid the need to construct a string value.

    If an entry with the given key does not exist in the cache, it is added
    (possibly evicting a number of expired and/or older entries). If the entry
    still exists (whether expired or not), it is updated with the new value
    (and possibly expiry time).

    This operation deletes any metadata associated with the entry.

    \return `true` if the entry was added or updated. `false` if the policy
    is `lru_ttl` and `expiry_time` is in the past.

    \param key The key of the entry.
    \param value A pointer to the first byte of the value.
    \param size The size of the value in bytes.
    \param expiry_time The time at which the entry expires.

    \throws invalid_argument `key` is the empty string.
    \throws invalid_argument `value` is `nullptr`.
    \throws invalid_argument `size` is negative.
    \throws logic_error The size of the entry exceeds the maximum cache size.
    \throws logic_error The cache policy is `lru_only` and a non-infinite expiry time was provided.
    */
    bool put(std::string const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Adds or updates an entry and its metadata.

    If an entry with the given key does not exist in the cache, it is added
    (possibly evicting a number of expired and/or older entries). If the entry
    still exists (whether expired or not), it is updated with the new value
    and metadata (and possibly expiry time).

    \return `true` if the entry was added or updated. `false` if the policy
    is `lru_ttl` and `expiry_time` is in the past.

    \throws invalid_argument `key` is the empty string.
    \throws logic_error The sum of sizes of the entry and metadata exceeds the maximum cache size.
    \throws logic_error The cache policy is `lru_only` and a non-infinite expiry time was provided.
    */
    bool put(std::string const& key,
             std::string const& value,
             std::string const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Adds or updates an entry and its metadata.

    \note This overload is provided to avoid the need to construct strings
    for the value and metadata.

    If an entry with the given key does not exist in the cache, it is added
    (possibly evicting a number of expired and/or older entries). If the entry
    still exists (whether expired or not), it is updated with the new value
    and metadata (and possibly expiry time).

    \return `true` if the entry was added or updated. `false` if the policy
    is `lru_ttl` and `expiry_time` is in the past.

    \param key The key of the entry.
    \param value A pointer to the first byte of the value.
    \param value_size The size of the value in bytes.
    \param metadata A pointer to the first byte of the metadata.
    \param metadata_size The size of the metadata in bytes.
    \param expiry_time The time at which the entry expires.

    \throws invalid_argument `key` is the empty string.
    \throws logic_error The sum of sizes of the entry and metadata exceeds the maximum cache size.
    \throws logic_error The cache policy is `lru_only` and a non-infinite expiry time was provided.
    */
    bool put(std::string const& key,
             char const* value,
             int64_t value_size,
             char const* metadata,
             int64_t metadata_size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Function called by the cache to load an entry after a cache miss.
    */
    typedef std::function<void(std::string const& key, PersistentStringCache& cache)> Loader;

    /**
    \brief Atomically retrieves or stores a cache entry.

    `get_or_put` attempts to retrieve the value of a (non-expired) entry.
    If the entry can be found, it returns its value. Otherwise, it calls
    `load_func`, which is expected to add the entry to the cache. If the
    load function succeeds in adding the entry, the value added by the load
    function is returned. The load function is called by the application thread.

    \return A null value if the entry could not be retrieved or loaded; the value of the entry, otherwise.
    \throws runtime_error The load function threw an exception.

    \note The load function must (synchronously) call one of the overloaded `put` methods to add a
    new entry for the provided key. Calling any other method on the cache from within the load
    function causes undefined behavior.

    \warning This operation holds a lock on the cache while the load function runs.
    This means that, if multiple threads call into the cache, they will be blocked
    for the duration of the load function.
    */
    Optional<std::string> get_or_put(std::string const& key, Loader const& load_func);

    /**
    \brief Atomically retrieves or stores a cache entry.

    `get_or_put` attempts to retrieve the value and metadata of a (non-expired) entry.
    If the entry can be found, it returns its data. Otherwise, it calls
    `load_func`, which is expected to add the entry to the cache. If the
    load function succeeds in adding the entry, the data added by the load
    function is returned. The load function is called by the application thread.

    \return A null value if the entry could not be retrieved or loaded; the value and metadata of the entry, otherwise.
    \throws runtime_error The load function threw an exception.

    \note The load function must (synchronously) call one of the overloaded `put` methods to add a
    new entry for the provided key. Calling any other method on the cache from within the load
    function causes undefined behavior.

    \warning This operation holds a lock on the cache while the load function runs.
    This means that, if multiple threads call into the cache, they will be blocked
    for the duration of the load function.
    */
    Optional<Data> get_or_put_data(std::string const& key, Loader const& load_func);

    /**
    \brief Adds or replaces the metadata for an entry.

    If a (non-expired) entry with the given key exists in the cache, its metadata
    is set to the provided value, replacing any previous metadata.

    \return `true` if the metadata was added or updated. `false` if the
    entry could not be found or was expired.

    \throws invalid_argument `key` is the empty string.
    \throws logic_error The new size of the entry would exceed the maximum cache size.

    \note This operation does _not_ update the access time of the entry.
    \see touch()
    */
    bool put_metadata(std::string const& key, std::string const& metadata);

    /**
    \brief Adds or replaces the metadata for an entry.

    \note This overload is provided to avoid the need to construct a string
    for the metadata.

    If a (non-expired) entry with the given key exists in the cache, its metadata
    is set to the provided value, replacing any previous metadata.

    \param key The key of the entry.
    \param metadata A pointer to the first byte of the metadata.
    \param size The size of the metadata in bytes.

    \return `true` if the metadata was added or updated. `false` if the
    entry could not be found or was expired.

    \throws invalid_argument `key` is the empty string.
    \throws invalid_argument `metadata` is `nullptr`.
    \throws invalid_argument `size` is negative.
    \throws logic_error The new size of the entry would exceed the maximum cache size.

    \note This operation does _not_ update the access time of the entry.
    \see touch()
    */
    bool put_metadata(std::string const& key, char const* metadata, int64_t size);

    /**
    \brief Removes an entry and returns its value.

    If a (non-expired) entry with the given key can be found, it is removed
    from the cache and its value returned.

    \return A null value if the entry could not be found; the value of the entry, otherwise.
    \throws invalid_argument `key` is the empty string.
    */
    Optional<std::string> take(std::string const& key);

    /**
    \brief Removes an entry and returns its value and metadata.

    If a (non-expired) entry with the given key can be found, it is removed
    from the cache and its data returned.
    If no metadata exists, `Data::metadata` is set to the empty string.

    \return A null value if the entry could not be retrieved; the value and metadata of the entry, otherwise.
    \throws invalid_argument `key` is the empty string.
    */
    Optional<Data> take_data(std::string const& key);

    /**
    \brief Removes an entry and its associated metadata (if any).

    If a (non-expired) entry with the given key can be found, it is removed
    from the cache.

    \return `true` if the entry was removed; `false` if the entry could not be found or was expired.
    \throws invalid_argument `key` is the empty string.
    */
    bool invalidate(std::string const& key);

    /**
    \brief Atomically removes the specified entries from the cache.

    \param keys A vector of keys for the entries to be removed. If the vector is empty, this operation is
    a no-op. If one or more keys are empty or specify non-existent entries, they are ignored.
    */
    void invalidate(std::vector<std::string> const& keys);

    /**
    \brief Atomically removes the specified entries from the cache.

    \param begin Iterator to the first key for the entries to be removed.
    \param end Iterator to the one-beyond-the-last key for the entries to be removed.

    If the iterator range is empty, this operation is a no-op.
    If one or more keys are empty or specify non-existent entries, they are ignored.
    */
    template<typename It>
    void invalidate(It begin, It end)
    {
        std::vector<std::string> keys;
        while (begin < end)
        {
            keys.push_back(*begin++);
        }
        invalidate(keys);
    }

    /**
    \brief Atomically removes the specified entries from the cache.

    \param keys The keys for the entries to be removed. If `keys` is empty, this operation is
    a no-op. If one or more keys are empty or specify non-existent entries, they are ignored.
    */
    void invalidate(std::initializer_list<std::string> const& keys);

    /**
    \brief Deletes all entries from the cache.

    This operation completely empties the cache.
    \note Clearing the cache also resets the statistics counters.
    \see clear_stats()
    */
    void invalidate();

    /**
    \brief Updates the access time of an entry.

    If the entry specified by `key` is still in the cache (whether expired or not),
    it is marked as the most-recently used entry. If the policy is `lru_ttl`, the
    entry's expiry time is updated with the specified time (infinite expiry by default).

    \return `true` if the entry was updated; `false` if the entry could not be found or
    `expiry_time` is in the past.
    \throws invalid_argument `key` is the empty string.
    \throws logic_error `key` is the empty string.
    \throws logic_error The cache policy is `lru_only` and a non-infinite expiry time was provided.
    */
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Resets all statistics counters.
    */
    void clear_stats();

    /**
    \brief Changes the maximum size of the cache.

    If `size_in_bytes` is greater or equal to max_size_in_bytes(), the cache size is set to `size_in_bytes`.

    If `size_in_bytes` is less than max_size_in_bytes(), the cache discards existing entries until
    the size falls to (or below) `size_in_bytes` and sets the cache size to the new value.

    \throws invalid_argument `size_in_bytes` is &lt; 1

    \note If the new size is less than the current size, this operation compacts the database
    to use the smallest possible amount of disk space.
    */
    void resize(int64_t size_in_bytes);

    /**
    \brief Expires entries.

    Expires entries using the cache's expiration policy until the cache size falls to or below
    `used_size_in_bytes`. If `used_size_in_bytes` is less than the current cache size, this
    operation is a no-op.

    \throws invalid_argument `used_size_in_bytes` is &lt; 0
    \throws logic_error `used_size_in_bytes` is &gt; max_size_in_bytes().
    */
    void trim_to(int64_t used_size_in_bytes);

    /**
    \brief Compacts the database.

    This operation compacts the database to consume as little disk space as possible.
    Note that this operation can be slow. (Compacting a 100 MB cache can take around
    ten seconds on a machine with a spinning-platter disk.)
    */
    void compact();

    //@}

    /** @name Monitoring cache activity

    The cache allows you to register one or more callback functions that are called when
    the cache contents change.

    \note Callback functions are called by the application thread that triggered the
    corresponding event.

    \warning Do not invoke operations on the cache from within a callback function. Doing
    so has undefined behavior.
    */

    //{@

    /**
    \brief The type of a handler function.

    \note Callback functions are called by the application thread that triggered the
    corresponding event.

    \warning Do not invoke operations on the cache from within a callback function. Doing
    so has undefined behavior.

    \param key The key of the entry.
    \param ev The event type.
    \param stats The cache statistics. Note that the `stats` parameter reflects the state
    of the cache _after_ the corresponding event. For example, for a `Put` event, `stats.size_in_bytes()`
    _includes_ the size of the added entry.
    */
    typedef std::function<void(std::string const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    /**
    \brief Installs a handler for one or more events.

    \param events A bitwise OR of the event types for which to install the handler. To install
    a handler for all events, you can use core::AllCacheEvents.

    \param cb The handler to install. To cancel an existing handler, pass `nullptr`.

    For example, to install a handler for `get` and `put` events, you could use:

    \code{.cpp}
    auto cache = PersistentStringCache::open("my_cache");

    auto handler = [](string const& key, CacheEvent event, PersistentCacheStats const& stats)
    {
        // ...
    };
    cache->set_handler(CacheEvent::get | CacheEvent::put, handler);
    \endcode

    \see CacheEvent
    */
    void set_handler(CacheEvent events, EventCallback cb);

    //@}

private:
    // @cond
    PersistentStringCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentStringCache(std::string const& cache_path);

    std::unique_ptr<internal::PersistentStringCacheImpl> p_;
    // @endcond
};

}  // namespace core
