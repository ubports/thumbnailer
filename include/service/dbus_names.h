/*
 * Copyright (C) 2014 Canonical Ltd
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

#pragma once

#include <string>

namespace unity
{

namespace thumbnailer
{

namespace service
{

static const char BUS_NAME[] = "com.canonical.Thumbnailer";
static const char THUMBNAILER_BUS_PATH[] = "/com/canonical/Thumbnailer";
static const char ADMIN_BUS_PATH[] = "/com/canonical/ThumbnailerAdmin";

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
