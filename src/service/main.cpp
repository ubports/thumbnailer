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

#include "admininterface.h"
#include "admininterfaceadaptor.h"
#include "dbusinterface.h"
#include "dbusinterfaceadaptor.h"
#include "inactivityhandler.h"
#include <internal/file_lock.h>
#include <internal/trace.h>
#include <service/dbus_names.h>

#include <QCoreApplication>

#include <cstdio>
#include <sys/stat.h>

using namespace std;
using namespace unity::thumbnailer::internal;
using namespace unity::thumbnailer::service;

namespace
{

QString get_summary(core::PersistentCacheStats const& stats)
{
    auto hits = stats.hits();
    auto misses = stats.misses();
    double hit_rate = (hits + misses == 0) ? 0.0 : double(hits) / (hits + misses);
    auto entries = stats.size();
    auto size_in_bytes = stats.size_in_bytes();
    QString entry_str = entries == 1 ? QStringLiteral("entry") : QStringLiteral("entries");
    QString summary;
    summary = QStringLiteral("%1 %2, %3 bytes, hit rate %5 (%6/%7), avg hit run %8, avg miss run %9")
        .arg(entries)
        .arg(entry_str)
        .arg(size_in_bytes)
        .arg(hit_rate, 4, 'f', 2, '0')
        .arg(hits)
        .arg(misses)
        .arg(stats.avg_hit_run_length(), 4, 'f', 2, '0')
        .arg(stats.avg_miss_run_length(), 4, 'f', 2, '0');
    return summary;
}

void show_stats(shared_ptr<Thumbnailer> const& thumbnailer)
{
    auto stats = thumbnailer->stats();
    qDebug() << qUtf8Printable("image cache:     " + get_summary(stats.full_size_stats));
    qDebug() << qUtf8Printable("thumbnail cache: " + get_summary(stats.thumbnail_stats));
    qDebug() << qUtf8Printable("failure cache:   " + get_summary(stats.failure_stats));
}

string get_cache_dir()
{
    string xdg_base = g_get_user_cache_dir();
    if (xdg_base == "")
    {
        throw runtime_error("Could not determine cache dir");
    }
    return xdg_base;
}

}  // namespace

int main(int argc, char** argv)
{
    TraceMessageHandler message_handler("thumbnailer-service");

    int rc = 1;
    try
    {
        qDebug() << "Initializing";

        // We keep a lock file while the service is alive. That's to avoid
        // a shutdown race where a new service instance starts up while
        // a previous instance is still shutting down, but the leveldb
        // lock has not been released yet by the previous instance.
        auto cache_dir = get_cache_dir();
        ::mkdir(cache_dir.c_str(), 0700);  // May not exist yet.
        AdvisoryFileLock file_lock(cache_dir + "/thumbnailer-service.lock");
        if (!file_lock.lock(chrono::milliseconds(10000)))
        {
            throw runtime_error("Could not acquire file lock within 10 seconds");
        }

        QCoreApplication app(argc, argv);

        auto inactivity_handler = make_shared<InactivityHandler>([&]{ qDebug() << "Idle timeout reached."; app.quit(); });

        auto thumbnailer = make_shared<Thumbnailer>();

        unity::thumbnailer::service::DBusInterface server(thumbnailer, inactivity_handler);
        new ThumbnailerAdaptor(&server);

        unity::thumbnailer::service::AdminInterface admin_server(move(thumbnailer), move(inactivity_handler));
        new ThumbnailerAdminAdaptor(&admin_server);

        auto bus = QDBusConnection::sessionBus();
        bus.registerObject(THUMBNAILER_BUS_PATH, &server);
        bus.registerObject(ADMIN_BUS_PATH, &admin_server);

        qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

        if (!bus.registerService(BUS_NAME))
        {
            throw runtime_error(string("thumbnailer-service: Could not acquire DBus name ") + BUS_NAME);
        }

        // Print basic cache stats on start-up. This is useful when examining log entries.
        show_stats(thumbnailer);

        rc = app.exec();

        // Release the bus name as soon as we decide to shut down, otherwise DBus
        // may still send us requests that we are no longer able to process.
        if (!bus.unregisterService(BUS_NAME))
        {
            throw runtime_error(string("thumbnailer-service: Could not release DBus name ") + BUS_NAME);  // LCOV_EXCL_LINE
        }

        qDebug() << "Exiting";
    }
    catch (std::exception const& e)
    {
        qDebug() << QString(e.what());
    }

    return rc;
}
