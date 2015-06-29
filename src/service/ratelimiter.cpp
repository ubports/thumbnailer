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

void RateLimiter::schedule(std::function<void()> job)
{
    if (running_ < concurrency_)
    {
        running_++;
        job();
    }
    else
    {
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
        auto job = queue_.front();
        queue_.pop();
        job();
    }
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
