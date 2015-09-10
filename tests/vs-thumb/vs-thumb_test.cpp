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
const char EMPTY_TEST_FILE[] = TESTDATADIR "/empty";

class ExtractorTest : public ::testing::Test
{
protected:
    ExtractorTest() = default;
    virtual ~ExtractorTest() = default;
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

gobj_ptr<GdkPixbuf> extract(std::string const& filename)
{
    gobj_ptr<GdkPixbuf> image;
    std::unique_ptr<GMainLoop, decltype(&g_main_loop_unref)> main_loop(
        g_main_loop_new(nullptr, false), g_main_loop_unref);
    auto callback = [&](GdkPixbuf* const thumbnail) {
        // We copy the image here, because it may be backed by mmaped memory
        image.reset(thumbnail ? gdk_pixbuf_copy(thumbnail) : nullptr);
        g_main_loop_quit(main_loop.get());
    };
    ThumbnailExtractor extractor;
    extractor.extract(filename_to_uri(filename), callback);
    g_main_loop_run(main_loop.get());
    return image;
}

TEST_F(ExtractorTest, extract_theora)
{
    if (!supports_decoder("video/x-theora"))
    {
        fprintf(stderr, "No support for theora decoder\n");
        return;
    }

    auto image = extract(THEORA_TEST_FILE);
    ASSERT_NE(nullptr, image.get());
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

    auto image = extract(MP4_LANDSCAPE_TEST_FILE);
    ASSERT_NE(nullptr, image.get());
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

    auto image = extract(MP4_PORTRAIT_TEST_FILE);
    ASSERT_NE(nullptr, image.get());
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 720);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 1280);
}

TEST_F(ExtractorTest, extract_vorbis_cover_art)
{
    auto image = extract(VORBIS_TEST_FILE);
    ASSERT_NE(nullptr, image.get());
    EXPECT_EQ(gdk_pixbuf_get_width(image.get()), 200);
    EXPECT_EQ(gdk_pixbuf_get_height(image.get()), 200);
}

TEST_F(ExtractorTest, extract_empty)
{
    auto image = extract(EMPTY_TEST_FILE);
    ASSERT_EQ(nullptr, image.get());
}

TEST_F(ExtractorTest, file_not_found)
{
    EXPECT_THROW(extract(TESTDATADIR "/no-such-file.ogv"), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    gst_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
