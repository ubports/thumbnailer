/*
 * Copyright (C) 2016 Canonical Ltd.
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

#include <internal/backoff_adjuster.h>

#include <gtest/gtest.h>

#include <thread>

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(BackoffAdjuster, basic)
{
    BackoffAdjuster ba;

    EXPECT_EQ(0, ba.backoff_period().count());
    EXPECT_EQ(1, ba.min_backoff().count());
    EXPECT_EQ(2, ba.max_backoff().count());
    EXPECT_EQ(chrono::system_clock::time_point(), ba.last_fail_time());

    ba.set_min_backoff(chrono::seconds(20));
    EXPECT_EQ(20, ba.min_backoff().count());

    ba.set_max_backoff(chrono::seconds(40));
    EXPECT_EQ(40, ba.max_backoff().count());

    ba.set_backoff_period(chrono::seconds(40));
    EXPECT_EQ(40, ba.backoff_period().count());

    ba.reset();
    EXPECT_EQ(0, ba.backoff_period().count());
    EXPECT_EQ(chrono::system_clock::time_point(), ba.last_fail_time());

    auto now = chrono::system_clock::now();
    ba.set_last_fail_time(now);
    EXPECT_EQ(now, ba.last_fail_time());
}

TEST(BackoffAdjuster, adjust_retry_limit)
{
    BackoffAdjuster ba;

    EXPECT_TRUE(ba.retry_ok());
    EXPECT_EQ(0, ba.backoff_period().count());
    EXPECT_TRUE(ba.adjust_retry_limit());               // Backoff period changed, must return true.
    EXPECT_EQ(1, ba.backoff_period().count());
    EXPECT_FALSE(ba.adjust_retry_limit());              // Backoff period still the same, must return false.

    this_thread::sleep_for(chrono::milliseconds(1100));

    EXPECT_TRUE(ba.adjust_retry_limit());               // Backoff period changed, must return true.
    EXPECT_EQ(2, ba.backoff_period().count());
    EXPECT_FALSE(ba.adjust_retry_limit());              // Backoff period still the same, must return false.
    EXPECT_FALSE(ba.retry_ok());

    this_thread::sleep_for(chrono::milliseconds(2100));

    EXPECT_TRUE(ba.adjust_retry_limit());               // Backoff period maxed out, must return true.
    EXPECT_EQ(2, ba.backoff_period().count());
    EXPECT_FALSE(ba.adjust_retry_limit());              // Backoff period still the same, must return false.
    EXPECT_FALSE(ba.retry_ok());

    ba.reset();
    EXPECT_FALSE(ba.retry_ok());

    this_thread::sleep_for(chrono::milliseconds(2100));
    EXPECT_TRUE(ba.retry_ok());
}
