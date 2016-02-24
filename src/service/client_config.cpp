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

#include <service/client_config.h>

using namespace unity::thumbnailer::service;

QDBusArgument& operator<<(QDBusArgument& arg, ConfigValues const& s)
{
    arg.beginStructure();
    arg << s.trace_client
        << s.max_backlog;
    arg.endStructure();
    return arg;
}

QDBusArgument const& operator>>(QDBusArgument const& arg, ConfigValues& s)
{
    arg.beginStructure();
    arg >> s.trace_client
        >> s.max_backlog;
    arg.endStructure();
    return arg;
}
