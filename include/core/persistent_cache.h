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

#include <core/cache_codec.h>
#include <core/persistent_string_cache.h>

namespace core
{

/**
\brief A persistent cache of key-value pairs and metadata of user-defined type.

`K`, `V`, and `M` are the key type, value type, and metadata type, respectively.

\note This template is a simple type adapter that forwards to
core::PersistentStringCache. See the documentation there for details
on cache operations and semantics.

In order to use the cache with custom types (other than `std::string`),
you must provide methods to encode the type to `string`, and decode
from `string` back to the type.

For example, suppose we have the following structure that we want to use
as the key type of the cache:

\code{.cpp}
struct Person
{
    string name;
    int    age;
};
\endcode

In order to use the cache with the `Person` struct as the key, you must specialize
the CacheCodec struct in namespace `core`:

\code{.cpp}
namespace core  // Specializations must be placed into namespace core.
{

template <>
string CacheCodec<Person>::encode(Person const& p)
{
    ostringstream s;
    s << p.age << ' ' << p.name;
    return s.str();
}

template <>
Person CacheCodec<Person>::decode(string const& str)
{
    istringstream s;
    Person p;
    s >> p.age >> p.name;
    return p;
}

}  // namespace core
\endcode

For this example, it is convenient to stream the age first because this
guarantees that `decode()` will work correctly even if the name contains
a space. The order in which you stream the fields does not matter, only that
(for custom _key_ types) the string representation of each value is unique.

With these two methods defined, we can now use the cache with `Person` instances
as the key. For example:

\code{.cpp}
// Custom cache using Person as the key, and string as the value and metadata.
using PersonCache = core::PersistentCache<Person, string>;

auto c = PersonCache::open("my_cache", 1024 * 1024 * 1024, CacheDiscardPolicy::LRU_only);

Person bjarne{"Bjarne Stroustrup", 65};
c->put(bjarne, "C++ inventor");
auto value = c->get(bjarne);
if (value)
{
    cout << bjarne.name << ": " << *value << endl;
}
Person person{"no such person", 0};
value = c->get(person);
assert(!value);
\endcode

Running this code produces the output:

\code
Bjarne Stroustrup: C++ inventor
\endcode

You can use a custom type for the cache's value and metadata as well by simply providing
CacheCodec specializations as needed.

\see core::CacheCodec
\see core::PersistentStringCache
*/

template <typename K, typename V, typename M = std::string>
class PersistentCache
{
public:
    /**
    Convenience typedef for the return type of open().
    */
    typedef std::unique_ptr<PersistentCache<K, V, M>> UPtr;

    /**
    \brief Simple pair of value and metadata.
    */
    struct Data
    {
        /**
        \brief Stores the value of an entry.
        */
        V value;

        /**
        \brief Stores the metadata of an entry. If no metadata exists for an entry,
        `metadata` is returned as the empty string when it is retrieved.
        */
        M metadata;
    };

    /** @name Typedefs for nullable keys, values, and metadata.
    */

    //{@

    /**
    \brief Convenience typedefs for returning nullable values.

    \note You should use `OptionalKey`, `OptionalValue`, and `OptionalMetadata` in
    your code in preference to `boost::optional`. This will ease an eventual
    transition to `std::optional`.
    */
    typedef Optional<K> OptionalKey;
    typedef Optional<V> OptionalValue;
    typedef Optional<M> OptionalMetadata;
    typedef Optional<Data> OptionalData;

    //@}

    /** @name Copy and Assignment
    */

    //{@
    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;
    //@}

    /**
    Destroys the instance.

    The destructor compacts the database. This ensures that, while a cache is
    not in use, it comsumes as little disk space as possible.
    */
    ~PersistentCache() = default;

    /** @name Creation Methods
    */

    //{@

    /**
    \brief Creates or opens a PersistentCache.
    */
    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);

    /**
    \brief Opens an existing PersistentCache.
    */
    static UPtr open(std::string const& cache_path);

    //@}

    /** @name Accessors
    */

    //{@

    /**
    \brief Returns the value of an entry in the cache, provided the entry has not expired.
    */
    OptionalValue get(K const& key) const;

    /**
    \brief Returns the data for an entry in the cache, provided the entry has not expired.
    */
    OptionalData get_data(K const& key) const;

    /**
    \brief Returns the metadata for an entry in the cache, provided the entry has not expired.
    */
    OptionalMetadata get_metadata(K const& key) const;

    /**
    \brief Tests if an (unexpired) entry is in the cache.
    */
    bool contains_key(K const& key) const;

    /**
    \brief Returns the number of entries in the cache.
    */
    int64_t size() const noexcept;

    /**
    \brief Returns the number of bytes consumed by entries in the cache.
    */
    int64_t size_in_bytes() const noexcept;

    /**
    \brief Returns the maximum size of the cache in bytes.
    */
    int64_t max_size_in_bytes() const noexcept;

    /**
    \brief Returns an estimate of the disk space consumed by the cache.
    */
    int64_t disk_size_in_bytes() const;

    /**
    \brief Returns the discard policy of the cache.
    */
    CacheDiscardPolicy discard_policy() const noexcept;

    /**
    \brief Returns statistics for the cache.
    */
    PersistentCacheStats stats() const;

    //@}

    /** @name Modifiers
    */

    //{@

