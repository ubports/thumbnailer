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

#include <QSize>
#include <QString>

namespace unity
{

namespace thumbnailer
{

namespace tools
{

// Return current working dir.

QString current_directory();

// Construct an output path from inpath and size. The outputh path
// is the stem of the input path with the size and ".jpg" appended.r
// For example, if the input is "xyz/some_image.png", and the size
// is (32, 48), the output becomes "some_image_32x48.jpg".
// If dir is non-empty, it is prepended to the returned path.

std::string make_output_path(std::string const& inpath, QSize const& size, std::string const& dir);

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
