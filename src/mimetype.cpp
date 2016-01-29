/*
 * Copyright (C) 2016 Canonical Ltd.
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

#include <internal/mimetype.h>

#include <internal/gobj_memory.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <gio/gio.h>
#pragma GCC diagnostic pop

#include <QDebug>

#include <cassert>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

string get_mimetype(string const& filename) noexcept
{
    string content_type = "application/octet-stream";

    gobj_ptr<GFile> file(g_file_new_for_path(filename.c_str()));
    assert(file);  // Cannot fail according to doc.

    gobj_ptr<GFileInfo> info(g_file_query_info(file.get(),
                                               G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                                               G_FILE_QUERY_INFO_NONE,
                                               /* cancellable */ NULL,
                                               /* error */ NULL));
    if (info)
    {
        string type = g_file_info_get_attribute_string(info.get(), G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
        if (type.empty())
        {
            return "";  // LCOV_EXCL_LINE
        }
        content_type = type;
    }

    if (content_type == "application/octet-stream")
    {
        // The FAST_CONTENT_TYPE detector returns 'application/octet-stream'
        // for all non-empty files without an extension (as it only uses the extension to
        // determine file type). In these cases, we fall back to the full content type detector.
        GError* err = nullptr;
        gobj_ptr<GFileInfo> full_info(g_file_query_info(file.get(),
                                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                        G_FILE_QUERY_INFO_NONE,
                                                        /* cancellable */ NULL,
                                                        &err));
        if (!full_info)
        {
            // LCOV_EXCL_START
            QString msg = QString(filename.c_str()) + ": g_file_query_info(): " + err->message;
            g_error_free(err);
            qCritical() << msg;
            return "";
            // LCOV_EXCL_STOP
        }

        content_type = g_file_info_get_attribute_string(full_info.get(), G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
        if (content_type.empty())
        {
            return "";  // LCOV_EXCL_LINE
        }
    }

    return content_type;
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
