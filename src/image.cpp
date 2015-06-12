/*
 * Copyright (C) 2015 Canonical Ltd
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
 *              Michi Henning <michi.henning@canonical.com>
 */

#include <internal/image.h>
#include <internal/safe_strerror.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libexif/exif-loader.h>

#include <memory>
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

class Image::Reader
{
public:
    Reader() = default;
    virtual ~Reader() = default;

    Reader(const Reader&) = delete;
    Reader& operator=(Reader&) = delete;

    // Returns true if (data, length) have been set to a segment of data
    virtual bool read(unsigned char const** data, size_t* length) = 0;
    virtual void rewind() = 0;
};

namespace
{

class BufferReader : public Image::Reader
{
public:
    BufferReader(unsigned char const* data, size_t length)
        : data_(data)
        , length_(length)
    {
    }

    bool read(unsigned char const** data, size_t* length) override
    {
        if (first_read)
        {
            *data = data_;
            *length = length_;
            first_read = false;
            return true;
        }
        return false;
    }

    void rewind() override
    {
        first_read = true;
    }

private:
    unsigned char const* const data_;
    size_t const length_;
    bool first_read = true;
};

class FdReader : public Image::Reader
{
public:
    FdReader(int fd)
        : fd_(fd)
    {
    }

    bool read(unsigned char const** data, size_t* length) override
    {
        ssize_t n_read = ::read(fd_, buffer_, sizeof(buffer_));
        if (n_read < 0)
        {
            throw runtime_error("FdReader::read() failed: " + safe_strerror(errno));  // LCOV_EXCL_LINE
        }
        *data = buffer_;
        *length = n_read;
        return n_read > 0;
    }

    void rewind() override
    {
        if (lseek(fd_, 0, SEEK_SET) < 0)
        {
            throw runtime_error("FdReader::rewind() failed: " + safe_strerror(errno));  // LCOV_EXCL_LINE
        }
    }

private:
    int fd_;
    unsigned char buffer_[64 * 1024];
};

auto do_loader_close = [](GdkPixbufLoader* loader)
{
    if (loader)
    {
        gdk_pixbuf_loader_close(loader, NULL);
        g_object_unref(loader);
    }
};
typedef unique_ptr<GdkPixbufLoader, decltype(do_loader_close)> LoaderPtr;

auto do_exif_loader_close = [](ExifLoader* loader)
{
    if (loader)
    {
        exif_loader_unref(loader);
    }
};
typedef unique_ptr<ExifLoader, decltype(do_exif_loader_close)> ExifLoaderPtr;

auto do_exif_data_unref = [](ExifData* data)
{
    if (data)
    {
        exif_data_unref(data);
    }
};
typedef unique_ptr<ExifData, decltype(do_exif_data_unref)> ExifDataPtr;

gobj_ptr<GdkPixbuf> load_image(Image::Reader& reader, GCallback size_prepared_cb, void* user_data)
{
    LoaderPtr loader(gdk_pixbuf_loader_new(), do_loader_close);
    if (!loader.get())
    {
        throw runtime_error("load_image(): cannot allocate GdkPixbufLoader");  // LCOV_EXCL_LINE
    }

    g_signal_connect(loader.get(), "size-prepared", size_prepared_cb, user_data);
    unsigned char const* data = nullptr;
    size_t length = 0;
    GError* err = nullptr;
    while (reader.read(&data, &length))
    {
        if (!gdk_pixbuf_loader_write(loader.get(), data, length, &err))
        {
            // LCOV_EXCL_START
            string msg = string("load_image(): cannot write to pixbuf loader: ") + err->message;
            g_error_free(err);
            throw runtime_error(msg);
            // LCOV_EXCL_STOP
        }
    }
    if (!gdk_pixbuf_loader_close(loader.get(), &err))
    {
        string msg = string("load_image(): cannot close pixbuf loader: ") + err->message;
        g_error_free(err);
        throw runtime_error(msg);
    }

    // get_pixbuf() may return NULL (e.g. if we stopped loading the image),
    gobj_ptr<GdkPixbuf> pixbuf(gdk_pixbuf_loader_get_pixbuf(loader.get()));
    if (!pixbuf)
    {
        throw runtime_error("load_image(): cannot create pixbuf");  // LCOV_EXCL_LINE
    }
    // gdk_pixbuf_loader_get_pixbuf() returns a borrowed reference
    g_object_ref(pixbuf.get());
    return pixbuf;
}

void maybe_scale_thumbnail(GdkPixbufLoader* loader, int width, int height, void* user_data)
{
    QSize requested_size = *reinterpret_cast<QSize*>(user_data);

    if ((requested_size.width() == 0 || width < requested_size.width()) &&
        (requested_size.height() == 0 || height < requested_size.height()))
    {
        // The thumbnail is smaller than the requested size, so don't
        // bother loading it.
        gdk_pixbuf_loader_set_size(loader, 0, 0);
        return;
    }

    // Fill in missing dimensions from requested size
    if (requested_size.width() == 0)
    {
        requested_size.setWidth(width);
    }
    if (requested_size.height() == 0)
    {
        requested_size.setHeight(height);
    }

    QSize image_size(width, height);
    image_size.scale(requested_size, Qt::KeepAspectRatio);
    if (image_size.width() != width || image_size.height() != height)
    {
        gdk_pixbuf_loader_set_size(loader, image_size.width(), image_size.height());
    }
}

void maybe_scale_image(GdkPixbufLoader* loader, int width, int height, void* user_data)
{
    QSize requested_size = *reinterpret_cast<QSize*>(user_data);

    // If no size has been requested, then keep the original size.
    if (!requested_size.isValid())
    {
        return;
    }

    // Fill in missing dimensions from requested size
    if (requested_size.width() == 0)
    {
        requested_size.setWidth(width);
    }
    if (requested_size.height() == 0)
    {
        requested_size.setHeight(height);
    }

    // If the image fits within the requested size, load it as is.
    if (width <= requested_size.width() && height <= requested_size.height())
    {
        return;
    }

    QSize image_size(width, height);
    image_size.scale(requested_size, Qt::KeepAspectRatio);
    gdk_pixbuf_loader_set_size(loader, image_size.width(), image_size.height());
}

}  // namespace

