/*
 * Copyright (C) 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#pragma once

#include <core/persistent_string_cache.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Helper class to wrap access to a persistent cache. We use this
// to handle database corruption: if the DB reports that it is corrupt
// (std::system error with code 666), we delete the cache files and
// re-create the cache.
// The constructor also deals with caches that are re-sized when opened.

class CacheHelper final
{
public:
    CacheHelper(std::string const& cache_path,
                int64_t max_size_in_bytes,
                core::CacheDiscardPolicy policy);

    core::Optional<std::string> get(std::string const& key) const;
    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    core::PersistentCacheStats stats() const;
    void clear_stats();
    void invalidate();
    void compact();

private:
    template<typename T>
    T call(std::function<T(void)> func) const;

    void call(std::function<void(void)> func) const;

    void recover() const;
    void init_cache();

    std::string path_;
    int64_t size_;
    core::CacheDiscardPolicy policy_;
    mutable std::unique_ptr<core::PersistentStringCache> c_;
};

// Simple call wrapper to allow us to call a functor and
// check if the called function threw an exception. If so, we re-try
// the call after calling recover(). In turn, if the exception
// indicates DB corruption, it deletes the cache, re-initializes it,
// and re-tries the function call one more time.

template<typename T>
T CacheHelper::call(std::function<T(void)> func) const
{
    try
    {
        return func();  // Try and call the passed function.
    }
    catch (...)
    {
        recover();      // If the DB is corrupt, recover() wipes the DB. If not, it re-throws.
        return func();  // Try again with the recovered DB.
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
