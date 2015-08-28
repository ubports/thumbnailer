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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#pragma once

#include "admininterface.h"
#include "dbusinterface.h"

#include <QObject>
#include <QTimer>

namespace unity
{

namespace thumbnailer
{

namespace service
{

class InactivityHandler : public QObject
{
    Q_OBJECT
public:
    InactivityHandler(DBusInterface& iface, AdminInterface& admin_interface);

private:
    QTimer timer_;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
