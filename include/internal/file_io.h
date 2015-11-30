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

#include <QByteArray>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Read entire file and return contents as a string.
std::string read_file(std::string const& filename);

// Write contents to filename.
void write_file(std::string const& filename, std::string const& contents);

// Write contents to filename.
void write_file(std::string const& filename, QByteArray const& contents);

// Write contents to filename.
void write_file(std::string const& filename, char const* buf, size_t len);

// Write contents of in_fd to out_fd, using current read position of in_fd.
void write_file(int in_fd, int out_fd);

// Write contents of fd to path.
void write_file(std::string const& path, int in_fd);

// Return a temporary file name in TMPDIR.
std::string create_tmp_filename();

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
