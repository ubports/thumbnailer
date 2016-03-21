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
#include <QUrl>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include <fcntl.h>

using namespace unity::thumbnailer::internal;

const char THEORA_TEST_FILE[] = TESTDATADIR "/testvideo.ogg";
const char MP4_LANDSCAPE_TEST_FILE[] = TESTDATADIR "/testvideo.mp4";
const char MP4_ROTATE_90_TEST_FILE[] = TESTDATADIR "/testvideo-90.mp4";
const char MP4_ROTATE_180_TEST_FILE[] = TESTDATADIR "/testvideo-180.mp4";
const char MP4_ROTATE_270_TEST_FILE[] = TESTDATADIR "/testvideo-270.mp4";
const char M4V_TEST_FILE[] = TESTDATADIR "/Forbidden Planet.m4v";

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
    return image;
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
    extractor.set_urls(QUrl::fromLocalFile(THEORA_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image();

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
    extractor.set_urls(QUrl::fromLocalFile(MP4_LANDSCAPE_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.has_video());
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image();

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
    extractor.set_urls(QUrl::fromLocalFile(MP4_ROTATE_90_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image();

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
    extractor.set_urls(QUrl::fromLocalFile(MP4_ROTATE_180_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image();

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
    extractor.set_urls(QUrl::fromLocalFile(MP4_ROTATE_270_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.extract_video_frame());
    extractor.write_image();

    auto image = load_image(outfile);
    EXPECT_EQ(720, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(1280, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, extract_m4v_cover_art)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";
    extractor.set_urls(QUrl::fromLocalFile(M4V_TEST_FILE), QUrl::fromLocalFile(outfile.c_str()));
    ASSERT_TRUE(extractor.extract_cover_art());
    extractor.write_image();

    auto image = load_image(outfile);
    EXPECT_EQ(1947, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(3000, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, can_write_to_fd)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    std::string outfile = tempdir + "/out.tiff";

    int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_GT(fd, 2);
    QString out_url = "fd:" + QString::fromStdString(std::to_string(fd));
    extractor.set_urls(QUrl::fromLocalFile(M4V_TEST_FILE), out_url);
    ASSERT_TRUE(extractor.extract_cover_art());

    extractor.write_image();

    auto image = load_image(outfile);
    EXPECT_EQ(1947, gdk_pixbuf_get_width(image.get()));
    EXPECT_EQ(3000, gdk_pixbuf_get_height(image.get()));
}

TEST_F(ExtractorTest, cant_write_to_fd)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    ThumbnailExtractor extractor;

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GT(fd, 2);
    ASSERT_EQ(0, close(fd));

    QString out_url = "fd:" + QString::fromStdString(std::to_string(fd));
    extractor.set_urls(QUrl::fromLocalFile(M4V_TEST_FILE), out_url);
    ASSERT_TRUE(extractor.extract_cover_art());

    try
    {
        extractor.write_image();
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::starts_with(e.what(), "write_image(): cannot write to file descriptor ")) << e.what();
        EXPECT_TRUE(boost::ends_with(e.what(), ": Bad file descriptor")) << e.what();
    }
}

TEST_F(ExtractorTest, file_not_found)
{
    ThumbnailExtractor extractor;
    EXPECT_THROW(extractor.set_urls(QUrl::fromLocalFile(TESTDATADIR "/no-such-file.ogv"),
                                    QUrl::fromLocalFile("/dev/null")), std::runtime_error);
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
    EXPECT_EQ("usage: vs-thumb source-file (output-file.tiff | fd:num)\n", err) << err;
}

TEST(ExeTest, usage_2)
{
    auto err = vs_thumb_err_output("arg1 arg2.tiff arg3");
    EXPECT_EQ("usage: vs-thumb source-file (output-file.tiff | fd:num)\n", err) << err;
}

TEST(ExeTest, usage_3)
{
    auto err = vs_thumb_err_output("file:arg1 file:arg2");
    EXPECT_EQ("vs-thumb: invalid output file name: file:arg2 (missing .tiff extension)\n", err) << err;
}

TEST(ExeTest, bad_input_uri)
{
    auto err = vs_thumb_err_output("99file:///abc file:test.tiff");
    EXPECT_TRUE(boost::starts_with(err, "vs-thumb: invalid input URL: ")) << err;
}

TEST(ExeTest, bad_output_uri)
{
    auto err = vs_thumb_err_output("file:///abc 99file:test.tiff");
    EXPECT_TRUE(boost::starts_with(err, "vs-thumb: invalid output URL: ")) << err;
}

TEST(ExeTest, bad_input_scheme)
{
    auto err = vs_thumb_err_output("xyz:///abc test.tiff");
    EXPECT_EQ("vs-thumb: invalid input URL: xyz:///abc (invalid scheme name, requires \"file:\")\n", err) << err;
}

TEST(ExeTest, bad_output_scheme)
{
    auto err = vs_thumb_err_output("file:abc ftp:test.tiff");
    EXPECT_EQ("vs-thumb: invalid output URL: ftp:test.tiff (invalid scheme name, requires \"file:\" or \"fd:\")\n",
              err) << err;
}

TEST(ExeTest, bad_fd_url)
{
    auto err = vs_thumb_err_output("file:abc fd:x");
    EXPECT_EQ("vs-thumb: invalid URL: fd:x (expected a number for file descriptor)\n", err) << err;
}

TEST(ExeTest, no_such_input_file)
{
    auto err = vs_thumb_err_output("file:///no_such_file file:test.tiff");
    EXPECT_NE(std::string::npos, err.find("vs-thumb: Error creating thumbnail: ThumbnailExtractor")) << err;
}

TEST(ExeTest, no_such_output_path)
{
    if (!supports_decoder("video/x-theora"))
    {
        fprintf(stderr, "No support for theora decoder\n");
        return;
    }

    auto err = vs_thumb_err_output(std::string("file://") + THEORA_TEST_FILE + " file:///no_such_dir/no_such_file.tiff");
    EXPECT_NE(std::string::npos, err.find("write_image(): cannot open /no_such_dir/no_such_file.tiff: "
              "No such file or directory")) << err;
}

int main(int argc, char** argv)
{
    setenv("LC_ALL", "C", true);

    ::testing::InitGoogleTest(&argc, argv);
    gst_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
