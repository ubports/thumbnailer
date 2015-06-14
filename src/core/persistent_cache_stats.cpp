/*
 * Copyright (C) 2015 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <core/persistent_cache_stats.h>

#include <core/internal/persistent_string_cache_stats.h>

using namespace std;

namespace core
{

PersistentCacheStats::PersistentCacheStats()
    : p_(make_shared<internal::PersistentStringCacheStats>())
    , internal_(false)
{
}

/// @cond

// So we can point at the cache's internal instance, which avoids a copy.

PersistentCacheStats::PersistentCacheStats(shared_ptr<internal::PersistentStringCacheStats> const& p) noexcept
    : p_(p)
    , internal_(true)
{
}

/// @endcond

PersistentCacheStats::PersistentCacheStats(PersistentCacheStats const& other)
    : internal_(false)
{
    if (other.internal_)
    {
        // Source is pointing at the internal instance, so we make a copy here to de-couple from it.
        p_ = make_shared<internal::PersistentStringCacheStats>(*other.p_);
    }
    else
    {
        p_ = other.p_;  // Class is read-only, so we can just point at the same impl.
    }
}

PersistentCacheStats::PersistentCacheStats(PersistentCacheStats&& other) noexcept
    : internal_(false)
{
    // The cache always passes the internal instance to the event
    // handler by const ref, so it is not possible to move construct from it.
    assert(!other.internal_);

    p_ = other.p_;  // Move must leave the instance in a usable state, so we just copy the shared_ptr.
}

PersistentCacheStats& PersistentCacheStats::operator=(PersistentCacheStats const& rhs)
{
    if (this != &rhs)
    {
        if (rhs.internal_)
        {
            // Source is pointing at the internal instance, so we make a copy here to de-couple from it.
            p_ = make_shared<internal::PersistentStringCacheStats>(*rhs.p_);
            internal_ = false;
        }
        else
        {
            assert(internal_ == false);
            p_ = rhs.p_;  // Class is read-only, so we can just point at the same impl.
        }
    }
    return *this;
}

PersistentCacheStats& PersistentCacheStats::operator=(PersistentCacheStats&& rhs)
{
    if (rhs.internal_)
    {
        // Source is pointing at the internal instance, so we make a copy here to de-couple from it.
        p_ = make_shared<internal::PersistentStringCacheStats>(*rhs.p_);
        internal_ = false;
    }
    else
    {
        assert(internal_ == false);
        p_ = rhs.p_; // Move must leave the instance in a usable state, so we just copy the shared_ptr.
    }
    return *this;
}

string PersistentCacheStats::cache_path() const
{
    return p_->cache_path_;
}

CacheDiscardPolicy PersistentCacheStats::policy() const noexcept
{
    return p_->policy_;
}

int64_t PersistentCacheStats::size() const noexcept
{
    return p_->num_entries_;
}

int64_t PersistentCacheStats::size_in_bytes() const noexcept
{
    return p_->cache_size_;
}

int64_t PersistentCacheStats::max_size_in_bytes() const noexcept
{
    return p_->max_cache_size_;
}

int64_t PersistentCacheStats::hits() const noexcept
{
    return p_->hits_;
}

int64_t PersistentCacheStats::misses() const noexcept
{
    return p_->misses_;
}

int64_t PersistentCacheStats::hits_since_last_miss() const noexcept
{
    return p_->hits_since_last_miss_;
}

int64_t PersistentCacheStats::misses_since_last_hit() const noexcept
{
    return p_->misses_since_last_hit_;
}

int64_t PersistentCacheStats::longest_hit_run() const noexcept
{
    return p_->longest_hit_run_;
}

int64_t PersistentCacheStats::longest_miss_run() const noexcept
{
    return p_->longest_miss_run_;
}

int64_t PersistentCacheStats::ttl_evictions() const noexcept
{
    return p_->ttl_evictions_;
}

int64_t PersistentCacheStats::lru_evictions() const noexcept
{
    return p_->lru_evictions_;
}

chrono::system_clock::time_point PersistentCacheStats::most_recent_hit_time() const noexcept
{
    return p_->most_recent_hit_time_;
}

chrono::system_clock::time_point PersistentCacheStats::most_recent_miss_time() const noexcept
{
    return p_->most_recent_miss_time_;
}

chrono::system_clock::time_point PersistentCacheStats::longest_hit_run_time() const noexcept
{
    return p_->longest_hit_run_time_;
}

chrono::system_clock::time_point PersistentCacheStats::longest_miss_run_time() const noexcept
{
    return p_->longest_miss_run_time_;
}

PersistentCacheStats::Histogram const& PersistentCacheStats::histogram() const noexcept
{
    return p_->hist_;
}

PersistentCacheStats::HistogramBounds const& PersistentCacheStats::histogram_bounds() noexcept
{
    static HistogramBounds bounds = []()
    {
        HistogramBounds b;
        b.push_back({1, 9});  // First bin collapses 1 - 9 into a single bin.

        unsigned i = 1;
        auto constexpr num_powers = (PersistentCacheStats::NUM_BINS - 2) / 9;
        for (; i <= num_powers; ++i)
        {
            for (unsigned int j = 1; j < 10; ++j)
            {
                uint64_t lower = pow(10, i) * j;
                uint64_t upper = lower + pow(10, i) - 1;
                b.push_back({lower, upper});
            }
        }
        b.push_back({pow(10, i), std::numeric_limits<decltype(b[0].second)>::max()});
        return b;
    }();
    return bounds;
}

}  // namespace core
