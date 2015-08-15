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

    void extract(std::string const& uri, std::function<void(GdkPixbuf* const)> callback);
    void reset();

private:
    gobj_ptr<GstElement> playbin_;
    gobj_ptr<GstBus> bus_;
    bool is_seeking_ = false;

    std::function<void(GdkPixbuf* const)> callback_;

    void finished(GdkPixbuf* const thumbnail);
    void set_uri(const std::string& uri);
    bool has_video();
    void extract_video_frame();
    void extract_audio_cover_art();
    void save_screenshot(GstSample* sample, GdkPixbufRotation rotation, bool raw);

    static gboolean on_new_message(GstBus *bus, GstMessage *message, void *user_data);
    void state_changed(GstState state);
    void bus_error(GError *error);
    void seek_done();
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
