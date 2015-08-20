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
#include <service/dbus_names.h>

#include <QCoreApplication>

#include <cstdio>

using namespace std;
using namespace unity::thumbnailer::internal;
using namespace unity::thumbnailer::service;

int main(int argc, char** argv)
{
    TraceMessageHandler message_handler("thumbnailer-service");

    int rc = 1;
    try
    {
        qDebug() << "Initializing";

        QCoreApplication app(argc, argv);

        auto thumbnailer = make_shared<Thumbnailer>();

        unity::thumbnailer::service::DBusInterface server(thumbnailer);
        new ThumbnailerAdaptor(&server);

        unity::thumbnailer::service::AdminInterface admin_server(thumbnailer);
        new ThumbnailerAdminAdaptor(&admin_server);

        auto bus = QDBusConnection::sessionBus();
        bus.registerObject(THUMBNAILER_BUS_PATH, &server);
        bus.registerObject(ADMIN_BUS_PATH, &admin_server);

        qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

        if (!bus.registerService(BUS_NAME))
        {
            throw runtime_error(string("thumbnailer-service: could not acquire DBus name ") + BUS_NAME);
        }

        new InactivityHandler(server);

        qDebug() << "Ready";
        rc = app.exec();

        // We must shut down the thumbnailer before we dismantle the DBus connection.
        // Otherwise, it is possible for an old instance of this service to still
        // be running, while a new instance is activated by DBus, and the database
        // may not yet have been unlocked by the previous instance.
        thumbnailer.reset();
        qDebug() << "Exiting";
    }
    catch (std::exception const& e)
    {
        qDebug() << QString(e.what());
    }

    return rc;
}
