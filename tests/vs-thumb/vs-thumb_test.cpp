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

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
#endif
#include <gst/gst.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop
#include <gtest/gtest.h>

#include <testsetup.h>
#include <internal/gobj_memory.h>
#include <utils/supports_decoder.h>
#include "../src/vs-thumb/thumbnailextractor.h"

using namespace unity::thumbnailer::internal;

const char THEORA_TEST_FILE[] = TESTDATADIR "/testvideo.ogg";
const char MP4_LANDSCAPE_TEST_FILE[] = TESTDATADIR "/gegl-landscape.mp4";
const char MP4_PORTRAIT_TEST_FILE[] = TESTDATADIR "/gegl-portrait.mp4";
const char VORBIS_TEST_FILE[] = TESTDATADIR "/testsong.ogg";
const char AAC_TEST_FILE[] = TESTDATADIR "/testsong.m4a";
const char MP3_TEST_FILE[] = TESTDATADIR "/testsong.mp3";

class ExtractorTest : public ::testing::Test
{
protected:
    ExtractorTest()
    {
    }
    virtual ~ExtractorTest()
    {
    }

    virtual void SetUp() override
    {
        tempdir = "./vsthumb-test.XXXXXX";
        if (mkdtemp(const_cast<char*>(tempdir.data())) == nullptr)
        {
            tempdir = "";
            throw std::runtime_error("Could not create temporary directory");
        }
    }

    virtual void TearDown() override
    {
        if (!tempdir.empty())
        {
            std::string cmd = "rm -rf \"" + tempdir + "\"";
            ASSERT_EQ(system(cmd.c_str()), 0);
        }
    }

    std::string tempdir;
};

std::string filename_to_uri(const std::string& filename)
{
    std::unique_ptr<GFile, void (*)(void*)> file(g_file_new_for_path(filename.c_str()), g_object_unref);
    if (!file)
    {
        throw std::runtime_error("Could not create GFile");
    }
    std::unique_ptr<char, void (*)(void*)> uri(g_file_get_uri(file.get()), g_free);
    return uri.get();
}

gobj_ptr<GdkPixbuf> load_image(const std::string& filename)
{
    GError* error = nullptr;
    gobj_ptr<GdkPixbuf> image(gdk_pixbuf_new_from_file(filename.c_str(), &error));
    if (error)
    {
        std::string message(error->message);
        g_error_free(error);
        throw std::runtime_error(message);
    }
    return std::move(image);
}

TEST_F(ExtractorTest, extract_theora)
{
    if (!supports_decoder("video/x-theora"))
    {
        fprintf(stderr, "No support for theora decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out";  // vs-thumb appens ".tiff"
    extractor.set_uri(filename_to_uri(THEORA_TEST_FILE));
    ASSERT_TRUE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile + ".tiff");
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 1920);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 1080);
}

TEST_F(ExtractorTest, extract_mp4)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP4_LANDSCAPE_TEST_FILE));
    ASSERT_TRUE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 1920);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 1080);
}

TEST_F(ExtractorTest, extract_mp4_rotation)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP4_PORTRAIT_TEST_FILE));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 720);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 1280);
}

TEST_F(ExtractorTest, extract_vorbis_cover_art)
{
    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(VORBIS_TEST_FILE));
    ASSERT_FALSE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_audio_cover_art());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 200);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 200);
}

TEST_F(ExtractorTest, extract_aac_cover_art)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for AAC decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(AAC_TEST_FILE));
    ASSERT_FALSE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_audio_cover_art());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 200);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 200);
}

TEST_F(ExtractorTest, extract_mp3_cover_art)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for MP3 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP3_TEST_FILE));
    ASSERT_FALSE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_audio_cover_art());
    extractor.save_screenshot(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 200);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 200);
}

TEST_F(ExtractorTest, file_not_found)
{
    ThumbnailExtractor extractor;
    EXPECT_THROW(extractor.set_uri(filename_to_uri(TESTDATADIR "/no-such-file.ogv")), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    gst_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
