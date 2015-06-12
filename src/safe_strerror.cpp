/*
 * Copyright (C) 2014 Canonical Ltd.
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

// Get XSI-compliant strerror_r()

#undef _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <string.h>

#include <internal/safe_strerror.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// We place this function into a source file by itself so the macro definitions above
// cannot interfere with anything else.

std::string safe_strerror(int errnum)
{
    char buf[512];
    int rc = strerror_r(errnum, buf, sizeof(buf));
    switch (rc)
    {
        case 0:
        {
            return buf;
        }
        case EINVAL:
        {
            return "invalid error number " + std::to_string(errnum) + " for strerror_r()";
        }
        // LCOV_EXCL_START
        case ERANGE:
        {
            return "buffer size of " + std::to_string(sizeof(buf)) + " is too small for strerror_r(), errnum = " +
                   std::to_string(errnum);
        }
        default:
        {
            return "impossible return value " + std::to_string(rc) + " from strerror_r(), errnum = " +
                   std::to_string(errnum);
        }
            // LCOV_EXCL_STOP
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
