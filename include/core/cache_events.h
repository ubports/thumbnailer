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
#include <type_traits>

/**
\brief Top-level namespace for core functionality.
*/

namespace core
{

/**
\brief Event types that can be monitored.
*/

// Note: Any change here must have a corresponding change to
//       CacheEventIndex in core/internal/cache_event_indexes.h!

enum class CacheEvent : uint32_t
{
    get        = 1 << 0,  ///< An entry was returned by a call to `get()`, `get_or_put()`, `take()`, or `take_data()`.
    put        = 1 << 1,  ///< An entry was added by a call to `put()` or `get_or_put()`.
    invalidate = 1 << 2,  ///< An entry was removed by a call to `invalidate()`, `take()`, or `take_data()`.
    touch      = 1 << 3,  ///< An entry was refreshed by a call to `touch()`.
    miss       = 1 << 4,  ///< A call to `get()`, `get_or_put()`, `take()`, or `take_data()` failed to return an entry.
    evict_ttl  = 1 << 5,  ///< An expired entry was evicted due to a call to `put()`,
                          ///< `get_or_put()`, `trim_to()`, or `resize()`.
    evict_lru  = 1 << 6,  ///< The oldest entry was evicted due to a call to `put()`,
                          ///< `get_or_put()`, `trim_to()`, or `resize()`.
    END_       = 1 << 7   ///< End marker
};

/**
\brief Returns the bitwise OR of two event types.
*/
inline CacheEvent operator|(CacheEvent left, CacheEvent right)
{
    auto l = std::underlying_type<CacheEvent>::type(left);
    auto r = std::underlying_type<CacheEvent>::type(right);
    return CacheEvent(l | r);
}

/**
\brief Assigns the bitwise OR of `left` and `right` to `left`.
*/
inline CacheEvent& operator|=(CacheEvent& left, CacheEvent right)
{
    return left = left | right;
}

/**
\brief Returns the bitwise AND of two event types.
*/
inline CacheEvent operator&(CacheEvent left, CacheEvent right)
{
    auto l = std::underlying_type<CacheEvent>::type(left);
    auto r = std::underlying_type<CacheEvent>::type(right);
    return CacheEvent(l & r);
}

/**
\brief Assigns the bitwise AND of `left` and `right` to `left`.
*/
inline CacheEvent& operator&=(CacheEvent& left, CacheEvent right)
{
    return left = left & right;
}

/**
\brief Returns the bitwise NOT of `ev`. Unused bits are set to zero.
*/
inline CacheEvent operator~(CacheEvent ev)
{
    auto mask = std::underlying_type<CacheEvent>::type(CacheEvent::END_) - 1;
    auto event = std::underlying_type<CacheEvent>::type(ev);
    return CacheEvent(~event & mask);
}

/**
\brief Convenience definition for all event types.
*/
static constexpr auto AllCacheEvents = CacheEvent(std::underlying_type<CacheEvent>::type(CacheEvent::END_) - 1);

}  // namespace core
