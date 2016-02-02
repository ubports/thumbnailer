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

#include <internal/local_album_art.h>

#include <internal/image.h>

#include <testsetup.h>
#include <gtest/gtest.h>

#define MP3_FILE TESTDATADIR "/testsong.mp3"
#define OGG_FILE TESTDATADIR "/testsong.ogg"

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(art_extractor, mp3)
{
    auto art = get_album_art(MP3_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, ogg)
{
    auto art = get_album_art(OGG_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

int main(int argc, char** argv)
{
    setenv("LC_ALL", "C", true);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
