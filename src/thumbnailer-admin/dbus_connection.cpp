/*
 * Copyright (C) 2015 Canonical Ltd.
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

#include "dbus_connection.h"

#include <service/dbus_names.h>

using namespace std;
using namespace unity::thumbnailer::service;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

DBusConnection::DBusConnection()
    : conn_(QDBusConnection::sessionBus())
    , thumbnailer_(BUS_NAME, THUMBNAILER_BUS_PATH, conn_)
    , admin_(BUS_NAME, ADMIN_BUS_PATH, conn_)
{
}

DBusConnection::~DBusConnection() = default;

ThumbnailerInterface& DBusConnection::thumbnailer() noexcept
{
    return thumbnailer_;
}

AdminInterface& DBusConnection::admin() noexcept
{
    return admin_;
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
