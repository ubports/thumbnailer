/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of version 3 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 */

#include "ratelimiter.h"

namespace unity
{

namespace thumbnailer
{

namespace service
{

RateLimiter::RateLimiter(int concurrency)
    : concurrency_(concurrency)
    , running_(0)
{
}

RateLimiter::~RateLimiter() = default;

void RateLimiter::schedule(std::function<void()> job, std::function<void()> job_started)
{
    if (running_ < concurrency_)
    {
        job_started();
        running_++;
        job();
    }
    else
    {
        job_started_ = job_started;
        queue_.push(job);
    }
}

void RateLimiter::done()
{
    if (queue_.empty())
    {
        running_--;
    }
    else
    {
        job_started_();
        auto job = queue_.front();
        queue_.pop();
        job();
    }
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
