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

#include <gst/tag/tag.h>

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
{
    GstElement* pb = gst_element_factory_make("playbin", "playbin");
    if (!pb)
    {
        throw_error("ThumbnailExtractor(): Could not create playbin");
    }
    playbin_.reset(static_cast<GstElement*>(g_object_ref_sink(pb)));

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
    g_object_set(playbin_.get(),
                 "audio-sink", audio_sink,
                 "video-sink", video_sink,
                 "flags", GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO,
                 nullptr);

    bus_.reset(gst_element_get_bus(pb));
    gst_bus_add_watch(bus_.get(), &ThumbnailExtractor::on_new_message, this);
}

ThumbnailExtractor::~ThumbnailExtractor()
{
    reset();
    if (bus_) {
        gst_bus_remove_watch(bus_.get());
    }
}

void ThumbnailExtractor::extract(std::string const& uri, std::function<void(GdkPixbuf* const)> callback)
{
    callback_ = callback;
    set_uri(uri);
}

void ThumbnailExtractor::finished(GdkPixbuf* const thumbnail)
{
    if (callback_)
    {
        callback_(thumbnail);
        callback_ = nullptr;
    }
    reset();
}

void ThumbnailExtractor::reset()
{
    gst_element_set_state(playbin_.get(), GST_STATE_NULL);
    is_seeking_ = false;
}

gboolean ThumbnailExtractor::on_new_message(GstBus */*bus*/, GstMessage *message, void *user_data)
{
    auto extractor = reinterpret_cast<ThumbnailExtractor*>(user_data);

    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) != GST_OBJECT(extractor->playbin_.get()))
        {
            break;
        }
        GstState newstate;
        gst_message_parse_state_changed(message, nullptr, &newstate, nullptr);
        extractor->state_changed(newstate);
        break;
    case GST_MESSAGE_ASYNC_DONE:
        if (GST_MESSAGE_SRC(message) != GST_OBJECT(extractor->playbin_.get()))
        {
            break;
        }
        fprintf(stderr, "Got Async done message, is_seeking == %d\n", extractor->is_seeking_);
        if (extractor->is_seeking_)
        {
            // Check to make sure the state change after the seek has
            // truely completed.
            if (gst_element_get_state(
                    extractor->playbin_.get(),
                    nullptr, nullptr, 0) != GST_STATE_CHANGE_ASYNC)
            {
                extractor->is_seeking_ = false;
                extractor->seek_done();
            }
        }
        break;
    case GST_MESSAGE_ERROR:
    {
        GError* error = nullptr;
        gst_message_parse_error(message, &error, nullptr);
        extractor->bus_error(error);
        g_error_free(error);
        break;
    }
    default:
        /* ignore other message types */
        ;
    }

    return G_SOURCE_CONTINUE;
}

void ThumbnailExtractor::set_uri(const std::string& uri)
{
    reset();
    g_object_set(playbin_.get(), "uri", uri.c_str(), nullptr);

    fprintf(stderr, "Changing to state PAUSED\n");
    auto ret = gst_element_set_state(playbin_.get(), GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        throw_error("set_uri(): Could not change pipeline state to paused.");
    }
}

void ThumbnailExtractor::state_changed(GstState state)
{
    fprintf(stderr, "State changed to %d\n", (int)state);
    switch (state) {
    case GST_STATE_PAUSED:
        if (has_video())
        {
            if (!is_seeking_)
            {
                // Seek somewhere that will hopefully give a relevant image.
                int64_t duration, seek_point;
                if (!gst_element_query_duration(playbin_.get(), GST_FORMAT_TIME, &duration))
                {
                    seek_point = 2 * duration / 7;
                }
                else
                {
                    seek_point = 10 * GST_SECOND;
                }
                fprintf(stderr, "Seeking to position %d\n", (int)(seek_point / GST_SECOND));
                is_seeking_ = true;
                gst_element_seek_simple(playbin_.get(), GST_FORMAT_TIME,
                                        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), seek_point);
            }
        }
        else
        {
            extract_audio_cover_art();
        }
        break;
    default:
        break;
    }
}

void ThumbnailExtractor::seek_done()
{
    extract_video_frame();
}

void ThumbnailExtractor::bus_error(GError *error)
{
    fprintf(stderr, "Error: %s\n", error->message);
    finished(nullptr);
}

bool ThumbnailExtractor::has_video()
{
    int n_video = 0;
    g_object_get(playbin_.get(), "n-video", &n_video, nullptr);
    fprintf(stderr, "Number of video streams == %d\n", n_video);
    return n_video > 0;
}

