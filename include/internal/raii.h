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

#include <unity/util/ResourcePtr.h>

#include <string>

#include <unistd.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

auto do_close = [](int fd)
{
    if (fd >= 0)
    {
        ::close(fd);
    }
};
typedef unity::util::ResourcePtr<int, decltype(do_close)> FdPtr;

auto do_unlink = [](std::string const& filename)
{
    ::unlink(filename.c_str());
};
typedef unity::util::ResourcePtr<std::string, decltype(do_unlink)> UnlinkPtr;

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
