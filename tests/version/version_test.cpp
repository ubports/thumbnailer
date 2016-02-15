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

#include <internal/version.h>

#include <testsetup.h>
#include <gtest/gtest.h>

using namespace std;
using namespace unity::thumbnailer::internal;

char const* VFILE = TESTBINDIR "/thumbnailer-version";
char const* CACHE_VFILE = TESTBINDIR "/thumbnailer-cache-version";

TEST(version, current_version)
{
    EXPECT_EQ(THUMBNAILER_VERSION_MAJOR, Version::major);
    EXPECT_EQ(THUMBNAILER_VERSION_MINOR, Version::minor);
    EXPECT_EQ(THUMBNAILER_VERSION_MICRO, Version::micro);
}

TEST(version, no_previous_file)
{
    ::unlink(VFILE);

    Version v(TESTBINDIR);
    EXPECT_EQ(2, v.prev_major());
    EXPECT_EQ(3, v.prev_minor());
    EXPECT_EQ(0, v.prev_micro());
}

TEST(version, empty_file)
{
    system((string(">") + VFILE).c_str());

    Version v(TESTBINDIR);
    EXPECT_EQ(2, v.prev_major());
    EXPECT_EQ(3, v.prev_minor());
    EXPECT_EQ(0, v.prev_micro());
}

TEST(version, new_version)
{
    system((string("echo 15 20 25 >") + VFILE).c_str());

    Version v(TESTBINDIR);
    EXPECT_EQ(15, v.prev_major());
    EXPECT_EQ(20, v.prev_minor());
    EXPECT_EQ(25, v.prev_micro());
}

TEST(cache_version, current_version)
{
    EXPECT_EQ(THUMBNAILER_CACHE_VERSION, Version::cache_version);
}

TEST(cache_version, no_previous_file)
{
    ::unlink(CACHE_VFILE);

    Version v(TESTBINDIR);
    EXPECT_EQ(0, v.prev_cache_version());
}

TEST(cache_version, empty_file)
{
    system((string(">") + CACHE_VFILE).c_str());

    Version v(TESTBINDIR);
    EXPECT_EQ(0, v.prev_cache_version());
}

TEST(cache_version, new_version)
{
    system((string("echo 7 >") + CACHE_VFILE).c_str());

    Version v(TESTBINDIR);
    EXPECT_EQ(7, v.prev_cache_version());
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
