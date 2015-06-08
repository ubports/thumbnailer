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

#include "dbusserver.h"
#include <service/dbus_names.h>
#include <testsetup.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>

DBusServer::DBusServer()
{
    using namespace unity::thumbnailer::service;

    runner_.reset(new QtDBusTest::DBusTestRunner());
    service_.reset(new QtDBusTest::QProcessDBusService(
        BUS_NAME, QDBusConnection::SessionBus,
        THUMBNAILER_SERVICE, {}));
    runner_->registerService(service_);
    runner_->startServices();

    thumbnailer_.reset(
        new ThumbnailerInterface(BUS_NAME, BUS_THUMBNAILER_PATH,
                                 runner_->sessionConnection()));
    admin_.reset(
        new AdminInterface(BUS_NAME, BUS_ADMIN_PATH,
                           runner_->sessionConnection()));
}

DBusServer::~DBusServer()
{
    // If the service is running, give it a chance to shut down.
    // Without this, it won't update the coverage stats.
    if (service_process().state() == QProcess::Running)
    {
        admin_->Shutdown().waitForFinished();
        service_process().waitForFinished();
    }
    admin_.reset();
    thumbnailer_.reset();

    runner_.reset();
}

QProcess& DBusServer::service_process()
{
    // Ugly, but we need access to some of the non-const methods
    return const_cast<QProcess&>(service_->underlyingProcess());
}
