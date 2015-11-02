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

#pragma once

#include <internal/gobj_memory.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses-equality"
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop

#include <memory>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class ThumbnailExtractor final
{
public:
    ThumbnailExtractor();
    ~ThumbnailExtractor();

    void reset();
    void set_uri(const std::string& uri);
    bool has_video();
    bool extract_video_frame();
    bool extract_cover_art();
    void save_screenshot(const std::string& filename);

    typedef std::unique_ptr<GstSample, decltype(&gst_sample_unref)> SampleUPtr;

private:
    void change_state(GstElement* element, GstState state);
    void throw_error(const char* msg, GError* error = nullptr);

    gobj_ptr<GstElement> playbin_;
    gint64 duration_ = -1;

    std::string uri_;
    SampleUPtr sample_{nullptr, gst_sample_unref};
    GdkPixbufRotation sample_rotation_ = GDK_PIXBUF_ROTATE_NONE;
    bool sample_raw_ = true;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
