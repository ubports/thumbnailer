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
 * Authored by: Michi Henning <michi@canonical.com>
 */

#pragma once

#include <chrono>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class BackoffAdjuster final
{
public:
    BackoffAdjuster() noexcept;

    BackoffAdjuster(BackoffAdjuster const&) = delete;
    BackoffAdjuster& operator=(BackoffAdjuster const&) = delete;

    BackoffAdjuster(BackoffAdjuster&&) = default;
    BackoffAdjuster& operator=(BackoffAdjuster&&) = default;

    std::chrono::system_clock::time_point last_fail_time() const noexcept;
    BackoffAdjuster& set_last_fail_time(std::chrono::system_clock::time_point fail_time) noexcept;

    std::chrono::seconds backoff_period() const noexcept;
    BackoffAdjuster& set_backoff_period(std::chrono::seconds backoff_period) noexcept;

    std::chrono::seconds min_backoff() const noexcept;
    BackoffAdjuster& set_min_backoff(std::chrono::seconds min_backoff) noexcept;

    std::chrono::seconds max_backoff() const noexcept;
    BackoffAdjuster& set_max_backoff(std::chrono::seconds max_backoff) noexcept;

    bool retry_ok() const noexcept;      // Returns true if it's OK to send another request now.
    bool adjust_retry_limit() noexcept;  // Informs adjuster that a request had a temporary failure.
    void reset() noexcept;               // Informs adjuster that a request succeeded.

private:
    std::chrono::system_clock::time_point last_fail_time_;
    std::chrono::seconds backoff_period_;  // Zero while in success mode, non-zero otherwise.
    std::chrono::seconds min_backoff_;
    std::chrono::seconds max_backoff_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
