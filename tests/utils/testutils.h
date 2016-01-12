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

#include <iosfwd>

class QString;

// Helper for gtest to allow us to insert QString into a stream.
std::ostream& operator<<(std::ostream& stream, const QString& s);
// Needed because the QString version will take precedence over the std:: one
std::ostream& operator<<(std::ostream& stream, const char* s);

void PrintTo(const QString& s, std::ostream* stream);
