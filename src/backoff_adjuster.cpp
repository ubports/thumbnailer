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

#include <algorithm>
#include <cassert>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

BackoffAdjuster::BackoffAdjuster() noexcept
    : backoff_period_(chrono::seconds(0))
    , min_backoff_(chrono::seconds(1))
    , max_backoff_(min_backoff_ * 2)
{
}

chrono::system_clock::time_point BackoffAdjuster::last_fail_time() const noexcept
{
    return last_fail_time_;
}

BackoffAdjuster& BackoffAdjuster::set_last_fail_time(chrono::system_clock::time_point fail_time) noexcept
{
    last_fail_time_ = fail_time;
    return *this;
}

chrono::seconds BackoffAdjuster::backoff_period() const noexcept
{
    return backoff_period_;
}

BackoffAdjuster& BackoffAdjuster::set_backoff_period(chrono::seconds backoff_period) noexcept
{
    assert(backoff_period <= max_backoff_);
    backoff_period_ = backoff_period;
    return *this;
}

chrono::seconds BackoffAdjuster::min_backoff() const noexcept
{
    return min_backoff_;
}

BackoffAdjuster& BackoffAdjuster::set_min_backoff(chrono::seconds min_backoff) noexcept
{
    assert(min_backoff.count() > 0);
    min_backoff_ = min_backoff;
    return *this;
}

chrono::seconds BackoffAdjuster::max_backoff() const noexcept
{
    return max_backoff_;
}

BackoffAdjuster& BackoffAdjuster::set_max_backoff(chrono::seconds max_backoff) noexcept
{
    assert(max_backoff.count() > 0);
    assert(max_backoff.count() >= 2 * min_backoff_.count());
    max_backoff_ = max_backoff;
    return *this;
}

bool BackoffAdjuster::retry_ok() const noexcept
{
    if (backoff_period_.count() == 0)
    {
        return true;
    }
    auto now = chrono::system_clock::now();
    return now > last_fail_time_ + backoff_period_;
}

// Caller calls this every time there is a temporary failure. We adjust
// the backoff period for exponential backoff. Whenever the backoff period
// changes or if the backoff period is maxed out, return true, false otherwise.

bool BackoffAdjuster::adjust_retry_limit() noexcept
{
    auto now = chrono::system_clock::now();
    if (backoff_period_.count() == 0)
    {
        // We are transitioning from "no failure" to "temporary failure".
        backoff_period_ = min_backoff_;
        last_fail_time_ = now;
        return true;
    }
    if (now > last_fail_time_ + backoff_period_)
    {
        // More time than the backoff period has elapsed since the
        // last failure, so we double the backoff period (up to the maximum)
        // and remember the time of this failure.
        backoff_period_ = min(2 * backoff_period_, max_backoff_);
        last_fail_time_ = now;
        return true;
    }
    // We are in failure mode, but haven't exceeded the current backoff period yet.
    return false;
}

// Reset the retry limit, but only if the current backoff_period_ (if any) has expired.
// We do not reset while the current period is still in effect because the thumbnailer
// schedules requests from a thread pool, which means that they can complete out of order.
// We need to prevent a false reset when we schedule A followed by B, but then B completes
// with an error followed by A reporting success.

void BackoffAdjuster::reset() noexcept
{
    if (backoff_period_.count() != 0)
    {
        auto now = chrono::system_clock::now();
        if (now > last_fail_time_ + backoff_period_)
        {
            backoff_period_ = chrono::seconds(0);
            last_fail_time_ = chrono::system_clock::time_point();
        }
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
