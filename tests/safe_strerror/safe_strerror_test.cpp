/*
 * Copyright (C) 2014 Canonical Ltd.
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

#include <internal/safe_strerror.h>
#include <gtest/gtest.h>

using namespace unity::thumbnailer::internal;

TEST(safe_strerror, safe_strerror)
{
    EXPECT_EQ("Success", safe_strerror(0));
    EXPECT_EQ("Operation not permitted", safe_strerror(1));
    EXPECT_EQ("invalid error number 77777 for strerror_r()", safe_strerror(77777));
}
