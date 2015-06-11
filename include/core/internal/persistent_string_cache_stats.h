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

#pragma once

#include <core/cache_discard_policy.h>
#include <core/persistent_cache_stats.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iostream>  // TODO: remove this

namespace core
{

namespace internal
{

// Simple stats class to keep track of accesses.

class PersistentStringCacheStats
{
public:
    PersistentStringCacheStats() noexcept
        : policy_(CacheDiscardPolicy::lru_only)
        , num_entries_(0)
        , cache_size_(0)
        , max_cache_size_(0)
        , state_(Initialized)
    {
        clear();
        hist_.resize(PersistentCacheStats::NUM_BINS, 0);
    }

    PersistentStringCacheStats(PersistentStringCacheStats const&) = default;
    PersistentStringCacheStats(PersistentStringCacheStats&&) = default;
    PersistentStringCacheStats& operator=(PersistentStringCacheStats const&) = default;
    PersistentStringCacheStats& operator=(PersistentStringCacheStats&&) = default;

    std::string cache_path_;           // Immutable
    core::CacheDiscardPolicy policy_;  // Immutable
    int64_t num_entries_;
    int64_t cache_size_;
    int64_t max_cache_size_;

    // Values below are reset by a call to clear().
    int64_t hits_;
    int64_t misses_;
    int64_t hits_since_last_miss_;
    int64_t misses_since_last_hit_;
    int64_t longest_hit_run_;
    int64_t longest_miss_run_;
    int64_t ttl_evictions_;
    int64_t lru_evictions_;
    std::chrono::system_clock::time_point most_recent_hit_time_;
    std::chrono::system_clock::time_point most_recent_miss_time_;
    std::chrono::system_clock::time_point longest_hit_run_time_;
    std::chrono::system_clock::time_point longest_miss_run_time_;
    PersistentCacheStats::Histogram hist_;

    enum State
    {
        Initialized,
        LastAccessWasHit,
        LastAccessWasMiss
    };
    State state_;

    void inc_hits()
    {
        ++hits_since_last_miss_;
        ++hits_;
        most_recent_hit_time_ = std::chrono::system_clock::now();
        misses_since_last_hit_ = 0;
        if (state_ != LastAccessWasHit)
        {
            state_ = LastAccessWasHit;
            misses_since_last_hit_ = 0;
        }
        if (state_ != LastAccessWasMiss && hits_since_last_miss_ > longest_hit_run_)
        {
            longest_hit_run_ = hits_since_last_miss_;
            longest_hit_run_time_ = most_recent_hit_time_;
        }
    }

    void inc_misses()
    {
        ++misses_since_last_hit_;
        ++misses_;
        most_recent_miss_time_ = std::chrono::system_clock::now();
        hits_since_last_miss_ = 0;
        if (state_ != LastAccessWasMiss)
        {
            state_ = LastAccessWasMiss;
            hits_since_last_miss_ = 0;
        }
        if (state_ != LastAccessWasHit && misses_since_last_hit_ > longest_miss_run_)
        {
            longest_miss_run_ = misses_since_last_hit_;
            longest_miss_run_time_ = most_recent_miss_time_;
        }
    }

    void hist_decrement(int64_t size)
    {
        assert(size > 0);
        --hist_[size_to_index(size)];
    }

    void hist_increment(int64_t size)
    {
        assert(size > 0);
        ++hist_[size_to_index(size)];
    }

    void hist_clear()
    {
        memset(&hist_[0], 0, hist_.size() * sizeof(PersistentCacheStats::Histogram::value_type));
    }

    void clear() noexcept
    {
        hits_ = 0;
        misses_ = 0;
        hits_since_last_miss_ = 0;
        misses_since_last_hit_ = 0;
        longest_hit_run_ = 0;
        longest_miss_run_ = 0;
        ttl_evictions_ = 0;
        lru_evictions_ = 0;
        most_recent_hit_time_ = std::chrono::system_clock::time_point();
        most_recent_miss_time_ = std::chrono::system_clock::time_point();
        longest_hit_run_time_ = std::chrono::system_clock::time_point();
        longest_miss_run_time_ = std::chrono::system_clock::time_point();
    }

    // Serialize the ephemeral part of the stats (counters and time stamps), but not histogram.

    std::string serialize()
    {
        using namespace std;
        using namespace std::chrono;

        ostringstream os;
        os << hits_ << " "
           << misses_ << " "
           << hits_since_last_miss_ << " "
           << misses_since_last_hit_ << " "
           << longest_hit_run_ << " "
           << ttl_evictions_ << " "
           << lru_evictions_ << " "
           << duration_cast<milliseconds>(most_recent_hit_time_.time_since_epoch()).count() << " "
           << duration_cast<milliseconds>(most_recent_miss_time_.time_since_epoch()).count() << " "
           << duration_cast<milliseconds>(longest_hit_run_time_.time_since_epoch()).count() << " "
           << duration_cast<milliseconds>(longest_miss_run_time_.time_since_epoch()).count();
        return os.str();
    }

    // De-serialize the ephemeral part of the stats (counters and time stamps), but not histogram.

    void deserialize(const std::string& s)
    {
        using namespace std;
        using namespace std::chrono;

        istringstream is(s);
        int64_t mrht;
        int64_t mrmt;
        int64_t lhrt;
        int64_t lmrt;
        is >> hits_
           >> misses_
           >> hits_since_last_miss_
           >> misses_since_last_hit_
           >> longest_hit_run_
           >> ttl_evictions_
           >> lru_evictions_
           >> mrht
           >> mrmt
           >> lhrt
           >> lmrt;
        assert(!is.bad());
        most_recent_hit_time_ = system_clock::time_point(milliseconds(mrht));
        most_recent_miss_time_ = system_clock::time_point(milliseconds(mrmt));
        longest_hit_run_time_ = system_clock::time_point(milliseconds(lhrt));
        longest_miss_run_time_ = system_clock::time_point(milliseconds(lmrt));
    }

private:
    unsigned size_to_index(int64_t size)
    {
        using namespace std;
        assert(size > 0);
        unsigned log = floor(log10(size));     // 0..9 = 0, 10..99 = 1, 100..199 = 2, etc.
        unsigned exp = pow(10, log);           // 0..9 = 1, 10..99 = 10, 100..199 = 100, etc.
        unsigned div = size / exp;             // Extracts first decimal digit of size.
        int index = log * 10 + div - log - 1;  // Partition each power of 10 into 9 bins.
        index -= 8;                            // Sizes < 10 all go into bin 0;
        return index < 0 ? 0 : (index > int(hist_.size()) - 1 ? hist_.size() - 1 : index);
    }
};

}  // namespace internal

}  // namespace core
