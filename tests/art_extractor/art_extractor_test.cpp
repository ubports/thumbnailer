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

#include <boost/algorithm/string.hpp>

#include <testsetup.h>
#include <gtest/gtest.h>

#define AIFF_FILE TESTDATADIR "/testsong.aiff"
#define FLAC_FILE TESTDATADIR "/testsong.flac"
#define FLAC_OTHER_FILE TESTDATADIR "/testsong_other.flac"
#define M4A_FILE TESTDATADIR "/testsong.m4a"
#define MP2_FILE TESTDATADIR "/testsong.mp2"
#define MP3_FILE TESTDATADIR "/testsong.mp3"
#define MP3_OTHER_FILE TESTDATADIR "/testsong_other.mp3"
#define OGG_FILE TESTDATADIR "/testsong.ogg"
#define OGG_OLD_ART_FILE TESTDATADIR "/testsong_old_art.ogg"
#define OGG_FLAC_FILE TESTDATADIR "/testsong.oga"
#define OPUS_FILE TESTDATADIR "/testsong.opus"
#define SPX_FILE TESTDATADIR "/testsong.spx"
#define BAD_MP3_FILE TESTDATADIR "/bad.mp3"
#define NO_EXTENSION TESTDATADIR "/testsong_ogg"

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(art_extractor, aiff)
{
    try
    {
        extract_local_album_art(AIFF_FILE);
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), "testsong.aiff: unknown container format")) << e.what();
    }
}

TEST(art_extractor, flac)
{
    auto art = extract_local_album_art(FLAC_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, flac_other)
{
    auto art = extract_local_album_art(FLAC_OTHER_FILE);
    Image img(art);
    EXPECT_EQ(128, img.width());
    EXPECT_EQ(96, img.height());
}

TEST(art_extractor, m4a)
{
    auto art = extract_local_album_art(M4A_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, mp2)
{
    try
    {
        extract_local_album_art(MP2_FILE);
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), "testsong.mp2: cannot create TagLib::FileRef")) << e.what();
    }
}

TEST(art_extractor, mp3)
{
    auto art = extract_local_album_art(MP3_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, mp3_other)
{
    auto art = extract_local_album_art(MP3_OTHER_FILE);
    Image img(art);
    EXPECT_EQ(128, img.width());
    EXPECT_EQ(96, img.height());
}

TEST(art_extractor, ogg)
{
    auto art = extract_local_album_art(OGG_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, ogg_old_art)
{
    auto art = extract_local_album_art(OGG_OLD_ART_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, ogg_flac)
{
    auto art = extract_local_album_art(OGG_FLAC_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, opus)
{
    auto art = extract_local_album_art(OPUS_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, spx)
{
    auto art = extract_local_album_art(SPX_FILE);
    Image img(art);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST(art_extractor, no_extension)
{
    try
    {
        extract_local_album_art(NO_EXTENSION);
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), "testsong_ogg: cannot create TagLib::FileRef")) << e.what();
    }
}

TEST(art_extractor, no_such_file)
{
    try
    {
        extract_local_album_art("no_such_file");
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_STREQ("no_such_file: cannot open for reading: No such file or directory", e.what());
    }
}

TEST(art_extractor, bad_mp3)
{
    EXPECT_EQ("", extract_local_album_art(BAD_MP3_FILE));
}

int main(int argc, char** argv)
{
    setenv("LC_ALL", "C", true);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
