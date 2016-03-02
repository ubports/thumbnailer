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

#include <internal/image.h>

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <fcntl.h>

#include <internal/file_io.h>
#include <internal/raii.h>
#include <testsetup.h>

#define TESTIMAGE TESTDATADIR "/orientation-1.jpg"
#define JPEGIMAGE TESTBINDIR "/saved_image.jpg"
#define BADIMAGE TESTDATADIR "/bad_image.jpg"
#define BIGIMAGE TESTDATADIR "/big.jpg"
#define HORIZONTAL_STRIP TESTDATADIR "/horizontal-strip.jpg"
#define VERTICAL_STRIP TESTDATADIR "/vertical-strip.jpg"
#define ANIMATEDIMAGE TESTDATADIR "/animated.gif"
#define SVG_TRANSPARENT_IMAGE TESTDATADIR "/transparent.svg"
#define PNG_TRANSPARENT_IMAGE TESTDATADIR "/transparent.png"

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(Image, basic)
{
    {
        Image i;
    }

    {
        string data = read_file(TESTIMAGE);
        Image i(data);
        EXPECT_EQ(640, i.width());
        EXPECT_EQ(480, i.height());
        EXPECT_EQ(0xFE0000FF, i.pixel(0, 0));
        EXPECT_EQ(0xFFFF00FF, i.pixel(639, 0));
        EXPECT_EQ(0x00FF01FF, i.pixel(639, 479));
        EXPECT_EQ(0x0000FEFF, i.pixel(0, 479));
        EXPECT_FALSE(i.has_alpha());

        // Move constructor
        Image i2(move(i));
        EXPECT_EQ(640, i2.width());
        EXPECT_EQ(480, i2.height());

        // Move assignment
        Image i3;
        i3 = move(i2);
        EXPECT_EQ(640, i3.width());
        EXPECT_EQ(480, i3.height());

        {
            // Load to fit in bounding box
            Image i(data, QSize(320, 320));
            EXPECT_EQ(320, i.width());
            EXPECT_EQ(240, i.height());
        }

        {
            // Load to fit width
            Image i(data, QSize(320, 0));
            EXPECT_EQ(320, i.width());
            EXPECT_EQ(240, i.height());
        }

        {
            // Load to fit height
            Image i(data, QSize(0, 240));
            EXPECT_EQ(320, i.width());
            EXPECT_EQ(240, i.height());
        }

        {
            // Try to up-scale width.
            Image i(data, QSize(700, 0));
            EXPECT_EQ(640, i.width());
            EXPECT_EQ(480, i.height());
        }

        {
            // Try to up-scale height.
            Image i(data, QSize(0, 5000));
            EXPECT_EQ(640, i.width());
            EXPECT_EQ(480, i.height());
        }
    }
}

TEST(Image, scale)
{
    string data = read_file(TESTIMAGE);
    Image img(data);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    Image scaled = img.scale(QSize(400, 400));
    EXPECT_EQ(400, scaled.width());
    EXPECT_EQ(300, scaled.height());

    // Invalid size doesn't change the image
    scaled = img.scale(QSize());
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    // A large requested size results in no scaling
    scaled = img.scale(QSize(1000, 1000));
    EXPECT_EQ(640, scaled.width());
    EXPECT_EQ(480, scaled.height());

    // Aspect ratio maintained
    scaled = img.scale(QSize(1000, 240));
    EXPECT_EQ(320, scaled.width());
    EXPECT_EQ(240, scaled.height());

    // Scale to width
    scaled = img.scale(QSize(400, 0));
    EXPECT_EQ(400, scaled.width());
    EXPECT_EQ(300, scaled.height());

    // Scale to height
    scaled = img.scale(QSize(0, 300));
    EXPECT_EQ(400, scaled.width());
    EXPECT_EQ(300, scaled.height());
}

TEST(Image, adjust_scale)
{
    // For coverage: Check that scaling in one dimension
    // such that the other dimension becomes zero
    // sets the other dimension to 1.

    {
        string data = read_file(HORIZONTAL_STRIP);
        Image img(data);
        EXPECT_EQ(200, img.width());
        EXPECT_EQ(10, img.height());

        Image scaled = img.scale(QSize(8, 0));
        EXPECT_EQ(8, scaled.width());
        EXPECT_EQ(1, scaled.height());
    }

    {
        string data = read_file(VERTICAL_STRIP);
        Image img(data);
        EXPECT_EQ(10, img.width());
        EXPECT_EQ(200, img.height());

        Image scaled = img.scale(QSize(0, 8));
        EXPECT_EQ(1, scaled.width());
        EXPECT_EQ(8, scaled.height());
    }
}

