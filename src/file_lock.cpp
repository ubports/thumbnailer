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

#include <internal/file_lock.h>

#include <unity/UnityExceptions.h>

#include <thread>

#include <sys/file.h>
#include <unistd.h>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

int const AdvisoryFileLock::sleep_interval;

AdvisoryFileLock::AdvisoryFileLock(string const& path)
    : path_(path)
    , locked_(false)
{
    if ((fd_ = open(path_.c_str(), O_CREAT, 0444)) == -1)
    {
        throw FileException("AdvisoryFileLock::lock(): cannot open " + path_, errno);
    }
}

bool AdvisoryFileLock::lock(chrono::milliseconds msecs)
{
    if (locked_)
    {
        throw LogicException("AdvisoryFileLock::lock(): locked already: " + path_);
    }

    auto mode = LOCK_EX;
    if (msecs != chrono::milliseconds::zero())
    {
        mode |= LOCK_NB;
    }
    int64_t remaining_time = msecs.count();
    int rc;
    do
    {
        if ((rc = flock(fd_, mode)) == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                this_thread::sleep_for(chrono::milliseconds(sleep_interval));
                remaining_time -= sleep_interval;
            }
            else
            {
                throw FileException("AdvisoryFileLock::lock(): flock failed", errno);  // LCOV_EXCL_LINE
            }
        }
    }
    while (rc == -1 && remaining_time > 0);
    locked_ = rc != -1;
    return locked_;
}

void AdvisoryFileLock::unlock()
{
    if (!locked_)
    {
        throw LogicException("AdvisoryFileLock::unlock(): unlocked already: " + path_);
    }
    if (flock(fd_, LOCK_UN) == -1)
    {
        throw FileException("AdvisoryFileLock::unlock(): cannot unlock " + path_, errno);  // LCOV_EXCL_LINE
    }
    locked_ = false;
}

AdvisoryFileLock::~AdvisoryFileLock()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
