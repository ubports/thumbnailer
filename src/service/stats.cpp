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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include "stats.h"

#include <QDBusArgument>

using namespace unity::thumbnailer::service;

QDBusArgument& operator<<(QDBusArgument& arg, CacheStats const& s)
{
    using namespace std::chrono;

    arg.beginStructure();
    arg << s.cache_path
        << s.policy
        << s.size
        << s.size_in_bytes
        << s.max_size_in_bytes
        << s.hits
        << s.misses
        << s.hits_since_last_miss
        << s.misses_since_last_hit
        << s.longest_hit_run
        << s.longest_miss_run
        << s.ttl_evictions
        << s.lru_evictions
        << quint64(duration_cast<milliseconds>(s.most_recent_hit_time.time_since_epoch()).count())
        << quint64(duration_cast<milliseconds>(s.most_recent_miss_time.time_since_epoch()).count())
        << quint64(duration_cast<milliseconds>(s.longest_hit_run_time.time_since_epoch()).count())
        << quint64(duration_cast<milliseconds>(s.longest_miss_run_time.time_since_epoch()).count())
        << s.histogram;
        arg.endStructure();
    return arg;
}

QDBusArgument const& operator>>(QDBusArgument const& arg, CacheStats& s)
{
    using namespace std::chrono;

    quint64 mrht;
    quint64 mrmt;
    quint64 lhrt;
    quint64 lmrt;
    arg.beginStructure();
    arg >> s.cache_path
        >> s.policy
        >> s.size
        >> s.size_in_bytes
        >> s.max_size_in_bytes
        >> s.hits
        >> s.misses
        >> s.hits_since_last_miss
        >> s.misses_since_last_hit
        >> s.longest_hit_run
        >> s.longest_miss_run
        >> s.ttl_evictions
        >> s.lru_evictions
        >> mrht
        >> mrmt
        >> lhrt
        >> lmrt
        >> s.histogram;
    arg.endStructure();
    s.most_recent_hit_time = system_clock::time_point(milliseconds(mrht));
    s.most_recent_miss_time = system_clock::time_point(milliseconds(mrmt));
    s.longest_hit_run_time = system_clock::time_point(milliseconds(lhrt));
    s.longest_miss_run_time = system_clock::time_point(milliseconds(lmrt));
    return arg;
}

QDBusArgument& operator<<(QDBusArgument& arg, AllStats const& s)
{
    arg.beginStructure();
    arg << s.full_size_stats
        << s.thumbnail_stats
        << s.failure_stats;
    arg.endStructure();
    return arg;
}

QDBusArgument const& operator>>(QDBusArgument const& arg, AllStats& s)
{
    arg.beginStructure();
    arg >> s.full_size_stats
        >> s.thumbnail_stats
        >> s.failure_stats;
    arg.endStructure();
    return arg;
}
