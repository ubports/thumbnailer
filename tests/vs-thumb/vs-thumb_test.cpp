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

#include <testsetup.h>
#include <internal/gobj_memory.h>
#include <utils/supports_decoder.h>
#include "../src/vs-thumb/thumbnailextractor.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
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

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

using namespace unity::thumbnailer::internal;

const char THEORA_TEST_FILE[] = TESTDATADIR "/testvideo.ogg";
const char MP4_LANDSCAPE_TEST_FILE[] = TESTDATADIR "/testvideo.mp4";
const char MP4_ROTATE_90_TEST_FILE[] = TESTDATADIR "/testvideo-90.mp4";
const char MP4_ROTATE_180_TEST_FILE[] = TESTDATADIR "/testvideo-180.mp4";
const char MP4_ROTATE_270_TEST_FILE[] = TESTDATADIR "/testvideo-270.mp4";
const char M4V_TEST_FILE[] = TESTDATADIR "/Forbidden Planet.m4v";
const char VORBIS_TEST_FILE[] = TESTDATADIR "/testsong.ogg";
const char AAC_TEST_FILE[] = TESTDATADIR "/testsong.m4a";
const char MP3_TEST_FILE[] = TESTDATADIR "/testsong.mp3";
const char MP3_NO_ARTWORK[] = TESTDATADIR "/no-artwork.mp3";

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
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(THEORA_TEST_FILE));
    ASSERT_TRUE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(1920, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(1080, gdk_pixbuf_get_height(image.get()));
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
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(1280, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(720, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_mp4_rotate_90)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP4_ROTATE_90_TEST_FILE));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(720, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(1280, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_mp4_rotate_180)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP4_ROTATE_180_TEST_FILE));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(1280, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(720, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_mp4_rotate_270)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;
    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP4_ROTATE_270_TEST_FILE));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(720, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(1280, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_vorbis_cover_art)
{
    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(VORBIS_TEST_FILE));
    ASSERT_FALSE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_cover_art());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(200, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(200, gdk_pixbuf_get_height(image.get()));
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
    ASSERT_TRUE(extractor.extract_cover_art());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(200, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(200, gdk_pixbuf_get_height(image.get()));
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
    ASSERT_TRUE(extractor.extract_cover_art());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(200, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(200, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_m4v_cover_art)
{
    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(M4V_TEST_FILE));
    ASSERT_TRUE(extractor.extract_cover_art());
    extractor.write_image(outfile);

    auto image = load_image(outfile);
    EXPECT_EQ(1947, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(3000, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, no_artwork)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for MP3 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_uri(filename_to_uri(MP3_NO_ARTWORK));
    ASSERT_FALSE(extractor.has_video());
    ASSERT_FALSE(extractor.extract_cover_art());
}

TEST_F(ExtractorTest, file_not_found)
{
    ThumbnailExtractor extractor;
    EXPECT_THROW(extractor.set_uri(filename_to_uri(TESTDATADIR "/no-such-file.ogv")), std::runtime_error);
}

std::string vs_thumb_err_output(std::string const& args)
{
    namespace io = boost::iostreams;

    static std::string const cmd = PROJECT_BINARY_DIR "/src/vs-thumb/vs-thumb";

    FILE* p = popen((cmd + " " + args + " 2>&1 >/dev/null").c_str(), "r");
    io::file_descriptor_source s(fileno(p), io::never_close_handle);
    std::stringstream err_output;
    io::copy(s, err_output);
    pclose(p);
    return err_output.str();
}

TEST(ExeTest, usage_1)
{
    auto err = vs_thumb_err_output("");
    EXPECT_EQ("usage: vs-thumb source-file [output-file.tiff]\n", err) << err;
}

TEST(ExeTest, usage_2)
{
    auto err = vs_thumb_err_output("arg1 arg2.tiff arg3");
    EXPECT_EQ("usage: vs-thumb source-file [output-file.tiff]\n", err) << err;
}

TEST(ExeTest, usage_3)
{
    auto err = vs_thumb_err_output("arg1 arg2");
    EXPECT_EQ("vs-thumb: invalid output file name: arg2 (missing .tiff extension)\n", err) << err;
}

TEST(ExeTest, bad_uri)
{
    auto err = vs_thumb_err_output("file:///no_such_file");
    EXPECT_NE(std::string::npos, err.find("vs-thumb: Error creating thumbnail: ThumbnailExtractor")) << err;
}

TEST(ExeTest, no_artwork)
{
    auto err = vs_thumb_err_output(std::string(MP3_NO_ARTWORK) + " xyz.tiff");
    auto msg = std::string("vs-thumb: No artwork in ") + MP3_NO_ARTWORK + "\n";
    EXPECT_TRUE(boost::ends_with(err, msg)) << err;
    boost::system::error_code ec;
    EXPECT_FALSE(boost::filesystem::exists("xyz.diff", ec));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    gst_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