Image::Image(string const& data, QSize requested_size)
{
    BufferReader reader(reinterpret_cast<unsigned char const*>(&data[0]), data.size());
    load(reader, requested_size);
}

Image::Image(int fd, QSize requested_size)
{
    FdReader reader(fd);
    load(reader, requested_size);
}

void Image::load(Reader& reader, QSize requested_size)
{
    // Try to load EXIF data for orientation information and embedded
    // thumbnail.
    ExifLoaderPtr el(exif_loader_new(), do_exif_loader_close);
    if (!el.get())
    {
        throw runtime_error("Image(): could not create ExifLoader");  // LCOV_EXCL_LINE
    }

    unsigned char const* data = nullptr;
    size_t length = 0;
    while (reader.read(&data, &length))
    {
        if (!exif_loader_write(el.get(), const_cast<unsigned char*>(data), length))
        {
            break;
        }
    }
    reader.rewind();
    ExifDataPtr exif(exif_loader_get_data(el.get()), do_exif_data_unref);

    int orientation = 1;
    QSize unrotated_requested_size = requested_size;
    if (exif)
    {
        // Record the image orientation, if it is available
        ExifByteOrder order = exif_data_get_byte_order(exif.get());
        ExifEntry* e = exif_data_get_entry(exif.get(), EXIF_TAG_ORIENTATION);
        if (e)
        {
            exif_entry_fix(e);
            if (e->format == EXIF_FORMAT_SHORT)
            {
                orientation = exif_get_short(e->data, order);
                switch (orientation)
                {
                    case 5:  // Rotate 90 clockwise and horizontal mirror image
                    case 6:  // Rotate 90 clockwise
                    case 7:  // Rotate 90 anti-clockwise and horizontal mirror image
                    case 8:  // Rotate 90 anti-clockwise
                        unrotated_requested_size.transpose();
                        break;
                    default:
                        break;
                }
            }
        }

        // If there is an embedded thumbnail and we want to resize the image, check if the pixbuf is appropriate.
        if (exif->data != nullptr && requested_size.isValid())
        {
            try
            {
                BufferReader thumbnail(exif->data, exif->size);
                pixbuf_ = load_image(thumbnail, G_CALLBACK(maybe_scale_thumbnail), &unrotated_requested_size);
            }
            catch (const runtime_error& e)
            {
                // Nothing: we'll try to load the main image.
            }
        }
    }

    if (!pixbuf_)
    {
        pixbuf_ = load_image(reader, G_CALLBACK(maybe_scale_image), &unrotated_requested_size);
    }

    // Correct the image orientation, if needed
    switch (orientation)
    {
        case 1:
            // Already in correct orientation
            break;
        case 2:
            // Horizontal mirror image
            pixbuf_.reset(gdk_pixbuf_flip(pixbuf_.get(), true));
            break;
        case 3:
            // Rotate 180
            pixbuf_.reset(gdk_pixbuf_rotate_simple(pixbuf_.get(), GDK_PIXBUF_ROTATE_UPSIDEDOWN));
            break;
        case 4:
            // Vertical mirror image.
            pixbuf_.reset(gdk_pixbuf_flip(pixbuf_.get(), false));
            break;
        case 5:
            // Rotate 90 clockwise and horizontal mirror image
            pixbuf_.reset(gdk_pixbuf_rotate_simple(pixbuf_.get(), GDK_PIXBUF_ROTATE_CLOCKWISE));
            pixbuf_.reset(gdk_pixbuf_flip(pixbuf_.get(), true));
            break;
        case 6:
            // Rotate 90 clockwise
            pixbuf_.reset(gdk_pixbuf_rotate_simple(pixbuf_.get(), GDK_PIXBUF_ROTATE_CLOCKWISE));
            break;
        case 7:
            // Rotate 90 anti-clockwise and horizontal mirror image
            pixbuf_.reset(gdk_pixbuf_rotate_simple(pixbuf_.get(), GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE));
            pixbuf_.reset(gdk_pixbuf_flip(pixbuf_.get(), true));
            break;
        case 8:
            // Rotate 90 anti-clockwise
            pixbuf_.reset(gdk_pixbuf_rotate_simple(pixbuf_.get(), GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE));
            break;
        default:
            // Impossible, according the spec. Rather than throwing or some such,
            // we do nothing and return the EXIF image without any adjustment.
            break;  // LCOV_EXCL_LINE
    }
}

