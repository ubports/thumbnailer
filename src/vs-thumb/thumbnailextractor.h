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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include <functional>
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

    void extract(std::string const& uri, std::string const& outfile, std::function<void(bool)> callback);

private:
    gobj_ptr<GstElement> playbin_;
    gobj_ptr<GstBus> bus_;

    std::string outfile_;
    std::function<void(bool)> callback_;

    bool is_seeking_ = false;
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample_{nullptr, gst_sample_unref};
    GdkPixbufRotation sample_rotation_ = GDK_PIXBUF_ROTATE_NONE;
    bool sample_raw_ = true;

    void reset();
    void set_uri(const std::string& uri);
    bool has_video();
    bool extract_video_frame();
    bool extract_audio_cover_art();
    bool save_screenshot();

    static gboolean on_new_message(GstBus *bus, GstMessage *message, void *user_data);
    void state_changed(GstState state);
    void bus_error(GError *error);
    void seek_done();
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
