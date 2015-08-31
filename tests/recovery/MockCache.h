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

#include <core/cache_discard_policy.h>
#include <core/optional.h>
#include <core/persistent_cache_stats.h>

#include <gmock/gmock.h>

#include <chrono>
#include <system_error>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace testing
{

class MockCache
{
public:
    typedef std::unique_ptr<MockCache> UPtr;

    static UPtr open(std::string const& cache_path,
                     int64_t /* max_size_in_bytes */,
                     core::CacheDiscardPolicy /* policy */);

    static UPtr open(std::string const& cache_path);

    MockCache(std::string const& cache_path);

    MOCK_METHOD1(get, core::Optional<std::string>(std::string const& key));

    MOCK_METHOD1(resize, void(int64_t size_in_bytes));  // Needed so template will instantiate in caller.

    // Methods below are not Google mocks because the recovery logic reinitializes
    // the cache, thereby replacing the original mock with a new one, and we can't
    // set expectations on that second instance.

    void invalidate();  // Throws system_error(666) on first call, then int(42).

    void compact();  // Throws system_error(666) on first call, succeeds on subsequent calls.

    // Throws system_error(666) on first call, then runtime_error.
    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time
                 = std::chrono::system_clock::time_point());

private:
    std::string path_;
};

}  // namespace testing

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
