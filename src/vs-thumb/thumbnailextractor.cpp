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

#include "thumbnailextractor.h"

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <cstring>

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

#include <internal/gobj_memory.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

const std::string class_name = "ThumbnailExtractor";

void throw_error(const char* msg, GError* error = nullptr)
{
    std::string message = class_name + ": " + msg;
    if (error != nullptr)
    {
        message += ": ";
        message += error->message;
        g_error_free(error);
    }
    throw std::runtime_error(message);
}

void change_state(GstElement* element, GstState state)
{
    GstStateChangeReturn ret = gst_element_set_state(element, state);
    switch (ret)
    {
        case GST_STATE_CHANGE_NO_PREROLL:
        case GST_STATE_CHANGE_SUCCESS:
            return;
        case GST_STATE_CHANGE_ASYNC:
            // The change is happening in a background thread, which we
            // will wait on below.
            break;
        case GST_STATE_CHANGE_FAILURE:
        default:
            throw_error("change_state(): Could not change element state");
    }

    // We're in the async case here, so pop messages off the bus until
    // it is done.
    gobj_ptr<GstBus> bus(gst_element_get_bus(element));
    while (true)
    {
        std::unique_ptr<GstMessage, decltype(&gst_message_unref)> message(
            gst_bus_timed_pop_filtered(bus.get(), GST_CLOCK_TIME_NONE,
                                       static_cast<GstMessageType>(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR)),
            gst_message_unref);
        if (!message)
        {
            break;
        }

        switch (GST_MESSAGE_TYPE(message.get()))
        {
            case GST_MESSAGE_ASYNC_DONE:
                if (GST_MESSAGE_SRC(message.get()) == GST_OBJECT(element))
                {
                    return;
                }
                break;
            case GST_MESSAGE_ERROR:
            {
                GError* error = nullptr;
                gst_message_parse_error(message.get(), &error, nullptr);
                throw_error("change_state(): reading async messages", error);
                break;
            }
            default:
                /* ignore other message types */
                ;
        }
    }
}

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

}  // namespace

struct ThumbnailExtractor::Private
{
    gobj_ptr<GstElement> playbin;
    gint64 duration = -1;

    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample{nullptr, gst_sample_unref};
    GdkPixbufRotation sample_rotation = GDK_PIXBUF_ROTATE_NONE;
    bool sample_raw = true;
};

/* GstPlayFlags flags from playbin.
 *
 * GStreamer does not install headers for the enums of individual
 * elements anywhere, but they make up part of its ABI. */
typedef enum
{
    GST_PLAY_FLAG_VIDEO             = (1 << 0),
    GST_PLAY_FLAG_AUDIO             = (1 << 1),
    GST_PLAY_FLAG_TEXT              = (1 << 2),
    GST_PLAY_FLAG_VIS               = (1 << 3),
    GST_PLAY_FLAG_SOFT_VOLUME       = (1 << 4),
    GST_PLAY_FLAG_NATIVE_AUDIO      = (1 << 5),
    GST_PLAY_FLAG_NATIVE_VIDEO      = (1 << 6),
    GST_PLAY_FLAG_DOWNLOAD          = (1 << 7),
    GST_PLAY_FLAG_BUFFERING         = (1 << 8),
    GST_PLAY_FLAG_DEINTERLACE       = (1 << 9),
    GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
    GST_PLAY_FLAG_FORCE_FILTERS     = (1 << 11),
} GstPlayFlags;

ThumbnailExtractor::ThumbnailExtractor()
    : p(new Private)
{
    GstElement* pb = gst_element_factory_make("playbin", "playbin");
    if (!pb)
    {
        throw_error("ThumbnailExtractor(): Could not create playbin");
    }
    p->playbin.reset(static_cast<GstElement*>(g_object_ref_sink(pb)));

    GstElement* audio_sink = gst_element_factory_make("fakesink", "audio-fake-sink");
    if (!audio_sink)
    {
        throw_error("ThumbnailExtractor(): Could not create audio sink");
    }
    GstElement* video_sink = gst_element_factory_make("fakesink", "video-fake-sink");
    if (!video_sink)
    {
        g_object_unref(audio_sink);
        throw_error("ThumbnailExtractor(): Could not create video sink");
    }

    g_object_set(video_sink, "sync", TRUE, nullptr);
    g_object_set(p->playbin.get(), "audio-sink", audio_sink, "video-sink", video_sink, "flags",
                 GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO, nullptr);
}

ThumbnailExtractor::~ThumbnailExtractor()
{
    reset();
}

void ThumbnailExtractor::reset()
{
    change_state(p->playbin.get(), GST_STATE_NULL);
    p->sample.reset();
    p->sample_rotation = GDK_PIXBUF_ROTATE_NONE;
    p->sample_raw = true;
}

void ThumbnailExtractor::set_uri(const std::string& uri)
{
    reset();
    g_object_set(p->playbin.get(), "uri", uri.c_str(), nullptr);
    fprintf(stderr, "Changing to state PAUSED\n");
    change_state(p->playbin.get(), GST_STATE_PAUSED);

    if (!gst_element_query_duration(p->playbin.get(), GST_FORMAT_TIME, &p->duration))
    {
        p->duration = -1;
    }
}

bool ThumbnailExtractor::has_video()
{
    int n_video = 0;
    g_object_get(p->playbin.get(), "n-video", &n_video, nullptr);
    return n_video > 0;
}

