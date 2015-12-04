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

#include <internal/env_vars.h>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

map<string, char const*> const env_vars =
{
    { "max_idle",           "THUMBNAILER_MAX_IDLE" },
    { "server_url",         "THUMBNAILER_UBUNTU_SERVER_URL" },
    { "util_dir",           "THUMBNAILER_UTIL_DIR" }
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