void ThumbnailExtractor::extract_video_frame()
{
    // Retrieve sample from the playbin
    fprintf(stderr, "Requesting sample frame.\n");
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> desired_caps(
        gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
                            1, nullptr),
        gst_caps_unref);
    GstSample* s;
    g_signal_emit_by_name(playbin_.get(), "convert-sample", desired_caps.get(), &s);
    if (!s)
    {
        fprintf(stderr, "Could not extract sample\n");
        finished(nullptr);
        return;
    }
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        s, gst_sample_unref);

    // Does the sample need to be rotated?
    GdkPixbufRotation rotation = GDK_PIXBUF_ROTATE_NONE;
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-video-tags", 0, &tags);
    if (tags)
    {
        char* orientation = nullptr;
        if (gst_tag_list_get_string_index(tags, GST_TAG_IMAGE_ORIENTATION, 0, &orientation) && orientation != nullptr)
        {
            if (!strcmp(orientation, "rotate-90"))
            {
                rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
            }
            else if (!strcmp(orientation, "rotate-180"))
            {
                rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
            }
            else if (!strcmp(orientation, "rotate-270"))
            {
                rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
            }
        }
        gst_tag_list_unref(tags);
    }
    save_screenshot(sample.get(), rotation, true);
}

void ThumbnailExtractor::extract_audio_cover_art()
{
    fprintf(stderr, "Extracting cover art\n");
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-audio-tags", 0, &tags);
    if (!tags)
    {
        finished(nullptr);
        return;
    }
    std::unique_ptr<GstSample, decltype(&gst_sample_unref)> sample(
        nullptr, gst_sample_unref);
    bool found_cover = false;
    for (int i = 0; !found_cover; i++)
    {
        GstSample* s;
        if (!gst_tag_list_get_sample_index(tags, GST_TAG_IMAGE, i, &s))
        {
            break;
        }
        // Check the type of this image
        auto caps = gst_sample_get_caps(s);
        auto structure = gst_caps_get_structure(caps, 0);
        int type = GST_TAG_IMAGE_TYPE_UNDEFINED;
        gst_structure_get_enum(structure, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &type);
        if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER)
        {
            sample.reset(gst_sample_ref(s));
            found_cover = true;
        }
        else if (type == GST_TAG_IMAGE_TYPE_UNDEFINED && !sample)
        {
            // Save the first unknown image tag, but continue scanning
            // in case there is one marked as the cover.
            sample.reset(gst_sample_ref(s));
        }
        gst_sample_unref(s);
    }
    gst_tag_list_unref(tags);
    if (!sample)
    {
        finished(nullptr);
        return;
    }
    save_screenshot(sample.get(), GDK_PIXBUF_ROTATE_NONE, false);
}

void ThumbnailExtractor::save_screenshot(GstSample* sample, GdkPixbufRotation rotation, bool raw)
{
    if (!sample)
    {
        fprintf(stderr, "save_screenshot(): Could not retrieve screenshot\n");
        finished(nullptr);
        return;
    }

    // Construct a pixbuf from the sample
    fprintf(stderr, "Creating pixbuf from sample\n");
    BufferMap buffermap;
    gobj_ptr<GdkPixbuf> image;
    if (raw)
    {
        GstCaps* sample_caps = gst_sample_get_caps(sample);
        if (!sample_caps)
        {
            fprintf(stderr, "save_screenshot(): Could not retrieve caps for sample buffer\n");
            finished(nullptr);
            return;
        }
        GstStructure* sample_struct = gst_caps_get_structure(sample_caps, 0);
        int width = 0, height = 0;
        gst_structure_get_int(sample_struct, "width", &width);
        gst_structure_get_int(sample_struct, "height", &height);
        if (width <= 0 || height <= 0)
        {
            fprintf(stderr, "save_screenshot(): Could not retrieve image dimensions\n");
            finished(nullptr);
            return;
        }

        buffermap.map(gst_sample_get_buffer(sample));
        image.reset(gdk_pixbuf_new_from_data(buffermap.data(), GDK_COLORSPACE_RGB, FALSE, 8, width, height,
                                             GST_ROUND_UP_4(width * 3), nullptr, nullptr));
    }
    else
    {
        gobj_ptr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new());

        buffermap.map(gst_sample_get_buffer(sample));
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
            fprintf(stderr, "save_screenshot(): decoding image: %s\n", error->message);
            g_error_free(error);
            finished(nullptr);
            return;
        }
    }

    if (rotation != GDK_PIXBUF_ROTATE_NONE)
    {
        GdkPixbuf* rotated = gdk_pixbuf_rotate_simple(image.get(), rotation);
        if (rotated)
        {
            image.reset(rotated);
        }
    }

    finished(image.get());
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity

#pragma GCC diagnostic pop