TEST(Image, save_jpeg)
{
    string data = read_file(TESTIMAGE);
    Image i(data);
    EXPECT_EQ(640, i.width());
    EXPECT_EQ(480, i.height());

    string jpeg = i.jpeg_data();
    Image i2(jpeg);
    EXPECT_EQ(640, i2.width());
    EXPECT_EQ(480, i2.height());
    // No test here. Because JPEG is lossy, there is no easy way to verify
    // that the image was saved correctly. Manual inspection of the file
    // is easier (see JPEGIMAGE in the test build dir).
    write_file(JPEGIMAGE, jpeg);
}

TEST(Image, use_exif_thumbnail)
{
    {
        string data = read_file(TESTIMAGE);
        Image img(data, QSize(160, 160));

        EXPECT_EQ(160, img.width());
        EXPECT_EQ(120, img.height());
        EXPECT_EQ(0xFE8081FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF80FF, img.pixel(159, 0));
        EXPECT_EQ(0x81FF81FF, img.pixel(159, 119));
        EXPECT_EQ(0x807FFEFF, img.pixel(0, 119));
    }

    {
        // Again, but with only width specified.
        string data = read_file(TESTIMAGE);
        Image img(data, QSize(160, 0));

        EXPECT_EQ(160, img.width());
        EXPECT_EQ(120, img.height());
        EXPECT_EQ(0xFE8081FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF80FF, img.pixel(159, 0));
        EXPECT_EQ(0x81FF81FF, img.pixel(159, 119));
        EXPECT_EQ(0x807FFEFF, img.pixel(0, 119));
    }

    {
        // Again, but with only height specified.
        string data = read_file(TESTIMAGE);
        Image img(data, QSize(0, 120));

        EXPECT_EQ(160, img.width());
        EXPECT_EQ(120, img.height());
        EXPECT_EQ(0xFE8081FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF80FF, img.pixel(159, 0));
        EXPECT_EQ(0x81FF81FF, img.pixel(159, 119));
        EXPECT_EQ(0x807FFEFF, img.pixel(0, 119));
    }

    {
        // Again, but asking for something smaller than the EXIF thumbnail.
        string data = read_file(TESTIMAGE);
        Image img(data, QSize(80, 0));

        EXPECT_EQ(80, img.width());
        EXPECT_EQ(60, img.height());
        EXPECT_EQ(0xFE8081FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF80FF, img.pixel(79, 0));
        EXPECT_EQ(0x81FF81FF, img.pixel(79, 59));
        EXPECT_EQ(0x807FFEFF, img.pixel(0, 59));
    }

    {
        // Again, asking for something larger than the EXIF thumbnail,
        // but smaller than the full image.
        string data = read_file(TESTIMAGE);
        Image img(data, QSize(200, 200));

        EXPECT_EQ(200, img.width());
        EXPECT_EQ(150, img.height());
        EXPECT_EQ(0xFE0000FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF00FF, img.pixel(199, 0));
        EXPECT_EQ(0x00FF01FF, img.pixel(199, 149));
        EXPECT_EQ(0x0000FEFF, img.pixel(0, 149));
    }
}

TEST(Image, orientation)
{
    for (int i = 1; i <= 8; i++)
    {
        auto filename = string(TESTDATADIR "/orientation-") + to_string(i) + ".jpg";
        string data = read_file(filename);
        Image img(data);
        EXPECT_EQ(640, img.width());
        EXPECT_EQ(480, img.height());
        EXPECT_EQ(0xFE0000FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF00FF, img.pixel(639, 0));
        EXPECT_EQ(0x00FF01FF, img.pixel(639, 479));
        EXPECT_EQ(0x0000FEFF, img.pixel(0, 479));

        // Scaled version
        img = Image(data, QSize(320, 240));
        EXPECT_EQ(320, img.width());
        EXPECT_EQ(240, img.height());
        EXPECT_EQ(0xFE0000FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF00FF, img.pixel(319, 0));
        EXPECT_EQ(0x00FF01FF, img.pixel(319, 239));
        EXPECT_EQ(0x0000FEFF, img.pixel(0, 239));

        // This version will be produced from the thumbnail, which has
        // been tinted to distinguish it from the original.
        img = Image(data, QSize(160, 160));
        EXPECT_EQ(160, img.width());
        EXPECT_EQ(120, img.height());
        EXPECT_EQ(0xFE8081FF, img.pixel(0, 0));
        EXPECT_EQ(0xFFFF80FF, img.pixel(159, 0));
        EXPECT_EQ(0x81FF81FF, img.pixel(159, 119));
        EXPECT_EQ(0x807FFEFF, img.pixel(0, 119));
    }
}

