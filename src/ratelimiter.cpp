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
#include <QDebug>

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

RateLimiter::RateLimiter(int concurrency, std::string const& name)
    : concurrency_(concurrency)
    , running_(0)
    , name_(name)
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

    if (running_ < concurrency_)
    {
        qDebug() << "scheduling immediately, running:" << running_;
        return schedule_now(job);
    }

    queue_.emplace(make_shared<function<void()>>(move(job)));

    // Returned function clears the job when called, provided the job is still in the queue.
    // done() removes any cleared jobs from the queue without calling them.
    weak_ptr<function<void()>> weak_p(queue_.back());
    return [this, weak_p]() noexcept
    {
        auto job_p = weak_p.lock();
        if (job_p)
        {
            *job_p = nullptr;
        }
        qDebug() << "cancel_func:" << (void*)job_p.get();
        return job_p != nullptr;
    };
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
    if (name_ == "Q")
    {
        qDebug() << "done, running_:" << running_ << "queue:" << queue_.size();
    }
    assert(running_ > 0);
    --running_;

    // Find the next job, discarding any cancelled jobs.
    shared_ptr<function<void()>> job_p;
    while (!queue_.empty())
    {
        job_p = queue_.front();
        assert(job_p);
        qDebug() << "done:" << (void*)job_p.get();
        queue_.pop();
        if (*job_p != nullptr)
        {
            qDebug() << "live job:" << (void*)job_p.get();
            break;
        }
    }

    // If we found an uncancelled job, call it.
    if (job_p && *job_p)
    {
        qDebug() << "calling job:" << (void*)job_p.get();
        schedule_now(*job_p);
    }
}

}  // namespace thumbnailer

}  // namespace unity
