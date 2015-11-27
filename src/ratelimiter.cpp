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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 *              Michi Henning <michi.henning@canonical.com>
 */

#include "ratelimiter.h"

#include <cassert>

using namespace std;

namespace unity
{

namespace thumbnailer
{

RateLimiter::RateLimiter(int concurrency)
    : concurrency_(concurrency)
    , running_(0)
    , next_id_(0)
{
    assert(concurrency > 0);
}

RateLimiter::~RateLimiter()
{
    // No assert here because this code is linked by the calling application.
    // If the application terminates without waiting for outstanding requests
    // to finish, we don't want to cause a core dump.
    // assert(running_ == 0);
}

RateLimiter::CancelFunc RateLimiter::schedule(function<void()> job)
{
    assert(job);
    assert (running_ >= 0);
    assert (running_ <= concurrency_);

    int id = next_id_++;

    if (running_ < concurrency_)
    {
        return schedule_now(move(job));
    }

    jobs_.emplace_hint(jobs_.end(), id, move(job));

    // Returned function erases the job when called, provided the job is still in the map.
    auto cancel_func = [this, id]() noexcept
    {
        auto it = jobs_.find(id);
        bool found = it != jobs_.end();
        if (found)
        {
            jobs_.erase(it);
        }
        return found;
    };

    return cancel_func;
}

RateLimiter::CancelFunc RateLimiter::schedule_now(function<void()> job)
{
    assert(job);

    ++running_;
    job();
    return []{ return false; };  // Wasn't queued, so cancel does nothing.
}

void RateLimiter::done()
{
    assert(running_ > 0);
    --running_;

    auto it = jobs_.begin();  // Find the oldest job.
    if (it != jobs_.end())
    {
        auto job = move(it->second);
        jobs_.erase(it);
        schedule_now(job);
    }
}

}  // namespace thumbnailer

}  // namespace unity
