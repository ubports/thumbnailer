/*
 * Copyright (C) 2016 Canonical Ltd.
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

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct ConfigValues
{
    bool trace_client;
    int max_backlog;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity

Q_DECLARE_METATYPE(unity::thumbnailer::service::ConfigValues)

QDBusArgument& operator<<(QDBusArgument& arg, unity::thumbnailer::service::ConfigValues const& s);
QDBusArgument const& operator>>(QDBusArgument const& arg, unity::thumbnailer::service::ConfigValues& s);
