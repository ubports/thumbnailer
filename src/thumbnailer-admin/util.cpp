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

#include "util.h"

#include <internal/image.h>
#include <internal/safe_strerror.h>

#include <boost/filesystem.hpp>

#include <stdexcept>

#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

// Return the current working directory.

QString current_directory()
{
    auto path = getcwd(nullptr, 0);
    if (!path)
    {
        throw runtime_error("getcwd(): " + safe_strerror(errno));  // LCOV_EXCL_LINE
    }
    QString dir = path;
    free(path);
    return dir;
}

// Construct an output path from inpath and size. The outputh path
// is the stem of the input path with the size and ".png" appended.
// For example, if the input is "xyz/some_image.jpg", and the size
// is (32, 48), the output becomes "some_image_32x48.png".
// If dir is non-empty, it is prepended to the returned path.

string make_output_path(string const& inpath, QSize const& size, string const& dir)
{
    string out_path = dir;
    if (!out_path.empty())
    {
        out_path += "/";
    }
    boost::filesystem::path p = inpath;
    out_path += p.filename().stem().native();
    out_path += string("_") + to_string(size.width()) + "x" + to_string(size.height());
    out_path += ".png";
    return out_path;
}

void to_png(QByteArray& ba)
{
    assert(ba.size() >= 8);

    static constexpr array<unsigned char, 8> png_magic = { 137, 80, 78, 71, 13, 10, 26, 10 };
    unsigned char const* data = reinterpret_cast<unsigned char const*>(ba.data());
    if (equal(begin(png_magic), end(png_magic), data))
    {
        return;  // Already in PNG format.
    }

    Image img(ba);
    ba = QByteArray::fromStdString(img.png_data());
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
