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

#include <QUrl>

#include <cassert>
#include <memory>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class BufferMap final
{
public:
    BufferMap()
        : buffer(nullptr, gst_buffer_unref)
    {
    }
    ~BufferMap()
    {
        unmap();
    }

    void map(GstBuffer* b)
    {
        unmap();
        buffer.reset(gst_buffer_ref(b));
        gst_buffer_map(buffer.get(), &info, GST_MAP_READ);
    }

    void unmap()
    {
        if (!buffer)
        {
            return;
        }
        gst_buffer_unmap(buffer.get(), &info);
        buffer.reset();
    }

    guint8* data() const
    {
        assert(buffer);
        return info.data;
    }

    gsize size() const
    {
        assert(buffer);
        return info.size;
    }

private:
    std::unique_ptr<GstBuffer, decltype(&gst_buffer_unref)> buffer;
    GstMapInfo info;
};

class ThumbnailExtractor final
{
public:
    ThumbnailExtractor();
    ~ThumbnailExtractor();

    void reset();
    void set_urls(QUrl const& in_url, QUrl const& out_url);
    bool has_video();
    bool extract_video_frame();
    bool extract_cover_art();
    void write_image();

    typedef std::unique_ptr<GstSample, decltype(&gst_sample_unref)> SampleUPtr;
    typedef unity::thumbnailer::internal::gobj_ptr<GdkPixbuf> PixbufUPtr;

private:
    void change_state(GstElement* element, GstState state);
    void throw_error(std::string const& msg, GError* error = nullptr);

    gobj_ptr<GstElement> playbin_;
    gint64 duration_ = -1;

    QUrl in_url_;
    QUrl out_url_;
    SampleUPtr sample_{nullptr, gst_sample_unref};  // Contains raw data for cover or still frame.
    PixbufUPtr still_frame_;                        // Non-null if we extracted still frame.
    BufferMap buffermap_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
