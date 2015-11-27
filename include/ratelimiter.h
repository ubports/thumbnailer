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

#pragma once

#include <functional>
#include <map>
#include <memory>

namespace unity
{

namespace thumbnailer
{

// RateLimiter is a simple class to control the level of concurrency
// of asynchronous jobs.  It performs no locking because it is only
// intended to be run from the event loop thread.

class RateLimiter
{
public:
    RateLimiter(int concurrency);
    ~RateLimiter();

    RateLimiter(RateLimiter const&) = delete;
    RateLimiter& operator=(RateLimiter const&) = delete;

    typedef std::function<bool() noexcept> CancelFunc;

    // Schedule a job to run.  If the concurrency limit has not been
    // reached, the job will be run immediately.  Otherwise it will be
    // added to the queue. Return value is a function that, when
    // called, cancels the job in the queue (if it's still in the queue).
    CancelFunc schedule(std::function<void()> job);

    // Schedule a job to run immediately, regardless of the concurrency limit.
    CancelFunc schedule_now(std::function<void()> job);

    // Notify that a job has completed. If there are queued jobs,
    // start the one at the head of the queue. Every call to schedule()
    // (but not schedule_now() *must* be matched by exactly one call to done().
    void done();

private:
    int const concurrency_;  // Max number of outstanding requests.
    int running_;            // Actual number of outstanding requests.
    int next_id_;            // Next available ID.
    // Map of <ID, job> pairs. IDs are integers that increase monotonically.
    // The job with the lowest ID has been waiting the longest.
    std::map<int, std::function<void()>> jobs_;
};

}  // namespace thumbnailer

}  // namespace unity
