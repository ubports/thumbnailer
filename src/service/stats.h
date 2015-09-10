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

#pragma once

#include <QDBusArgument>

#include <chrono>

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct CacheStats
{
    QString cache_path;
    quint32 policy;
    qint64 size;
    qint64 size_in_bytes;
    qint64 max_size_in_bytes;
    qint64 hits;
    qint64 misses;
    qint64 hits_since_last_miss;
    qint64 misses_since_last_hit;
    qint64 longest_hit_run;
    qint64 longest_miss_run;
    double avg_hit_run_length;
    double avg_miss_run_length;
    qint64 ttl_evictions;
    qint64 lru_evictions;
    std::chrono::time_point<std::chrono::system_clock> most_recent_hit_time;
    std::chrono::time_point<std::chrono::system_clock> most_recent_miss_time;
    std::chrono::time_point<std::chrono::system_clock> longest_hit_run_time;
    std::chrono::time_point<std::chrono::system_clock> longest_miss_run_time;
    QList<quint32> histogram;
};

struct AllStats
{
    CacheStats full_size_stats;
    CacheStats thumbnail_stats;
    CacheStats failure_stats;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity

Q_DECLARE_METATYPE(unity::thumbnailer::service::AllStats)

QDBusArgument& operator<<(QDBusArgument& arg, unity::thumbnailer::service::CacheStats const& s);
QDBusArgument const& operator>>(QDBusArgument const& arg, unity::thumbnailer::service::CacheStats& s);

QDBusArgument& operator<<(QDBusArgument& arg, unity::thumbnailer::service::AllStats const& s);
QDBusArgument const& operator>>(QDBusArgument const& arg, unity::thumbnailer::service::AllStats& s);
