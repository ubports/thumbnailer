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

#include <gtest/gtest.h>

using namespace std;
using namespace unity::thumbnailer::internal;

string const lockfile = "./lock_file";

TEST(file_lock, basic)
{
    {
        AdvisoryFileLock lock(lockfile);
    }

    {
        AdvisoryFileLock lock(lockfile);
        EXPECT_TRUE(lock.lock());
        lock.unlock();
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

TEST(file_lock, timeout_fail)
{
    if (!SLOW_TESTS)
    {
        return;
    }
    system("./hold_lock &");  // Holds lock for three seconds
    sleep(1);
    AdvisoryFileLock lock(lockfile);
    EXPECT_FALSE(lock.lock(chrono::milliseconds(1000)));
}

TEST(file_lock, timeout_success)
{
    if (!SLOW_TESTS)
    {
        return;
    }
    system("./hold_lock &");  // Holds lock for three seconds
    sleep(1);
    AdvisoryFileLock lock(lockfile);
    EXPECT_TRUE(lock.lock(chrono::milliseconds(5000)));
}

#pragma GCC diagnostic pop

TEST(file_lock, exceptions)
{
    {
        AdvisoryFileLock lock(lockfile);
        EXPECT_TRUE(lock.lock());
        try
        {
            lock.lock(chrono::milliseconds());
            FAIL();
        }
        catch (unity::LogicException const& e)
        {
            EXPECT_STREQ("unity::LogicException: AdvisoryFileLock::lock(): locked already: ./lock_file", e.what());
        }
    }

    // Lock released by the destructor from previous test, so must be able to get it back again.
    {
        AdvisoryFileLock lock(lockfile);
        EXPECT_TRUE(lock.lock());

        // Unlocking twice must fail.
        lock.unlock();
        try
        {
            lock.unlock();
            FAIL();
        }
        catch (unity::LogicException const& e)
        {
            EXPECT_STREQ("unity::LogicException: AdvisoryFileLock::unlock(): unlocked already: ./lock_file", e.what());
        }
    }

    {
        try
        {
            AdvisoryFileLock lock("/no_such_dir/xyz");
        }
        catch (unity::FileException const& e)
        {
            EXPECT_STREQ("unity::FileException: AdvisoryFileLock::lock(): cannot open /no_such_dir/xyz (errno = 2)",
                         e.what());
        }
    }
}

int main(int argc, char** argv)
{
    setenv("LC_ALL", "C", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
