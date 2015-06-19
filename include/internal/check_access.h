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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#pragma once

#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Returns true if the given AppArmor profile can read the given path.
// Note that if path is a symlink, the check will be against the
// symlink path rather than the symlink target.
bool apparmor_can_read(std::string const& apparmor_label,
                       std::string const& path);

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
