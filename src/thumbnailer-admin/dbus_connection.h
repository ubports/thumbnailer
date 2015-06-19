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

#include "thumbnailerinterface.h"
#include "admininterface.h"

#include <QDBusConnection>
#include <unity/util/DefinesPtrs.h>

namespace unity
{

namespace thumbnailer
{

namespace tools
{

class DBusConnection final
{
public:
    UNITY_DEFINES_PTRS(DBusConnection);

    DBusConnection();
    ~DBusConnection();

    ThumbnailerInterface& thumbnailer() noexcept;
    AdminInterface& admin() noexcept;

private:
    QDBusConnection conn_;
    ThumbnailerInterface thumbnailer_;
    AdminInterface admin_;
};

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
