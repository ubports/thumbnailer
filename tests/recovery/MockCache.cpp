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

#include "MockCache.h"

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace testing
{

MockCache::UPtr MockCache::open(std::string const& cache_path,
                                int64_t /* max_size_in_bytes */,
                                core::CacheDiscardPolicy /* policy */)
{
    static int first = false;
    first = !first;
    if (cache_path == "throw_std_exception")
    {
        if (first)
        {
            throw std::runtime_error("testing std exception");
        }
    }
    else if (cache_path == "throw_unknown_exception")
    {
        if (first)
        {
            throw 99;
        }
    }
    return UPtr(new MockCache(cache_path));
}

MockCache::UPtr MockCache::open(std::string const& cache_path)
{
    return UPtr(new MockCache(cache_path));
}

MockCache::MockCache(std::string const& cache_path)
    : path_(cache_path)
{
}

// Throws system_error(666) on first call, then int(42).
void MockCache::invalidate()
{
    static bool first = false;
    first = !first;
    if (first)
    {
        throw std::system_error(std::error_code(666, std::generic_category()), "corrupt DB");
    }
    else
    {
        throw 42;
    }
}

// Throws system_error(666) on first call, succeeds on subsequent calls.
void MockCache::compact()
{
    static bool once = false;
    if (!once)
    {
        once = true;
        throw std::system_error(std::error_code(666, std::generic_category()), "corrupt DB");
    }
}

// Throws system_error(666) on first call, then runtime_error.
bool MockCache::put(std::string const& /* key */,
                    std::string const& /* value */,
                    std::chrono::time_point<std::chrono::system_clock> /* expiry_time */)
{
    static bool once = false;
    if (!once)
    {
        once = true;
        throw std::system_error(std::error_code(666, std::generic_category()), "corrupt DB");
    }
    throw std::runtime_error("bang");
}

}  // namespace testing

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
