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
 * Authored by: Michi Henning <michi@canonical.com>
 */

#pragma once

#include <internal/gobj_memory.h>

#include <QByteArray>
#include <QSize>

#include <string>

struct _GdkPixbuf;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class Image
{
public:
    class Reader;

    // Default constructor does nothing.
    Image() = default;

    // Loads internal pixbuf with provided image data to fit within
    // the dimensions of the requested size.  The image will be
    // rotated if required by the EXIF metadata.
    Image(std::string const& data, QSize requested_size = QSize());
    Image(QByteArray const& ba, QSize requested_size = QSize());
    Image(int fd, QSize requested_size = QSize());

    Image(Image const&) = default;
    Image& operator=(Image const&) = default;
    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    int width() const;
    int height() const;

    // Return the pixel value at the (x,y) coordinates as an integer:
    //  r << 16 | g << 8 | b
    int pixel(int x, int y) const;

    // Return a scaled version of the image that fits within the given
    // requested size.
    Image scale(QSize requested_size) const;

    bool has_alpha() const;  // Returns true if the image has an alpha channel, even if transparency is not used.

    // Returns image as JPEG data if the source does not use transparency,
    // and as PNG, otherwise.
    // The quality must be in the range 0-100, regardless of the whether
    // the source image uses transparency but, if PNG is returned,
    // the quality setting has no effect.
    std::string get_data(int quality = 75) const;

    std::string get_jpeg(int quality = 75) const;
    std::string get_png() const;

private:
    void load(Reader& reader, QSize requested_size);

    gobj_ptr<struct _GdkPixbuf> pixbuf_;
    bool has_alpha_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
