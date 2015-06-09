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
#include <internal/trace.h>

#include <QCoreApplication>

#include <cstdio>

using namespace std;
using namespace unity::thumbnailer::internal;
using namespace unity::thumbnailer::service;

static const char BUS_NAME[] = "com.canonical.Thumbnailer";

static const char BUS_THUMBNAILER_PATH[] = "/com/canonical/Thumbnailer";
static const char BUS_ADMIN_PATH[] = "/com/canonical/ThumbnailerAdmin";

int main(int argc, char** argv)
{
    int rc = 1;
    try
    {
        qDebug() << "Initializing";

        QCoreApplication app(argc, argv);

        shared_ptr<Thumbnailer> thumbnailer;
        thumbnailer = make_shared<Thumbnailer>();

        unity::thumbnailer::service::DBusInterface server(thumbnailer);
        new ThumbnailerAdaptor(&server);

        unity::thumbnailer::service::AdminInterface admin_server(thumbnailer);
        new ThumbnailerAdminAdaptor(&admin_server);

        auto bus = QDBusConnection::sessionBus();
        bus.registerObject(BUS_THUMBNAILER_PATH, &server);
        bus.registerObject(BUS_ADMIN_PATH, &admin_server);

        qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

        if (!bus.registerService(BUS_NAME))
        {
            throw runtime_error(string("thumbnailer-service: could not aqcquire DBus name ") + BUS_NAME);
        }

        new InactivityHandler(server);

        qDebug() << "Ready";
        rc = app.exec();
        qDebug() << "Exiting";
    }
    catch (std::exception const& e)
    {
        qDebug() << QString(e.what());
    }

    return rc;
}
