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

#include <cstdint>

namespace core
{

namespace internal
{

// Note: Any change here must have a corresponding change to
//       CacheEvent in core/cache_events.h!

enum class CacheEventIndex : uint32_t
{
    get        = 0,
    put        = 1,
    invalidate = 2,
    touch      = 3,
    miss       = 4,
    evict_ttl  = 5,
    evict_lru  = 6,
    END_       = 7
};

}  // namespace internal

}  // namespace core
