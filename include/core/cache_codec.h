/*
 * Copyright (C) 2015 Canonical Ltd.
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

namespace core
{

/**
Traits for serialization and deserialization of cache custom types.
To use custom types, specialize this template
for each custom type (other than string) in the `core` namespace.

\warning Do _not_ specialize this struct for `std::string`!
Doing so has no effect.

\see PersistentCache
*/

template <typename T>
struct CacheCodec
{
    /**
    \brief Converts a value of custom type T into a string.
    */
    static std::string encode(T const& value);
    /**
    \brief Converts a string into a value of custom type T.
    */
    static T decode(std::string const& s);
};

}  // namespace core
