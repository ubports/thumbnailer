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

#include "admininterface.h"

#include <internal/thumbnailer.h>

#include <QCoreApplication>


using namespace std;
using namespace unity::thumbnailer::internal;

namespace unity
{

namespace thumbnailer
{

namespace service
{

namespace
{

char const ADMIN_ERROR[] = "com.canonical.ThumbnailerAdmin.Error.Failed";

// Conversion to QDateTime is somewhat awkward because system_clock
// is not guaranteed to use the same epoch as QDateTime.
// (The C++ standard leaves the epoch time point undefined.)
// We figure out the epoch for both clocks and adjust if they differ.
// (On Linux, this should never be the case. Still, without the adjustment,
// the conversion is technically undefined.)

using namespace std::chrono;

QList<quint32> to_list(core::PersistentCacheStats::Histogram const& histogram)
{
    QList<quint32> l;
    for (auto c : histogram)
    {
        l.append(c);
    }
    return l;
}

CacheStats to_cache_stats(core::PersistentCacheStats const& st)
{
    return
    {
        QString(st.cache_path().c_str()),
        quint32(st.policy()),
        qint64(st.size()),
        qint64(st.size_in_bytes()),
        qint64(st.max_size_in_bytes()),
        qint64(st.hits()),
        qint64(st.misses()),
        qint64(st.hits_since_last_miss()),
        qint64(st.misses_since_last_hit()),
        qint64(st.longest_hit_run()),
        qint64(st.longest_miss_run()),
        qint64(st.ttl_evictions()),
        qint64(st.lru_evictions()),
        st.most_recent_hit_time(),
        st.most_recent_miss_time(),
        st.longest_hit_run_time(),
        st.longest_miss_run_time(),
        to_list(st.histogram())
    };
}

}  // namespace

unity::thumbnailer::service::AllStats AdminInterface::Stats()
{
    auto const st = thumbnailer_->stats();
    AllStats all;
    all.full_size_stats = to_cache_stats(st.full_size_stats);
    all.thumbnail_stats = to_cache_stats(st.thumbnail_stats);
    all.failure_stats = to_cache_stats(st.failure_stats);
    return all;
}

void AdminInterface::ClearStats(int cache_id)
{
    if (cache_id < 0 || cache_id >= int(Thumbnailer::CacheSelector::LAST__))
    {
        sendErrorReply(ADMIN_ERROR, QString("ClearStats(): invalid cache selector: ") + QString::number(cache_id));
        return;
    }
    auto selector = static_cast<Thumbnailer::CacheSelector>(cache_id);
    thumbnailer_->clear_stats(selector);
}

void AdminInterface::Clear(int cache_id)
{
    if (cache_id < 0 || cache_id >= int(Thumbnailer::CacheSelector::LAST__))
    {
        sendErrorReply(ADMIN_ERROR, QString("Clear(): invalid cache selector: ") + QString::number(cache_id));
        return;
    }
    auto selector = static_cast<Thumbnailer::CacheSelector>(cache_id);
    thumbnailer_->clear(selector);
}

void AdminInterface::Compact(int cache_id)
{
    if (cache_id < 0 || cache_id >= int(Thumbnailer::CacheSelector::LAST__))
    {
        sendErrorReply(ADMIN_ERROR, QString("Compact(): invalid cache selector: ") + QString::number(cache_id));
        return;
    }
    auto selector = static_cast<Thumbnailer::CacheSelector>(cache_id);
    thumbnailer_->compact(selector);
}

void AdminInterface::Shutdown()
{
    QCoreApplication::instance()->quit();
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
