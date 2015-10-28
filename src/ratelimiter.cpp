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

RateLimiter::RateLimiter(int concurrency, string const& name)
    : concurrency_(concurrency)
    , name_(name)
    , running_(0)
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

//RateLimiter::CancelFunc RateLimiter::schedule(function<void()> job)
RateLimiter::CancelFunc RateLimiter::schedule(function<void()> job)
{
    assert(job);
    assert (running_ >= 0);

    if (running_ < concurrency_)
    {
        return schedule_now(job);
    }

    cerr << name_ << ": queued, s: " << queue_.size() << ", r: " << running_ << endl;
    queue_.emplace(make_shared<function<void()>>(move(job)));

    // Returned function clears the job when called, provided the job is still in the queue.
    // done() removes any cleared jobs from the queue without calling them.
    weak_ptr<function<void()>> weak_p(queue_.back());
    return [this, weak_p]() noexcept
    {
        auto job_p = weak_p.lock();
        if (job_p)
        {
            cerr << name_ << ": cancelled, s: " << queue_.size() << ", r: " << running_ << endl;
            *job_p = nullptr;
        }
    };
}

RateLimiter::CancelFunc RateLimiter::schedule_now(function<void()> job)
{
    assert(job);

    running_++;
    cerr << name_ << ": scheduled, s: " << queue_.size() << ", r: " << running_ << endl;
    job();
    return []{};  // Wasn't queued, so cancel does nothing.
}

void RateLimiter::done()
{
    // Find the next job, discarding any cancelled jobs.
    shared_ptr<function<void()>> job_p;
    while (!queue_.empty())
    {
        job_p = queue_.front();
        assert(job_p);
        queue_.pop();
        if (*job_p != nullptr)
        {
            cerr << name_ << ": found request, s: " << queue_.size() << ", r: " << running_ << endl;
            break;
        }
        else
        {
            cerr << name_ << ": removed, s: " << queue_.size() << ", r: " << running_ << endl;
        }
    }

    // If we found an uncancelled job, call it.
    if (job_p && *job_p)
    {
        cerr << name_ << ": calling job, s: " << queue_.size() << ", r: " << running_ << endl;
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