    /**
    \brief Adds or updates an entry. If `V` = `std::string`, the method is also overloaded to
    to accept `char const*` and `size`.
    */
    bool put(K const& key,
             V const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Adds or updates an entry and its metadata. If 'V' or `M` = `std::string`,
    the method is also overloaded to accept `char const*` and `size`.
    */
    bool put(K const& key,
             V const& value,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Function called by the cache to load an entry after a cache miss.
    */
    typedef std::function<void(K const& key, PersistentCache<K, V, M>& cache)> Loader;

    /**
    \brief Atomically retrieves or stores a cache entry.
    */
    OptionalValue get_or_put(K const& key, Loader const& load_func);

    /**
    \brief Atomically retrieves or stores a cache entry.
    */
    OptionalData get_or_put_data(K const& key, Loader const& load_func);

    /**
    \brief Adds or replaces the metadata for an entry. If `M` = `std::string`, an overload that accepts
    `const char*` and `size` is provided as well.
    */
    bool put_metadata(K const& key, M const& metadata);

    /**
    \brief Removes an entry and returns its value.
    */
    OptionalValue take(K const& key);

    /**
    \brief Removes an entry and returns its value and metadata.
    */
    OptionalData take_data(K const& key);

    /**
    \brief Removes an entry and its associated metadata (if any).
    */
    bool invalidate(K const& key);

    /**
    \brief Atomically removes the specified entries from the cache.
    */
    void invalidate(std::vector<K> const& keys);

    /**
    \brief Atomically removes the specified entries from the cache.
    */
    template <typename It>
    void invalidate(It begin, It end);

    /**
    \brief Atomically removes the specified entries from the cache.
    */
    void invalidate(std::initializer_list<K> const& keys);

    /**
    \brief Deletes all entries from the cache.
    */
    void invalidate();

    /**
    \brief Updates the access time of an entry.
    */
    bool touch(
        K const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    /**
    \brief Resets all statistics counters.
    */
    void clear_stats();

    /**
    \brief Changes the maximum size of the cache.
    */
    void resize(int64_t size_in_bytes);

    /**
    \brief Expires entries.
    */
    void trim_to(int64_t used_size_in_bytes);

    /**
    \brief Compacts the database.
    */
    void compact();

    //@}

    /** @name Monitoring cache activity
    */

    //{@

    /**
    \brief The type of a handler function.
    */
    typedef std::function<void(K const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    /**
    \brief Installs a handler for one or more events.
    */
    void set_handler(CacheEvent events, EventCallback cb);

    //@}

private:
    // @cond
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
    // @endcond
};

// @cond

// Method implementations. Out-of-line because, otherwise, things become completely unreadable.

template <typename K, typename V, typename M>
PersistentCache<K, V, M>::PersistentCache(std::string const& cache_path,
                                          int64_t max_size_in_bytes,
                                          CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename K, typename V, typename M>
PersistentCache<K, V, M>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::UPtr PersistentCache<K, V, M>::open(std::string const& cache_path,
                                                                       int64_t max_size_in_bytes,
                                                                       CacheDiscardPolicy policy)
{
    return PersistentCache<K, V, M>::UPtr(new PersistentCache<K, V, M>(cache_path, max_size_in_bytes, policy));
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::UPtr PersistentCache<K, V, M>::open(std::string const& cache_path)
{
    return PersistentCache<K, V, M>::UPtr(new PersistentCache<K, V, M>(cache_path));
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalValue PersistentCache<K, V, M>::get(K const& key) const
{
    auto const& svalue = p_->get(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalData PersistentCache<K, V, M>::get_data(K const& key) const
{
    auto sdata = p_->get_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalMetadata PersistentCache<K, V, M>::get_metadata(K const& key) const
{
    auto smeta = p_->get_metadata(CacheCodec<K>::encode(key));
    return smeta ? OptionalMetadata(CacheCodec<M>::decode(*smeta)) : OptionalMetadata();
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::contains_key(K const& key) const
{
    return p_->contains_key(CacheCodec<K>::encode(key));
}

template <typename K, typename V, typename M>
int64_t PersistentCache<K, V, M>::size() const noexcept
{
    return p_->size();
}

template <typename K, typename V, typename M>
int64_t PersistentCache<K, V, M>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename K, typename V, typename M>
int64_t PersistentCache<K, V, M>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename K, typename V, typename M>
int64_t PersistentCache<K, V, M>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename K, typename V, typename M>
CacheDiscardPolicy PersistentCache<K, V, M>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename K, typename V, typename M>
PersistentCacheStats PersistentCache<K, V, M>::stats() const
{
    return p_->stats();
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::put(K const& key,
                                   V const& value,
                                   std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), CacheCodec<V>::encode(value), expiry_time);
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::put(K const& key,
                                   V const& value,
                                   M const& metadata,
                                   std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), CacheCodec<V>::encode(value), CacheCodec<M>::encode(metadata),
                   expiry_time);
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalValue PersistentCache<K, V, M>::get_or_put(
    K const& key, PersistentCache<K, V, M>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(skey, sload_func);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalData PersistentCache<K, V, M>::get_or_put_data(
    K const& key, PersistentCache<K, V, M>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(skey, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::put_metadata(K const& key, M const& metadata)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), CacheCodec<M>::encode(metadata));
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalValue PersistentCache<K, V, M>::take(K const& key)
{
    auto svalue = p_->take(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V, typename M>
typename PersistentCache<K, V, M>::OptionalData PersistentCache<K, V, M>::take_data(K const& key)
{
    auto sdata = p_->take_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::invalidate(K const& key)
{
    return p_->invalidate(CacheCodec<K>::encode(key));
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::invalidate(std::vector<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename V, typename M>
template <typename It>
void PersistentCache<K, V, M>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(CacheCodec<K>::encode(*it));
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::invalidate(std::initializer_list<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::invalidate()
{
    p_->invalidate();
}

template <typename K, typename V, typename M>
bool PersistentCache<K, V, M>::touch(K const& key, std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(CacheCodec<K>::encode(key), expiry_time);
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::clear_stats()
{
    p_->clear_stats();
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::compact()
{
    p_->compact();
}

template <typename K, typename V, typename M>
void PersistentCache<K, V, M>::set_handler(CacheEvent events, EventCallback cb)
{
    auto scb = [cb](std::string const& key, CacheEvent ev, PersistentCacheStats const& c)
    {
        cb(CacheCodec<K>::decode(key), ev, c);
    };
    p_->set_handler(events, scb);
}

// Below are specializations for the various combinations of one or more of K, V, and M
// being of type std::string. Without this, we would end up calling through a string-to-string
// codec, which would force a copy for everything of type string, which is brutally inefficient.
//
// This is verbose because we must specialize the class template in order to avoid
// calling the CacheCodec methods for type string. The combinations that follow are
// <s,V,M>, <K,s,M>, <K,V,s>, <s,s,M>, <s,V,s>, <K,s,s>, and <s,s,s>.
// The code is identical, except that, where we know that K, V, or M are std::string,
// it avoids calling the encode()/decode() methods.

// Specialization for K = std::string.

template <typename V, typename M>
class PersistentCache<std::string, V, M>
{
public:
    typedef std::unique_ptr<PersistentCache<std::string, V, M>> UPtr;

    typedef Optional<std::string> OptionalKey;
    typedef Optional<V> OptionalValue;
    typedef Optional<M> OptionalMetadata;

    struct Data
    {
        V value;
        M metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(std::string const& key) const;
    OptionalData get_data(std::string const& key) const;
    OptionalMetadata get_metadata(std::string const& key) const;
    bool contains_key(std::string const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(std::string const& key,
             V const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             V const& value,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(std::string const& key, PersistentCache<std::string, V, M>& cache)> Loader;

    OptionalValue get_or_put(std::string const& key, Loader const& load_func);
    OptionalData get_or_put_data(std::string const& key, Loader const& load_func);

    bool put_metadata(std::string const& key, M const& metadata);
    OptionalValue take(std::string const& key);
    OptionalData take_data(std::string const& key);
    bool invalidate(std::string const& key);
    void invalidate(std::vector<std::string> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<std::string> const& keys);
    void invalidate();
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(std::string const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename V, typename M>
PersistentCache<std::string, V, M>::PersistentCache(std::string const& cache_path,
                                                    int64_t max_size_in_bytes,
                                                    CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename V, typename M>
PersistentCache<std::string, V, M>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::UPtr PersistentCache<std::string, V, M>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<std::string, V, M>::UPtr(
        new PersistentCache<std::string, V, M>(cache_path, max_size_in_bytes, policy));
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::UPtr PersistentCache<std::string, V, M>::open(
    std::string const& cache_path)
{
    return PersistentCache<std::string, V, M>::UPtr(new PersistentCache<std::string, V, M>(cache_path));
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalValue PersistentCache<std::string, V, M>::get(
    std::string const& key) const
{
    auto const& svalue = p_->get(key);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalData PersistentCache<std::string, V, M>::get_data(
    std::string const& key) const
{
    auto sdata = p_->get_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalMetadata PersistentCache<std::string, V, M>::get_metadata(
    std::string const& key) const
{
    auto smeta = p_->get_metadata(key);
    return smeta ? OptionalMetadata(CacheCodec<M>::decode(*smeta)) : OptionalMetadata();
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::contains_key(std::string const& key) const
{
    return p_->contains_key(key);
}

template <typename V, typename M>
int64_t PersistentCache<std::string, V, M>::size() const noexcept
{
    return p_->size();
}

template <typename V, typename M>
int64_t PersistentCache<std::string, V, M>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename V, typename M>
int64_t PersistentCache<std::string, V, M>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename V, typename M>
int64_t PersistentCache<std::string, V, M>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename V, typename M>
CacheDiscardPolicy PersistentCache<std::string, V, M>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename V, typename M>
PersistentCacheStats PersistentCache<std::string, V, M>::stats() const
{
    return p_->stats();
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::put(std::string const& key,
                                             V const& value,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, CacheCodec<V>::encode(value), expiry_time);
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::put(std::string const& key,
                                             V const& value,
                                             M const& metadata,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, CacheCodec<V>::encode(value), CacheCodec<M>::encode(metadata), expiry_time);
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalValue PersistentCache<std::string, V, M>::get_or_put(
    std::string const& key, PersistentCache<std::string, V, M>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(key, sload_func);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalData PersistentCache<std::string, V, M>::get_or_put_data(
    std::string const& key, PersistentCache<std::string, V, M>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(key, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::put_metadata(std::string const& key, M const& metadata)
{
    return p_->put_metadata(key, CacheCodec<M>::encode(metadata));
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalValue PersistentCache<std::string, V, M>::take(
    std::string const& key)
{
    auto svalue = p_->take(key);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V, typename M>
typename PersistentCache<std::string, V, M>::OptionalData PersistentCache<std::string, V, M>::take_data(
    std::string const& key)
{
    auto sdata = p_->take_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), CacheCodec<M>::decode(sdata->metadata)});
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::invalidate(std::string const& key)
{
    return p_->invalidate(key);
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::invalidate(std::vector<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename V, typename M>
template <typename It>
void PersistentCache<std::string, V, M>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(*it);
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::invalidate(std::initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::invalidate()
{
    p_->invalidate();
}

template <typename V, typename M>
bool PersistentCache<std::string, V, M>::touch(std::string const& key,
                                               std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(key, expiry_time);
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::clear_stats()
{
    p_->clear_stats();
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::compact()
{
    p_->compact();
}

template <typename V, typename M>
void PersistentCache<std::string, V, M>::set_handler(CacheEvent events, EventCallback cb)
{
    p_->set_handler(events, cb);
}

// Specialization for V = std::string.

template <typename K, typename M>
class PersistentCache<K, std::string, M>
{
public:
    typedef std::unique_ptr<PersistentCache<K, std::string, M>> UPtr;

    typedef Optional<K> OptionalKey;
    typedef Optional<std::string> OptionalValue;
    typedef Optional<M> OptionalMetadata;

    struct Data
    {
        std::string value;
        M metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(K const& key) const;
    OptionalData get_data(K const& key) const;
    OptionalMetadata get_metadata(K const& key) const;
    bool contains_key(K const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(K const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             std::string const& value,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             char const* value,
             int64_t value_size,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(K const& key, PersistentCache<K, std::string, M>& cache)> Loader;

    OptionalValue get_or_put(K const& key, Loader const& load_func);
    OptionalData get_or_put_data(K const& key, Loader const& load_func);

    bool put_metadata(K const& key, M const& metadata);
    OptionalValue take(K const& key);
    OptionalData take_data(K const& key);
    bool invalidate(K const& key);
    void invalidate(std::vector<K> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<K> const& keys);
    void invalidate();
    bool touch(
        K const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(K const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename K, typename M>
PersistentCache<K, std::string, M>::PersistentCache(std::string const& cache_path,
                                                    int64_t max_size_in_bytes,
                                                    CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename K, typename M>
PersistentCache<K, std::string, M>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::UPtr PersistentCache<K, std::string, M>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<K, std::string, M>::UPtr(
        new PersistentCache<K, std::string, M>(cache_path, max_size_in_bytes, policy));
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::UPtr PersistentCache<K, std::string, M>::open(
    std::string const& cache_path)
{
    return PersistentCache<K, std::string, M>::UPtr(new PersistentCache<K, std::string, M>(cache_path));
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalValue PersistentCache<K, std::string, M>::get(K const& key) const
{
    auto const& svalue = p_->get(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalData PersistentCache<K, std::string, M>::get_data(
    K const& key) const
{
    auto sdata = p_->get_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalMetadata PersistentCache<K, std::string, M>::get_metadata(
    K const& key) const
{
    auto smeta = p_->get_metadata(CacheCodec<K>::encode(key));
    return smeta ? OptionalMetadata(CacheCodec<M>::decode(*smeta)) : OptionalMetadata();
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::contains_key(K const& key) const
{
    return p_->contains_key(CacheCodec<K>::encode(key));
}

template <typename K, typename M>
int64_t PersistentCache<K, std::string, M>::size() const noexcept
{
    return p_->size();
}

template <typename K, typename M>
int64_t PersistentCache<K, std::string, M>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename K, typename M>
int64_t PersistentCache<K, std::string, M>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename K, typename M>
int64_t PersistentCache<K, std::string, M>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename K, typename M>
CacheDiscardPolicy PersistentCache<K, std::string, M>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename K, typename M>
PersistentCacheStats PersistentCache<K, std::string, M>::stats() const
{
    return p_->stats();
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::put(K const& key,
                                             std::string const& value,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, expiry_time);
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::put(K const& key,
                                             char const* value,
                                             int64_t size,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, size, expiry_time);
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::put(K const& key,
                                             std::string const& value,
                                             M const& metadata,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, CacheCodec<M>::encode(metadata), expiry_time);
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::put(K const& key,
                                             char const* value,
                                             int64_t size,
                                             M const& metadata,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string md = CacheCodec<M>::encode(metadata);
    return p_->put(CacheCodec<K>::encode(key), value, size, md.data(), md.size(), expiry_time);
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalValue PersistentCache<K, std::string, M>::get_or_put(
    K const& key, PersistentCache<K, std::string, M>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(skey, sload_func);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalData PersistentCache<K, std::string, M>::get_or_put_data(
    K const& key, PersistentCache<K, std::string, M>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(skey, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::put_metadata(K const& key, M const& metadata)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), CacheCodec<M>::encode(metadata));
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalValue PersistentCache<K, std::string, M>::take(K const& key)
{
    auto svalue = p_->take(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K, typename M>
typename PersistentCache<K, std::string, M>::OptionalData PersistentCache<K, std::string, M>::take_data(
    K const& key)
{
    auto sdata = p_->take_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::invalidate(K const& key)
{
    return p_->invalidate(CacheCodec<K>::encode(key));
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::invalidate(std::vector<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename M>
template <typename It>
void PersistentCache<K, std::string, M>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(CacheCodec<K>::encode(*it));
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::invalidate(std::initializer_list<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::invalidate()
{
    p_->invalidate();
}

template <typename K, typename M>
bool PersistentCache<K, std::string, M>::touch(K const& key,
                                               std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(CacheCodec<K>::encode(key), expiry_time);
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::clear_stats()
{
    p_->clear_stats();
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::compact()
{
    p_->compact();
}

template <typename K, typename M>
void PersistentCache<K, std::string, M>::set_handler(CacheEvent events, EventCallback cb)
{
    auto scb = [cb](std::string const& key, CacheEvent ev, PersistentCacheStats const& c)
    {
        cb(CacheCodec<K>::decode(key), ev, c);
    };
    p_->set_handler(events, scb);
}

// Specialization for M = std::string.

template <typename K, typename V>
class PersistentCache<K, V, std::string>
{
public:
    typedef std::unique_ptr<PersistentCache<K, V, std::string>> UPtr;

    typedef Optional<K> OptionalKey;
    typedef Optional<V> OptionalValue;
    typedef Optional<std::string> OptionalMetadata;

    struct Data
    {
        V value;
        std::string metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(K const& key) const;
    OptionalData get_data(K const& key) const;
    OptionalMetadata get_metadata(K const& key) const;
    bool contains_key(K const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(K const& key,
             V const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             V const& value,
             std::string const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             V const& value,
             char const* metadata,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(K const& key, PersistentCache<K, V, std::string>& cache)> Loader;

    OptionalValue get_or_put(K const& key, Loader const& load_func);
    OptionalData get_or_put_data(K const& key, Loader const& load_func);

    bool put_metadata(K const& key, std::string const& metadata);
    bool put_metadata(K const& key, char const* metadata, int64_t size);
    OptionalValue take(K const& key);
    OptionalData take_data(K const& key);
    bool invalidate(K const& key);
    void invalidate(std::vector<K> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<K> const& keys);
    void invalidate();
    bool touch(
        K const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(K const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename K, typename V>
PersistentCache<K, V, std::string>::PersistentCache(std::string const& cache_path,
                                                    int64_t max_size_in_bytes,
                                                    CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename K, typename V>
PersistentCache<K, V, std::string>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::UPtr PersistentCache<K, V, std::string>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<K, V, std::string>::UPtr(
        new PersistentCache<K, V, std::string>(cache_path, max_size_in_bytes, policy));
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::UPtr PersistentCache<K, V, std::string>::open(
    std::string const& cache_path)
{
    return PersistentCache<K, V, std::string>::UPtr(new PersistentCache<K, V, std::string>(cache_path));
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalValue PersistentCache<K, V, std::string>::get(K const& key) const
{
    auto const& svalue = p_->get(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalData PersistentCache<K, V, std::string>::get_data(
    K const& key) const
{
    auto sdata = p_->get_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalMetadata PersistentCache<K, V, std::string>::get_metadata(
    K const& key) const
{
    auto smeta = p_->get_metadata(CacheCodec<K>::encode(key));
    return smeta ? OptionalMetadata(*smeta) : OptionalMetadata();
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::contains_key(K const& key) const
{
    return p_->contains_key(CacheCodec<K>::encode(key));
}

template <typename K, typename V>
int64_t PersistentCache<K, V, std::string>::size() const noexcept
{
    return p_->size();
}

template <typename K, typename V>
int64_t PersistentCache<K, V, std::string>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename K, typename V>
int64_t PersistentCache<K, V, std::string>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename K, typename V>
int64_t PersistentCache<K, V, std::string>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename K, typename V>
CacheDiscardPolicy PersistentCache<K, V, std::string>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename K, typename V>
PersistentCacheStats PersistentCache<K, V, std::string>::stats() const
{
    return p_->stats();
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::put(K const& key,
                                             V const& value,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), CacheCodec<V>::encode(value), expiry_time);
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::put(K const& key,
                                             V const& value,
                                             std::string const& metadata,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string v = CacheCodec<V>::encode(value);
    return p_->put(CacheCodec<K>::encode(key), v.data(), v.size(), metadata.data(), metadata.size(), expiry_time);
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::put(K const& key,
                                             V const& value,
                                             char const* metadata,
                                             int64_t size,
                                             std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string v = CacheCodec<V>::encode(value);
    return p_->put(CacheCodec<K>::encode(key), v.data(), v.size(), metadata, size, expiry_time);
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalValue PersistentCache<K, V, std::string>::get_or_put(
    K const& key, PersistentCache<K, V, std::string>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(skey, sload_func);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalData PersistentCache<K, V, std::string>::get_or_put_data(
    K const& key, PersistentCache<K, V, std::string>::Loader const& load_func)
{
    std::string const& skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(skey, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::put_metadata(K const& key, std::string const& metadata)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), metadata);
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::put_metadata(K const& key, char const* metadata, int64_t size)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), metadata, size);
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalValue PersistentCache<K, V, std::string>::take(K const& key)
{
    auto svalue = p_->take(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename K, typename V>
typename PersistentCache<K, V, std::string>::OptionalData PersistentCache<K, V, std::string>::take_data(
    K const& key)
{
    auto sdata = p_->take_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::invalidate(K const& key)
{
    return p_->invalidate(CacheCodec<K>::encode(key));
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::invalidate(std::vector<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename V>
template <typename It>
void PersistentCache<K, V, std::string>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(CacheCodec<K>::encode(*it));
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::invalidate(std::initializer_list<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::invalidate()
{
    p_->invalidate();
}

template <typename K, typename V>
bool PersistentCache<K, V, std::string>::touch(K const& key,
                                               std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(CacheCodec<K>::encode(key), expiry_time);
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::clear_stats()
{
    p_->clear_stats();
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::compact()
{
    p_->compact();
}

template <typename K, typename V>
void PersistentCache<K, V, std::string>::set_handler(CacheEvent events, EventCallback cb)
{
    auto scb = [cb](std::string const& key, CacheEvent ev, PersistentCacheStats const& c)
    {
        cb(CacheCodec<K>::decode(key), ev, c);
    };
    p_->set_handler(events, scb);
}

// Specialization for K and V = std::string.

template <typename M>
class PersistentCache<std::string, std::string, M>
{
public:
    typedef std::unique_ptr<PersistentCache<std::string, std::string, M>> UPtr;

    typedef Optional<std::string> OptionalKey;
    typedef Optional<std::string> OptionalValue;
    typedef Optional<M> OptionalMetadata;

    struct Data
    {
        std::string value;
        M metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(std::string const& key) const;
    OptionalData get_data(std::string const& key) const;
    OptionalMetadata get_metadata(std::string const& key) const;
    bool contains_key(std::string const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             std::string const& value,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value,
             int64_t value_size,
             M const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(std::string const& key, PersistentCache<std::string, std::string, M>& cache)> Loader;

    OptionalValue get_or_put(std::string const& key, Loader const& load_func);
    OptionalData get_or_put_data(std::string const& key, Loader const& load_func);

    bool put_metadata(std::string const& key, M const& metadata);
    OptionalValue take(std::string const& key);
    OptionalData take_data(std::string const& key);
    bool invalidate(std::string const& key);
    void invalidate(std::vector<std::string> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<std::string> const& keys);
    void invalidate();
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(std::string const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename M>
PersistentCache<std::string, std::string, M>::PersistentCache(std::string const& cache_path,
                                                              int64_t max_size_in_bytes,
                                                              CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename M>
PersistentCache<std::string, std::string, M>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::UPtr PersistentCache<std::string, std::string, M>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<std::string, std::string, M>::UPtr(
        new PersistentCache<std::string, std::string, M>(cache_path, max_size_in_bytes, policy));
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::UPtr PersistentCache<std::string, std::string, M>::open(
    std::string const& cache_path)
{
    return PersistentCache<std::string, std::string, M>::UPtr(
        new PersistentCache<std::string, std::string, M>(cache_path));
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalValue PersistentCache<std::string, std::string, M>::get(
    std::string const& key) const
{
    auto const& svalue = p_->get(key);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalData
    PersistentCache<std::string, std::string, M>::get_data(std::string const& key) const
{
    auto sdata = p_->get_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalMetadata
    PersistentCache<std::string, std::string, M>::get_metadata(std::string const& key) const
{
    auto smeta = p_->get_metadata(key);
    return smeta ? OptionalMetadata(CacheCodec<M>::decode(*smeta)) : OptionalMetadata();
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::contains_key(std::string const& key) const
{
    return p_->contains_key(key);
}

template <typename M>
int64_t PersistentCache<std::string, std::string, M>::size() const noexcept
{
    return p_->size();
}

template <typename M>
int64_t PersistentCache<std::string, std::string, M>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename M>
int64_t PersistentCache<std::string, std::string, M>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename M>
int64_t PersistentCache<std::string, std::string, M>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename M>
CacheDiscardPolicy PersistentCache<std::string, std::string, M>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename M>
PersistentCacheStats PersistentCache<std::string, std::string, M>::stats() const
{
    return p_->stats();
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::put(std::string const& key,
                                                       std::string const& value,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, expiry_time);
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::put(std::string const& key,
                                                       char const* value,
                                                       int64_t size,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, size, nullptr, 0, expiry_time);
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::put(std::string const& key,
                                                       std::string const& value,
                                                       M const& metadata,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, CacheCodec<M>::encode(metadata), expiry_time);
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::put(std::string const& key,
                                                       char const* value,
                                                       int64_t size,
                                                       M const& metadata,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string md = CacheCodec<M>::encode(metadata);
    return p_->put(key, value, size, md.data(), md.size(), expiry_time);
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalValue
    PersistentCache<std::string, std::string, M>::get_or_put(
        std::string const& key, PersistentCache<std::string, std::string, M>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(key, sload_func);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalData
    PersistentCache<std::string, std::string, M>::get_or_put_data(
        std::string const& key, PersistentCache<std::string, std::string, M>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(key, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::put_metadata(std::string const& key, M const& metadata)
{
    return p_->put_metadata(key, CacheCodec<M>::encode(metadata));
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalValue PersistentCache<std::string, std::string, M>::take(
    std::string const& key)
{
    auto svalue = p_->take(key);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename M>
typename PersistentCache<std::string, std::string, M>::OptionalData
    PersistentCache<std::string, std::string, M>::take_data(std::string const& key)
{
    auto sdata = p_->take_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, CacheCodec<M>::decode(sdata->metadata)});
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::invalidate(std::string const& key)
{
    return p_->invalidate(key);
}

template <typename M>
void PersistentCache<std::string, std::string, M>::invalidate(std::vector<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename M>
template <typename It>
void PersistentCache<std::string, std::string, M>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(*it);
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename M>
void PersistentCache<std::string, std::string, M>::invalidate(std::initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename M>
void PersistentCache<std::string, std::string, M>::invalidate()
{
    p_->invalidate();
}

template <typename M>
bool PersistentCache<std::string, std::string, M>::touch(std::string const& key,
                                                         std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(key, expiry_time);
}

template <typename M>
void PersistentCache<std::string, std::string, M>::clear_stats()
{
    p_->clear_stats();
}

template <typename M>
void PersistentCache<std::string, std::string, M>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename M>
void PersistentCache<std::string, std::string, M>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename M>
void PersistentCache<std::string, std::string, M>::compact()
{
    p_->compact();
}

template <typename M>
void PersistentCache<std::string, std::string, M>::set_handler(CacheEvent events, EventCallback cb)
{
    p_->set_handler(events, cb);
}

// Specialization for K and M = std::string.

template <typename V>
class PersistentCache<std::string, V, std::string>
{
public:
    typedef std::unique_ptr<PersistentCache<std::string, V, std::string>> UPtr;

    typedef Optional<std::string> OptionalKey;
    typedef Optional<V> OptionalValue;
    typedef Optional<std::string> OptionalMetadata;

    struct Data
    {
        V value;
        std::string metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(std::string const& key) const;
    OptionalData get_data(std::string const& key) const;
    OptionalMetadata get_metadata(std::string const& key) const;
    bool contains_key(std::string const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(std::string const& key,
             V const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             V const& value,
             std::string const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             V const& value,
             char const* metadata,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(std::string const& key, PersistentCache<std::string, V, std::string>& cache)> Loader;

    OptionalValue get_or_put(std::string const& key, Loader const& load_func);
    OptionalData get_or_put_data(std::string const& key, Loader const& load_func);

    bool put_metadata(std::string const& key, std::string const& metadata);
    bool put_metadata(std::string const& key, char const* metadata, int64_t size);
    OptionalValue take(std::string const& key);
    OptionalData take_data(std::string const& key);
    bool invalidate(std::string const& key);
    void invalidate(std::vector<std::string> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<std::string> const& keys);
    void invalidate();
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(std::string const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename V>
PersistentCache<std::string, V, std::string>::PersistentCache(std::string const& cache_path,
                                                              int64_t max_size_in_bytes,
                                                              CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename V>
PersistentCache<std::string, V, std::string>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::UPtr PersistentCache<std::string, V, std::string>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<std::string, V, std::string>::UPtr(
        new PersistentCache<std::string, V, std::string>(cache_path, max_size_in_bytes, policy));
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::UPtr PersistentCache<std::string, V, std::string>::open(
    std::string const& cache_path)
{
    return PersistentCache<std::string, V, std::string>::UPtr(
        new PersistentCache<std::string, V, std::string>(cache_path));
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalValue PersistentCache<std::string, V, std::string>::get(
    std::string const& key) const
{
    auto const& svalue = p_->get(key);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalData
    PersistentCache<std::string, V, std::string>::get_data(std::string const& key) const
{
    auto sdata = p_->get_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalMetadata
    PersistentCache<std::string, V, std::string>::get_metadata(std::string const& key) const
{
    auto smeta = p_->get_metadata(key);
    return smeta ? OptionalMetadata(*smeta) : OptionalMetadata();
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::contains_key(std::string const& key) const
{
    return p_->contains_key(key);
}

template <typename V>
int64_t PersistentCache<std::string, V, std::string>::size() const noexcept
{
    return p_->size();
}

template <typename V>
int64_t PersistentCache<std::string, V, std::string>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename V>
int64_t PersistentCache<std::string, V, std::string>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename V>
int64_t PersistentCache<std::string, V, std::string>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename V>
CacheDiscardPolicy PersistentCache<std::string, V, std::string>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename V>
PersistentCacheStats PersistentCache<std::string, V, std::string>::stats() const
{
    return p_->stats();
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::put(std::string const& key,
                                                       V const& value,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, CacheCodec<V>::encode(value), expiry_time);
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::put(std::string const& key,
                                                       V const& value,
                                                       std::string const& metadata,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string v = CacheCodec<V>::encode(value);
    return p_->put(key, v.data(), v.size(), metadata.data(), metadata.size(), expiry_time);
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::put(std::string const& key,
                                                       V const& value,
                                                       char const* metadata,
                                                       int64_t size,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    std::string v = CacheCodec<V>::encode(value);
    return p_->put(key, v.data(), v.size(), metadata, size, expiry_time);
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalValue
    PersistentCache<std::string, V, std::string>::get_or_put(
        std::string const& key, PersistentCache<std::string, V, std::string>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(key, sload_func);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalData
    PersistentCache<std::string, V, std::string>::get_or_put_data(
        std::string const& key, PersistentCache<std::string, V, std::string>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(key, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::put_metadata(std::string const& key, std::string const& metadata)
{
    return p_->put_metadata(key, metadata);
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::put_metadata(std::string const& key,
                                                                char const* metadata,
                                                                int64_t size)
{
    return p_->put_metadata(key, metadata, size);
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalValue PersistentCache<std::string, V, std::string>::take(
    std::string const& key)
{
    auto svalue = p_->take(key);
    return svalue ? OptionalValue(CacheCodec<V>::decode(*svalue)) : OptionalValue();
}

template <typename V>
typename PersistentCache<std::string, V, std::string>::OptionalData
    PersistentCache<std::string, V, std::string>::take_data(std::string const& key)
{
    auto sdata = p_->take_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({CacheCodec<V>::decode(sdata->value), sdata->metadata});
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::invalidate(std::string const& key)
{
    return p_->invalidate(key);
}

template <typename V>
void PersistentCache<std::string, V, std::string>::invalidate(std::vector<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename V>
template <typename It>
void PersistentCache<std::string, V, std::string>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(*it);
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename V>
void PersistentCache<std::string, V, std::string>::invalidate(std::initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename V>
void PersistentCache<std::string, V, std::string>::invalidate()
{
    p_->invalidate();
}

template <typename V>
bool PersistentCache<std::string, V, std::string>::touch(std::string const& key,
                                                         std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(key, expiry_time);
}

template <typename V>
void PersistentCache<std::string, V, std::string>::clear_stats()
{
    p_->clear_stats();
}

template <typename V>
void PersistentCache<std::string, V, std::string>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename V>
void PersistentCache<std::string, V, std::string>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename V>
void PersistentCache<std::string, V, std::string>::compact()
{
    p_->compact();
}

template <typename V>
void PersistentCache<std::string, V, std::string>::set_handler(CacheEvent events, EventCallback cb)
{
    p_->set_handler(events, cb);
}

// Specialization for V and M = std::string.

template <typename K>
class PersistentCache<K, std::string, std::string>
{
public:
    typedef std::unique_ptr<PersistentCache<K, std::string, std::string>> UPtr;

    typedef Optional<K> OptionalKey;
    typedef Optional<std::string> OptionalValue;
    typedef Optional<std::string> OptionalMetadata;

    struct Data
    {
        std::string value;
        std::string metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(K const& key) const;
    OptionalData get_data(K const& key) const;
    OptionalMetadata get_metadata(K const& key) const;
    bool contains_key(K const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(K const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             std::string const& value,
             std::string const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(K const& key,
             char const* value,
             int64_t value_size,
             char const* metadata,
             int64_t metadata_size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(K const& key, PersistentCache<K, std::string, std::string>& cache)> Loader;

    OptionalValue get_or_put(K const& key, Loader const& load_func);
    OptionalData get_or_put_data(K const& key, Loader const& load_func);

    bool put_metadata(K const& key, std::string const& metadata);
    bool put_metadata(K const& key, char const* metadata, int64_t size);
    OptionalValue take(K const& key);
    OptionalData take_data(K const& key);
    bool invalidate(K const& key);
    void invalidate(std::vector<K> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<K> const& keys);
    void invalidate();
    bool touch(
        K const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(K const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

template <typename K>
PersistentCache<K, std::string, std::string>::PersistentCache(std::string const& cache_path,
                                                              int64_t max_size_in_bytes,
                                                              CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

template <typename K>
PersistentCache<K, std::string, std::string>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::UPtr PersistentCache<K, std::string, std::string>::open(
    std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy)
{
    return PersistentCache<K, std::string, std::string>::UPtr(
        new PersistentCache<K, std::string, std::string>(cache_path, max_size_in_bytes, policy));
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::UPtr PersistentCache<K, std::string, std::string>::open(
    std::string const& cache_path)
{
    return PersistentCache<K, std::string, std::string>::UPtr(
        new PersistentCache<K, std::string, std::string>(cache_path));
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalValue PersistentCache<K, std::string, std::string>::get(
    K const& key) const
{
    auto const& svalue = p_->get(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalData
    PersistentCache<K, std::string, std::string>::get_data(K const& key) const
{
    auto sdata = p_->get_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalMetadata
    PersistentCache<K, std::string, std::string>::get_metadata(K const& key) const
{
    auto smeta = p_->get_metadata(CacheCodec<K>::encode(key));
    return smeta ? OptionalMetadata(*smeta) : OptionalMetadata();
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::contains_key(K const& key) const
{
    return p_->contains_key(CacheCodec<K>::encode(key));
}

template <typename K>
int64_t PersistentCache<K, std::string, std::string>::size() const noexcept
{
    return p_->size();
}

template <typename K>
int64_t PersistentCache<K, std::string, std::string>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

template <typename K>
int64_t PersistentCache<K, std::string, std::string>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

template <typename K>
int64_t PersistentCache<K, std::string, std::string>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

template <typename K>
CacheDiscardPolicy PersistentCache<K, std::string, std::string>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

template <typename K>
PersistentCacheStats PersistentCache<K, std::string, std::string>::stats() const
{
    return p_->stats();
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put(K const& key,
                                                       std::string const& value,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, expiry_time);
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put(K const& key,
                                                       char const* value,
                                                       int64_t size,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, size, expiry_time);
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put(K const& key,
                                                       std::string const& value,
                                                       std::string const& metadata,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, metadata, expiry_time);
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put(K const& key,
                                                       char const* value,
                                                       int64_t value_size,
                                                       char const* metadata,
                                                       int64_t metadata_size,
                                                       std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(CacheCodec<K>::encode(key), value, value_size, metadata, metadata_size, expiry_time);
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalValue
    PersistentCache<K, std::string, std::string>::get_or_put(
        K const& key, PersistentCache<K, std::string, std::string>::Loader const& load_func)
{
    auto skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(skey, sload_func);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalData
    PersistentCache<K, std::string, std::string>::get_or_put_data(
        K const& key, PersistentCache<K, std::string, std::string>::Loader const& load_func)
{
    auto skey = CacheCodec<K>::encode(key);
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(skey, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put_metadata(K const& key, std::string const& metadata)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), metadata);
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::put_metadata(K const& key, char const* metadata, int64_t size)
{
    return p_->put_metadata(CacheCodec<K>::encode(key), metadata, size);
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalValue PersistentCache<K, std::string, std::string>::take(
    K const& key)
{
    auto svalue = p_->take(CacheCodec<K>::encode(key));
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

template <typename K>
typename PersistentCache<K, std::string, std::string>::OptionalData
    PersistentCache<K, std::string, std::string>::take_data(K const& key)
{
    auto sdata = p_->take_data(CacheCodec<K>::encode(key));
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::invalidate(K const& key)
{
    return p_->invalidate(CacheCodec<K>::encode(key));
}

template <typename K>
void PersistentCache<K, std::string, std::string>::invalidate(std::vector<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K>
template <typename It>
void PersistentCache<K, std::string, std::string>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(CacheCodec<K>::encode(*it));
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

template <typename K>
void PersistentCache<K, std::string, std::string>::invalidate(std::initializer_list<K> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename K>
void PersistentCache<K, std::string, std::string>::invalidate()
{
    p_->invalidate();
}

template <typename K>
bool PersistentCache<K, std::string, std::string>::touch(K const& key,
                                                         std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(CacheCodec<K>::encode(key), expiry_time);
}

template <typename K>
void PersistentCache<K, std::string, std::string>::clear_stats()
{
    p_->clear_stats();
}

template <typename K>
void PersistentCache<K, std::string, std::string>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

template <typename K>
void PersistentCache<K, std::string, std::string>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

template <typename K>
void PersistentCache<K, std::string, std::string>::compact()
{
    p_->compact();
}

template <typename K>
void PersistentCache<K, std::string, std::string>::set_handler(CacheEvent events, EventCallback cb)
{
    auto scb = [cb](std::string const& key, CacheEvent ev, PersistentCacheStats const& c)
    {
        cb(CacheCodec<K>::decode(key), ev, c);
    };
    p_->set_handler(events, scb);
}

// Specialization for K, V, and M = std::string.

template <>
class PersistentCache<std::string, std::string, std::string>
{
public:
    typedef std::unique_ptr<PersistentCache<std::string, std::string, std::string>> UPtr;

    typedef Optional<std::string> OptionalKey;
    typedef Optional<std::string> OptionalValue;
    typedef Optional<std::string> OptionalMetadata;

    struct Data
    {
        std::string value;
        std::string metadata;
    };
    typedef Optional<Data> OptionalData;

    PersistentCache(PersistentCache const&) = delete;
    PersistentCache& operator=(PersistentCache const&) = delete;

    PersistentCache(PersistentCache&&) = default;
    PersistentCache& operator=(PersistentCache&&) = default;

    ~PersistentCache() = default;

    static UPtr open(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    static UPtr open(std::string const& cache_path);

    OptionalValue get(std::string const& key) const;
    OptionalData get_data(std::string const& key) const;
    OptionalMetadata get_metadata(std::string const& key) const;
    bool contains_key(std::string const& key) const;
    int64_t size() const noexcept;
    int64_t size_in_bytes() const noexcept;
    int64_t max_size_in_bytes() const noexcept;
    int64_t disk_size_in_bytes() const;
    CacheDiscardPolicy discard_policy() const noexcept;
    PersistentCacheStats stats() const;

    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value,
             int64_t size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             std::string const& value,
             std::string const& metadata,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    bool put(std::string const& key,
             char const* value,
             int64_t value_size,
             char const* metadata,
             int64_t metadata_size,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());

    typedef std::function<void(std::string const& key, PersistentCache<std::string, std::string, std::string>& cache)>
        Loader;

    OptionalValue get_or_put(std::string const& key, Loader const& load_func);
    OptionalData get_or_put_data(std::string const& key, Loader const& load_func);

    bool put_metadata(std::string const& key, std::string const& metadata);
    bool put_metadata(std::string const& key, char const* metadata, int64_t size);
    OptionalValue take(std::string const& key);
    OptionalData take_data(std::string const& key);
    bool invalidate(std::string const& key);
    void invalidate(std::vector<std::string> const& keys);
    template <typename It>
    void invalidate(It begin, It end);
    void invalidate(std::initializer_list<std::string> const& keys);
    void invalidate();
    bool touch(
        std::string const& key,
        std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    void clear_stats();
    void resize(int64_t size_in_bytes);
    void trim_to(int64_t used_size_in_bytes);
    void compact();

    typedef std::function<void(std::string const& key, CacheEvent ev, PersistentCacheStats const& stats)> EventCallback;

    void set_handler(CacheEvent events, EventCallback cb);

private:
    PersistentCache(std::string const& cache_path, int64_t max_size_in_bytes, CacheDiscardPolicy policy);
    PersistentCache(std::string const& cache_path);

    std::unique_ptr<PersistentStringCache> p_;
};

PersistentCache<std::string, std::string, std::string>::PersistentCache(std::string const& cache_path,
                                                                        int64_t max_size_in_bytes,
                                                                        CacheDiscardPolicy policy)
    : p_(PersistentStringCache::open(cache_path, max_size_in_bytes, policy))
{
}

PersistentCache<std::string, std::string, std::string>::PersistentCache(std::string const& cache_path)
    : p_(PersistentStringCache::open(cache_path))
{
}

typename PersistentCache<std::string, std::string, std::string>::UPtr
    PersistentCache<std::string, std::string, std::string>::open(std::string const& cache_path,
                                                                 int64_t max_size_in_bytes,
                                                                 CacheDiscardPolicy policy)
{
    return PersistentCache<std::string, std::string, std::string>::UPtr(
        new PersistentCache<std::string, std::string, std::string>(cache_path, max_size_in_bytes, policy));
}

typename PersistentCache<std::string, std::string, std::string>::UPtr
    PersistentCache<std::string, std::string, std::string>::open(std::string const& cache_path)
{
    return PersistentCache<std::string, std::string, std::string>::UPtr(
        new PersistentCache<std::string, std::string, std::string>(cache_path));
}

typename PersistentCache<std::string, std::string, std::string>::OptionalValue
    PersistentCache<std::string, std::string, std::string>::get(std::string const& key) const
{
    auto const& svalue = p_->get(key);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

typename PersistentCache<std::string, std::string, std::string>::OptionalData
    PersistentCache<std::string, std::string, std::string>::get_data(std::string const& key) const
{
    auto sdata = p_->get_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

typename PersistentCache<std::string, std::string, std::string>::OptionalMetadata
    PersistentCache<std::string, std::string, std::string>::get_metadata(std::string const& key) const
{
    auto smeta = p_->get_metadata(key);
    return smeta ? OptionalMetadata(*smeta) : OptionalMetadata();
}

bool PersistentCache<std::string, std::string, std::string>::contains_key(std::string const& key) const
{
    return p_->contains_key(key);
}

int64_t PersistentCache<std::string, std::string, std::string>::size() const noexcept
{
    return p_->size();
}

int64_t PersistentCache<std::string, std::string, std::string>::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

int64_t PersistentCache<std::string, std::string, std::string>::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

int64_t PersistentCache<std::string, std::string, std::string>::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

CacheDiscardPolicy PersistentCache<std::string, std::string, std::string>::discard_policy() const noexcept
{
    return p_->discard_policy();
}

PersistentCacheStats PersistentCache<std::string, std::string, std::string>::stats() const
{
    return p_->stats();
}

bool PersistentCache<std::string, std::string, std::string>::put(
    std::string const& key, std::string const& value, std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, expiry_time);
}

bool PersistentCache<std::string, std::string, std::string>::put(
    std::string const& key,
    char const* value,
    int64_t size,
    std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, size, expiry_time);
}

bool PersistentCache<std::string, std::string, std::string>::put(
    std::string const& key,
    std::string const& value,
    std::string const& metadata,
    std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, metadata, expiry_time);
}

bool PersistentCache<std::string, std::string, std::string>::put(
    std::string const& key,
    char const* value,
    int64_t value_size,
    char const* metadata,
    int64_t metadata_size,
    std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->put(key, value, value_size, metadata, metadata_size, expiry_time);
}

typename PersistentCache<std::string, std::string, std::string>::OptionalValue
    PersistentCache<std::string, std::string, std::string>::get_or_put(
        std::string const& key, PersistentCache<std::string, std::string, std::string>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto svalue = p_->get_or_put(key, sload_func);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

typename PersistentCache<std::string, std::string, std::string>::OptionalData
    PersistentCache<std::string, std::string, std::string>::get_or_put_data(
        std::string const& key, PersistentCache<std::string, std::string, std::string>::Loader const& load_func)
{
    auto sload_func = [&](std::string const&, PersistentStringCache const&)
    {
        load_func(key, *this);
    };
    auto sdata = p_->get_or_put_data(key, sload_func);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

bool PersistentCache<std::string, std::string, std::string>::put_metadata(std::string const& key,
                                                                          std::string const& metadata)
{
    return p_->put_metadata(key, metadata);
}

bool PersistentCache<std::string, std::string, std::string>::put_metadata(std::string const& key,
                                                                          char const* metadata,
                                                                          int64_t size)
{
    return p_->put_metadata(key, metadata, size);
}

typename PersistentCache<std::string, std::string, std::string>::OptionalValue
    PersistentCache<std::string, std::string, std::string>::take(std::string const& key)
{
    auto svalue = p_->take(key);
    return svalue ? OptionalValue(*svalue) : OptionalValue();
}

typename PersistentCache<std::string, std::string, std::string>::OptionalData
    PersistentCache<std::string, std::string, std::string>::take_data(std::string const& key)
{
    auto sdata = p_->take_data(key);
    if (!sdata)
    {
        return OptionalData();
    }
    return OptionalData({sdata->value, sdata->metadata});
}

bool PersistentCache<std::string, std::string, std::string>::invalidate(std::string const& key)
{
    return p_->invalidate(key);
}

void PersistentCache<std::string, std::string, std::string>::invalidate(std::vector<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

template <typename It>
void PersistentCache<std::string, std::string, std::string>::invalidate(It begin, It end)
{
    std::vector<std::string> skeys;
    for (auto&& it = begin; it < end; ++it)
    {
        skeys.push_back(*it);
    }
    p_->invalidate(skeys.begin(), skeys.end());
}

void PersistentCache<std::string, std::string, std::string>::invalidate(std::initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

void PersistentCache<std::string, std::string, std::string>::invalidate()
{
    p_->invalidate();
}

bool PersistentCache<std::string, std::string, std::string>::touch(
    std::string const& key, std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return p_->touch(key, expiry_time);
}

void PersistentCache<std::string, std::string, std::string>::clear_stats()
{
    p_->clear_stats();
}

void PersistentCache<std::string, std::string, std::string>::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

void PersistentCache<std::string, std::string, std::string>::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

void PersistentCache<std::string, std::string, std::string>::compact()
{
    p_->compact();
}

void PersistentCache<std::string, std::string, std::string>::set_handler(CacheEvent events, EventCallback cb)
{
    p_->set_handler(events, cb);
}

// @endcond

}  // namespace core