bool ThumbnailExtractor::extract_video_frame()
{
    gint64 seek_point = 10 * GST_SECOND;
    if (p->duration >= 0)
    {
        seek_point = 2 * p->duration / 7;
    }
    fprintf(stderr, "Seeking to position %d\n", (int)(seek_point / GST_SECOND));
    gst_element_seek_simple(p->playbin.get(), GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), seek_point);
    fprintf(stderr, "Waiting for seek to complete\n");
    gst_element_get_state(p->playbin.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);
    fprintf(stderr, "Done.\n");

    // Retrieve sample from the playbin
    fprintf(stderr, "Requesting sample frame.\n");
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> desired_caps(
        gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
                            1, nullptr),
        gst_caps_unref);
    GstSample* s;
    g_signal_emit_by_name(p->playbin.get(), "convert-sample", desired_caps.get(), &s);
    if (!s)
    {
        return false;
    }
    p->sample.reset(s);
    p->sample_raw = true;

    // Does the sample need to be rotated?
    p->sample_rotation = GDK_PIXBUF_ROTATE_NONE;
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(p->playbin.get(), "get-video-tags", 0, &tags);
    if (tags)
    {
        char* orientation = nullptr;
        if (gst_tag_list_get_string_index(tags, GST_TAG_IMAGE_ORIENTATION, 0, &orientation) && orientation != nullptr)
        {
            if (!strcmp(orientation, "rotate-90"))
            {
                p->sample_rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
            }
            else if (!strcmp(orientation, "rotate-180"))
            {
                p->sample_rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
            }
            else if (!strcmp(orientation, "rotate-270"))
            {
                p->sample_rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
            }
        }
        gst_tag_list_unref(tags);
    }
    return true;
}

bool ThumbnailExtractor::extract_audio_cover_art()
{
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(p->playbin.get(), "get-audio-tags", 0, &tags);
    if (!tags)
    {
        return false;
    }
    p->sample.reset();
    p->sample_rotation = GDK_PIXBUF_ROTATE_NONE;
    p->sample_raw = false;
    bool found_cover = false;
    for (int i = 0; !found_cover; i++)
    {
        GstSample* sample;
        if (!gst_tag_list_get_sample_index(tags, GST_TAG_IMAGE, i, &sample))
        {
            break;
        }
        // Check the type of this image
        auto caps = gst_sample_get_caps(sample);
        auto structure = gst_caps_get_structure(caps, 0);
        int type = GST_TAG_IMAGE_TYPE_UNDEFINED;
        gst_structure_get_enum(structure, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &type);
        if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER)
        {
            p->sample.reset(gst_sample_ref(sample));
            found_cover = true;
        }
        else if (type == GST_TAG_IMAGE_TYPE_UNDEFINED && !p->sample)
        {
            // Save the first unknown image tag, but continue scanning
            // in case there is one marked as the cover.
            p->sample.reset(gst_sample_ref(sample));
        }
        gst_sample_unref(sample);
    }
    gst_tag_list_unref(tags);
    return bool(p->sample);
}

void ThumbnailExtractor::save_screenshot(const std::string& filename)
{
    if (!p->sample)
    {
        throw_error("save_screenshot(): Could not retrieve screenshot");
    }

    // Construct a pixbuf from the sample
    fprintf(stderr, "Creating pixbuf from sample\n");
    BufferMap buffermap;
    gobj_ptr<GdkPixbuf> image;
    if (p->sample_raw)
    {
        GstCaps* sample_caps = gst_sample_get_caps(p->sample.get());
        if (!sample_caps)
        {
            throw_error("save_screenshot(): Could not retrieve caps for sample buffer");
        }
        GstStructure* sample_struct = gst_caps_get_structure(sample_caps, 0);
        int width = 0, height = 0;
        gst_structure_get_int(sample_struct, "width", &width);
        gst_structure_get_int(sample_struct, "height", &height);
        if (width <= 0 || height <= 0)
        {
            throw_error("save_screenshot(): Could not retrieve image dimensions");
        }

        buffermap.map(gst_sample_get_buffer(p->sample.get()));
        image.reset(gdk_pixbuf_new_from_data(buffermap.data(), GDK_COLORSPACE_RGB, FALSE, 8, width, height,
                                             GST_ROUND_UP_4(width * 3), nullptr, nullptr));
    }
    else
    {
        gobj_ptr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new());

        buffermap.map(gst_sample_get_buffer(p->sample.get()));
        GError* error = nullptr;
        if (gdk_pixbuf_loader_write(loader.get(), buffermap.data(), buffermap.size(), &error) &&
            gdk_pixbuf_loader_close(loader.get(), &error))
        {
            GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());
            if (pixbuf)
            {
                image.reset(static_cast<GdkPixbuf*>(g_object_ref(pixbuf)));
            }
        }
        else
        {
            throw_error("save_screenshot(): decoding image", error);
        }
    }

    if (p->sample_rotation != GDK_PIXBUF_ROTATE_NONE)
    {
        GdkPixbuf* rotated = gdk_pixbuf_rotate_simple(image.get(), p->sample_rotation);
        if (rotated)
        {
            image.reset(rotated);
        }
    }

    // Saving as TIFF with no compression here to avoid artefacts due to converting to jpg twice.
    // (The main thumbnailer saves as jpg.) By staying lossless here, we
    // keep all the policy decisions about image quality in the main thumbnailer.
    fprintf(stderr, "Saving pixbuf to tiff\n");
    GError* error = nullptr;
    if (!gdk_pixbuf_save(image.get(), filename.c_str(), "tiff", &error, "compression", "1", nullptr))
    {
        throw_error("save_screenshot(): saving image", error);
    }
    fprintf(stderr, "Done.\n");
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity

#pragma GCC diagnostic pop
