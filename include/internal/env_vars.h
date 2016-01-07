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
 * Authored by: Michi Henning <michi@canonical.com>
 */

#pragma once

#include <map>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

constexpr char const* MAX_IDLE = "THUMBNAILER_MAX_IDLE";
constexpr char const* UBUNTU_SERVER_URL = "THUMBNAILER_UBUNTU_SERVER_URL";
constexpr char const* UTIL_DIR = "THUMBNAILER_UTIL_DIR";
constexpr char const* LOG_LEVEL = "THUMBNAILER_LOG_LEVEL";

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