int Image::width() const
{
    assert(pixbuf_);

    auto w = gdk_pixbuf_get_width(pixbuf_.get());
    if (w < 1)
    {
        throw runtime_error("Image::width(): invalid image width: " + to_string(w));  // LCOV_EXCL_LINE
    }
    return w;
}

int Image::height() const
{
    assert(pixbuf_);

    auto h = gdk_pixbuf_get_height(pixbuf_.get());
    if (h < 1)
    {
        throw runtime_error("Image::height(): invalid image height: " + to_string(h));  // LCOV_EXCL_LINE
    }
    return h;
}

int Image::pixel(int x, int y) const
{
    assert(gdk_pixbuf_get_colorspace(pixbuf_.get()) == GDK_COLORSPACE_RGB);
    assert(gdk_pixbuf_get_bits_per_sample(pixbuf_.get()) == 8);

    if (x < 0 || x >= width())
    {
        throw runtime_error("Image::pixel(): invalid x coordinate: " + to_string(x));
    }
    if (y < 0 || y >= height())
    {
        throw runtime_error("Image::pixel(): invalid y coordinate: " + to_string(y));
    }

    int n_channels = gdk_pixbuf_get_n_channels(pixbuf_.get());
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf_.get());
    unsigned char* data = gdk_pixbuf_get_pixels(pixbuf_.get());

    unsigned char* p = data + y * rowstride + x * n_channels;
    return p[0] << 16 | p[1] << 8 | p[2];
}

Image Image::scale(QSize requested_size) const
{
    assert(pixbuf_);
    if (!requested_size.isValid())
    {
        return *this;
    }

    QSize scaled_size(width(), height());
    if (requested_size.width() == 0)
    {
        requested_size.setWidth(scaled_size.width());
    }
    if (requested_size.height() == 0)
    {
        requested_size.setHeight(scaled_size.height());
    }
    // If the image fits within the requested size, return it as is.
    if (width() <= requested_size.width() && height() <= requested_size.height())
    {
        return *this;
    }

    scaled_size.scale(requested_size, Qt::KeepAspectRatio);
    Image scaled;
    scaled.pixbuf_.reset(
        gdk_pixbuf_scale_simple(pixbuf_.get(), scaled_size.width(), scaled_size.height(), GDK_INTERP_BILINEAR));
    if (!scaled.pixbuf_)
    {
        throw runtime_error("Image::scale(): could not create scaled image");  // LCOV_EXCL_LINE
    }
    return scaled;
}

string Image::to_jpeg(int quality) const
{
    assert(pixbuf_);

    if (quality < 0 || quality > 100)
    {
        throw invalid_argument("Image::to_jpeg(): quality out of range [0..100]: " + to_string(quality));
    }
    string s_qual = to_string(quality);

    gchar* buf;
    gsize size;
    GError* err = nullptr;
    if (!gdk_pixbuf_save_to_buffer(pixbuf_.get(), &buf, &size, "jpeg", &err, "quality", s_qual.c_str(), NULL))
    {
        // LCOV_EXCL_START
        string msg = string("Image::get_data(): cannot convert to jpeg: ") + err->message;
        g_error_free(err);
        throw runtime_error(msg);
        // LCOV_EXCL_STOP
    }
    string s(buf, size);
    g_free(buf);
    return s;
}

#pragma GCC diagnostic pop
