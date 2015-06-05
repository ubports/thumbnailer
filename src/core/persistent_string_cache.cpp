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

// @cond

#include <core/persistent_string_cache.h>

#include <core/internal/persistent_string_cache_impl.h>
#include <core/persistent_cache_stats.h>

using namespace std;

namespace core
{

PersistentStringCache::PersistentStringCache(string const& cache_path,
                                             int64_t max_size_in_bytes,
                                             CacheDiscardPolicy policy)
    : p_(new internal::PersistentStringCacheImpl(cache_path, max_size_in_bytes, policy, this))
{
}

PersistentStringCache::PersistentStringCache(string const& cache_path)
    : p_(new internal::PersistentStringCacheImpl(cache_path, this))
{
}

PersistentStringCache::PersistentStringCache(PersistentStringCache&&) = default;
PersistentStringCache& PersistentStringCache::operator=(PersistentStringCache&&) = default;

PersistentStringCache::~PersistentStringCache() = default;

PersistentStringCache::UPtr PersistentStringCache::open(string const& cache_path,
                                                        int64_t max_size_in_bytes,
                                                        CacheDiscardPolicy policy)
{
    return PersistentStringCache::UPtr(new PersistentStringCache(cache_path, max_size_in_bytes, policy));
}

PersistentStringCache::UPtr PersistentStringCache::open(string const& cache_path)
{
    return PersistentStringCache::UPtr(new PersistentStringCache(cache_path));
}

Optional<string> PersistentStringCache::get(string const& key) const
{
    string value;
    return p_->get(key, value) ? Optional<string>(move(value)) : Optional<string>();
}

Optional<PersistentStringCache::Data> PersistentStringCache::get_data(string const& key) const
{
    string value;
    string metadata;
    return p_->get(key, value, &metadata) ? Optional<Data>(move(Data{move(value), move(metadata)})) : Optional<Data>();
}

Optional<string> PersistentStringCache::get_metadata(string const& key) const
{
    string metadata;
    return p_->get_metadata(key, metadata) ? Optional<string>(move(metadata)) : Optional<string>();
}

bool PersistentStringCache::contains_key(string const& key) const
{
    return p_->contains_key(key);
}

int64_t PersistentStringCache::size() const noexcept
{
    return p_->size();
}

int64_t PersistentStringCache::size_in_bytes() const noexcept
{
    return p_->size_in_bytes();
}

int64_t PersistentStringCache::max_size_in_bytes() const noexcept
{
    return p_->max_size_in_bytes();
}

int64_t PersistentStringCache::disk_size_in_bytes() const
{
    return p_->disk_size_in_bytes();
}

CacheDiscardPolicy PersistentStringCache::discard_policy() const noexcept
{
    return p_->discard_policy();
}

PersistentCacheStats PersistentStringCache::stats() const
{
    return p_->stats();
}

bool PersistentStringCache::put(string const& key,
                                string const& value,
                                chrono::time_point<chrono::system_clock> expiry_time)
{
    return p_->put(key, value.data(), value.size(), nullptr, 0, expiry_time);
}

bool PersistentStringCache::put(string const& key,
                                char const* value,
                                int64_t size,
                                chrono::time_point<chrono::system_clock> expiry_time)
{
    return p_->put(key, value, size, nullptr, 0, expiry_time);
}

bool PersistentStringCache::put(string const& key,
                                string const& value,
                                string const& metadata,
                                chrono::time_point<chrono::system_clock> expiry_time)
{
    return p_->put(key, value.data(), value.size(), metadata.data(), metadata.size(), expiry_time);
}

bool PersistentStringCache::put(string const& key,
                                char const* value,
                                int64_t value_size,
                                char const* metadata,
                                int64_t metadata_size,
                                chrono::time_point<chrono::system_clock> expiry_time)
{
    return p_->put(key, value, value_size, metadata, metadata_size, expiry_time);
}

Optional<string> PersistentStringCache::get_or_put(
    string const& key, PersistentStringCache::Loader const& load_func)
{
    string value;
    bool found = p_->get_or_put(key, value, load_func);
    return found ? Optional<string>(move(value)) : Optional<string>();
}

Optional<PersistentStringCache::Data> PersistentStringCache::get_or_put_data(
    string const& key, PersistentStringCache::Loader const& load_func)
{
    string value;
    string metadata;
    return p_->get_or_put(key, value, &metadata, load_func) ? Optional<Data>(Data{move(value), move(metadata)}) :
                                                              Optional<Data>();
}

bool PersistentStringCache::put_metadata(string const& key, string const& metadata)
{
    return p_->put_metadata(key, metadata.data(), metadata.size());
}

bool PersistentStringCache::put_metadata(string const& key, char const* metadata, int64_t size)
{
    return p_->put_metadata(key, metadata, size);
}

Optional<string> PersistentStringCache::take(string const& key)
{
    string value;
    return p_->take(key, value) ? Optional<string>(move(value)) : Optional<string>();
}

Optional<PersistentStringCache::Data> PersistentStringCache::take_data(string const& key)
{
    string value;
    string metadata;
    return p_->take(key, value, &metadata) ? Optional<Data>(move(Data{move(value), move(metadata)})) : Optional<Data>();
}

bool PersistentStringCache::invalidate(string const& key)
{
    return p_->invalidate(key);
}

void PersistentStringCache::invalidate(vector<string> const& keys)
{
    p_->invalidate(keys);
}

void PersistentStringCache::invalidate(initializer_list<std::string> const& keys)
{
    invalidate(keys.begin(), keys.end());
}

void PersistentStringCache::invalidate()
{
    p_->invalidate();
}

bool PersistentStringCache::touch(string const& key, chrono::time_point<chrono::system_clock> expiry_time)
{
    return p_->touch(key, expiry_time);
}

void PersistentStringCache::clear_stats()
{
    p_->clear_stats();
}

void PersistentStringCache::resize(int64_t size_in_bytes)
{
    p_->resize(size_in_bytes);
}

void PersistentStringCache::trim_to(int64_t used_size_in_bytes)
{
    p_->trim_to(used_size_in_bytes);
}

void PersistentStringCache::compact()
{
    p_->compact();
}

void PersistentStringCache::set_handler(CacheEvent events, EventCallback cb)
{
    p_->set_handler(events, cb);
}

}  // namespace core

// @endcond
