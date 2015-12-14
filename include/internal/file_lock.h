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

#include <chrono>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class AdvisoryFileLock final
{
public:
    AdvisoryFileLock(std::string const& path);
    AdvisoryFileLock(AdvisoryFileLock const&) = delete;
    AdvisoryFileLock& operator=(AdvisoryFileLock const&) = delete;
    AdvisoryFileLock(AdvisoryFileLock&&) = default;
    AdvisoryFileLock& operator=(AdvisoryFileLock&&) = default;
    ~AdvisoryFileLock();

    static int const sleep_interval = 100;

    // Returns true if lock can be acquired within msecs, false otherwise.
    // Pass milliseconds::zero() to wait indefinitely.
    // Throws if lock is already held by the calling process.
    // Wait is implemented as a busy-wait loop, retrying every sleep_interval milliseconds.
    bool lock(std::chrono::milliseconds msecs = std::chrono::milliseconds::zero());

    // Throws if lock is not held by the calling process.
    void unlock();

private:
    std::string path_;
    bool locked_;
    int fd_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace service
