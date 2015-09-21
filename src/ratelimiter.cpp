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
#include <iostream>  // TODO: remove this

using namespace std;

namespace unity
{

namespace thumbnailer
{

RateLimiter::RateLimiter(int concurrency)
    : concurrency_(concurrency)
    , running_(0)
{
    assert(concurrency > 0);
}

RateLimiter::~RateLimiter()
{
    assert(running_ == 0);
}

function<void() noexcept> RateLimiter::schedule(function<void()> job)
{
    assert(job);

    if (running_ < concurrency_)
    {
        running_++;
        job();
        return []{};  // Wasn't queued, so cancel does nothing.
    }

    shared_ptr<function<void()>> job_p(new function<void()>(move(job)));
    queue_.emplace(job_p);

    // Returned function clears job when called, provided the job is still in the queue.
    // done() removes any cleared jobs from the queue without calling them.
    weak_ptr<function<void()>> weak_p(job_p);
    return [weak_p]() noexcept
    {
        auto job_p = weak_p.lock();
        if (job_p)
        {
            *job_p = nullptr;
        }
    };
}

void RateLimiter::done()
{
    // Find the next job, discarding any cancelled jobs.
    shared_ptr<function<void()>> job_p;
    while (!queue_.empty())
    {
        job_p = queue_.front();
        queue_.pop();
        if (*job_p)
        {
            break;
        }
    }

    // If we found an uncancelled job, call it.
    if (job_p && *job_p)
    {
        (*job_p)();
    }
    else if (queue_.empty())
    {
        assert(running_ > 0);
        --running_;
    }
}

}  // namespace thumbnailer

}  // namespace unity