TEST(Image, exceptions)
{
    {
        try
        {
            string data = read_file(BADIMAGE);
            Image i(data);
            FAIL();
        }
        catch (std::exception const& e)
        {
            string msg = e.what();
            EXPECT_TRUE(boost::starts_with(msg, "load_image(): cannot close pixbuf loader: ")) << msg;
        }
    }

    {
        string data = read_file(TESTIMAGE);
        Image i(data);

        try
        {
            i.pixel(-1, 0);
            FAIL();
        }
        catch (std::exception const& e)
        {
            EXPECT_STREQ("Image::pixel(): invalid x coordinate: -1", e.what());
        }

        try
        {
            i.pixel(0, -1);
            FAIL();
        }
        catch (std::exception const& e)
        {
            EXPECT_STREQ("Image::pixel(): invalid y coordinate: -1", e.what());
        }

        try
        {
            i.pixel(640, 0);
            FAIL();
        }
        catch (std::exception const& e)
        {
            EXPECT_STREQ("Image::pixel(): invalid x coordinate: 640", e.what());
        }

        try
        {
            i.pixel(0, 480);
            FAIL();
        }
        catch (std::exception const& e)
        {
            EXPECT_STREQ("Image::pixel(): invalid y coordinate: 480", e.what());
        }
    }

    {
        string data = read_file(TESTIMAGE);
        Image i(data);

        try
        {
            i.jpeg_data(-1);
            FAIL();
        }
        catch (invalid_argument const& e)
        {
            EXPECT_STREQ("Image::jpeg_data(): quality out of range [0..100]: -1", e.what());
        }
        try
        {
            i.jpeg_data(101);
            FAIL();
        }
        catch (invalid_argument const& e)
        {
            EXPECT_STREQ("Image::jpeg_data(): quality out of range [0..100]: 101", e.what());
        }
        EXPECT_NO_THROW(i.jpeg_data(0));
        EXPECT_NO_THROW(i.jpeg_data(100));
    }
}

TEST(Image, load_fd)
{
    FdPtr fd(open(TESTIMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get());
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());
}

TEST(Image, load_fd_big_image)
{
    FdPtr fd(open(BIGIMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    // This image is significantly larger than the buffer used to read
    // the file, so multiple read() calls will be needed to fully
    // consume the image.
    Image img(fd.get());
    EXPECT_EQ(2731, img.width());
    EXPECT_EQ(2048, img.height());
}

TEST(Image, animated_gif)
{
    FdPtr fd(open(ANIMATEDIMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get());
    EXPECT_EQ(480, img.width());
    EXPECT_EQ(360, img.height());
    EXPECT_EQ(0xDDDFDCFF, img.pixel(0, 0));
    EXPECT_EQ(0xD1D3D0FF, img.pixel(479, 359));
    EXPECT_TRUE(img.has_alpha());

    // We stopped reading the image before the end of the file:
    off_t pos = lseek(fd.get(), 0, SEEK_CUR);
    struct stat st;
    ASSERT_EQ(0, fstat(fd.get(), &st));
    EXPECT_LT(pos, st.st_size);
}

TEST(Image, animated_gif_scaled)
{
    FdPtr fd(open(ANIMATEDIMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get(), QSize(400, 0));
    EXPECT_EQ(400, img.width());
    EXPECT_EQ(300, img.height());
    EXPECT_EQ(0xDDDFDCFF, img.pixel(0, 0));
    EXPECT_EQ(0xD1D3D0FF, img.pixel(399, 299));
    EXPECT_TRUE(img.has_alpha());

    // We stopped reading the image before the end of the file:
    off_t pos = lseek(fd.get(), 0, SEEK_CUR);
    struct stat st;
    ASSERT_EQ(0, fstat(fd.get(), &st));
    EXPECT_LT(pos, st.st_size);
}

TEST(Image, svg_transparency)
{
    FdPtr fd(open(SVG_TRANSPARENT_IMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get(), QSize(400, 400));
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
    EXPECT_EQ(0x0, img.pixel(0, 0));
    EXPECT_EQ(0xFF0000FF, img.pixel(100, 100));
    EXPECT_TRUE(img.has_alpha());
}

TEST(Image, svg_transparency_no_size)
{
    FdPtr fd(open(SVG_TRANSPARENT_IMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get(), QSize());
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
    EXPECT_EQ(0x0, img.pixel(0, 0));
    EXPECT_EQ(0xFF0000FF, img.pixel(100, 100));
    EXPECT_TRUE(img.has_alpha());
}

TEST(Image, png_transparency)
{
    FdPtr fd(open(PNG_TRANSPARENT_IMAGE, O_RDONLY), do_close);
    ASSERT_GT(fd.get(), 0);

    Image img(fd.get(), QSize(400, 400));
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
    EXPECT_EQ(0x0, img.pixel(0, 0));
    EXPECT_EQ(0xFF0000FF, img.pixel(100, 100));
    EXPECT_TRUE(img.has_alpha());
}
