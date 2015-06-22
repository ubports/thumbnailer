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

#include <string>
#include <sys/stat.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Recursively create the directories in path, setting permissions to the specified mode
// (regardless of the setting of umask). If one or more directories already exist, they
// are left unchanged (including their permissions). If a directory cannot be created,
// fail silently.

void make_directories(std::string const& path_name, mode_t mode);

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
