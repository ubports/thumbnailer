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
 *              Michi Henning <michi.henning@canonical.com>
 */

#include "thumbnailextractor.h"

#include <boost/algorithm/string.hpp>
#include <QDebug>

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <cstring>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

const std::string class_name = "ThumbnailExtractor";

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

// GstPlayFlags flags from playbin.
//
// GStreamer does not install headers for the enums of individual
// elements anywhere, but they make up part of its ABI.

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

enum ImageType { cover, other };
struct CoverImage
{
    ImageType type;
    ThumbnailExtractor::SampleUPtr sample;
};

// Look for an image with the specified tag. If we find a cover image, CoverImage.type is set to cover,
// and CoverImage.sample points at the image. If we find some other (non-cover) image, type is set to other,
// and sample points at the image. Otherwise, if we can't find any image at all, sample is set to nullptr.

CoverImage find_audio_cover(GstTagList* tags, const char* tag_name)
{
    CoverImage ci{other, {nullptr, gst_sample_unref}};

    bool found_cover = false;
    for (int i = 0; !found_cover; i++)
    {
        GstSample* s;
        if (!gst_tag_list_get_sample_index(tags, tag_name, i, &s))
        {
            break;
        }
        assert(s);
        // Check the type of this image
        int type = GST_TAG_IMAGE_TYPE_UNDEFINED;
        auto structure = gst_sample_get_info(s);
        if (structure)
        {
            gst_structure_get_enum(structure, "image-type", GST_TYPE_TAG_IMAGE_TYPE, &type);
        }
        if (type == GST_TAG_IMAGE_TYPE_FRONT_COVER)
        {
            ci.sample.reset(gst_sample_ref(s));
            found_cover = true;
            ci.type = cover;
        }
        else if (type == GST_TAG_IMAGE_TYPE_UNDEFINED && !ci.sample)
        {
            // Save the first unknown image tag, but continue scanning
            // in case there is one marked as the cover.
            ci.sample.reset(gst_sample_ref(s));
        }
        gst_sample_unref(s);
    }
    return ci;
}

}  // namespace

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
    g_object_set(playbin_.get(), "audio-sink", audio_sink, "video-sink", video_sink, "flags",
                 GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO, nullptr);
}

ThumbnailExtractor::~ThumbnailExtractor()
{
    reset();
}

void ThumbnailExtractor::reset()
{
    change_state(playbin_.get(), GST_STATE_NULL);
    sample_.reset();
    sample_rotation_ = GDK_PIXBUF_ROTATE_NONE;
    sample_raw_ = true;
}

void ThumbnailExtractor::set_uri(const std::string& uri)
{
    assert(!uri.empty());
    reset();
    uri_= uri;
    g_object_set(playbin_.get(), "uri", uri.c_str(), nullptr);
    qDebug().nospace() << uri_.c_str() << ": Changing to state PAUSED";
    change_state(playbin_.get(), GST_STATE_PAUSED);

    if (!gst_element_query_duration(playbin_.get(), GST_FORMAT_TIME, &duration_))
    {
        duration_ = -1;
    }
}

bool ThumbnailExtractor::has_video()
{
    int n_video = 0;
    g_object_get(playbin_.get(), "n-video", &n_video, nullptr);
    return n_video > 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

bool ThumbnailExtractor::extract_video_frame()
{
    gint64 seek_point = 10 * GST_SECOND;
    if (duration_ >= 0)
    {
        seek_point = 2 * duration_ / 7;
    }
    qDebug().nospace() << uri_.c_str() << ": Seeking to position " << int(seek_point / GST_SECOND);
    gst_element_seek_simple(playbin_.get(), GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), seek_point);
    gst_element_get_state(playbin_.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);

    // Retrieve sample from the playbin
    qDebug().nospace() << uri_.c_str() << ": Requesting sample frame";
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> desired_caps(
        gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
                            1, nullptr),
        gst_caps_unref);
    GstSample* s;
    g_signal_emit_by_name(playbin_.get(), "convert-sample", desired_caps.get(), &s);
    if (!s)
    {
        return false;  // TODO: Dubious, should at least report an error here because we should
                       //       always get a still frame from a video.
    }
    sample_.reset(s);
    sample_raw_ = true;

    // Does the sample need to be rotated?
    sample_rotation_ = GDK_PIXBUF_ROTATE_NONE;
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-video-tags", 0, &tags);
    if (tags)
    {
        // TODO: The "flip-rotate-*" transforms defined in gst/gsttaglist.h need to be added here.
        char* orientation = nullptr;
        if (gst_tag_list_get_string_index(tags, GST_TAG_IMAGE_ORIENTATION, 0, &orientation) && orientation != nullptr)
        {
qDebug() << "rotating:" << orientation;
            if (!strcmp(orientation, "rotate-90"))
            {
                sample_rotation_ = GDK_PIXBUF_ROTATE_CLOCKWISE;
            }
            else if (!strcmp(orientation, "rotate-180"))
            {
                sample_rotation_ = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
            }
            else if (!strcmp(orientation, "rotate-270"))
            {
                sample_rotation_ = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
            }
            else
            {
                qCritical() << "extract_video_frame(): unknown rotation value:" << orientation;
                // No error here, a flipped/rotated image is better than none.
            }
        }
        gst_tag_list_unref(tags);
    }
    return true;
}

#pragma GCC diagnostic pop

bool ThumbnailExtractor::extract_audio_cover_art()
{
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-audio-tags", 0, &tags);
    if (!tags)
    {
        return false;
    }
    sample_.reset();
    sample_rotation_ = GDK_PIXBUF_ROTATE_NONE;
    sample_raw_ = false;

    // Look for a normal image (cover or other image).
    auto image = std::move(find_audio_cover(tags, GST_TAG_IMAGE));
    if (image.sample && image.type == cover)
    {
        sample_ = std::move(image.sample);
        return true;
    }

    // We didn't find a full-size cover image. Try to find a preview image instead.
    auto preview_image = find_audio_cover(tags, GST_TAG_PREVIEW_IMAGE);
    if (preview_image.sample && preview_image.type == cover)
    {
        sample_ = std::move(preview_image.sample);
        return true;
    }

    // See if we found some other normal image.
    if (image.sample)
    {
        sample_ = std::move(image.sample);
        return true;
    }

    // We might have found a non-cover preview image.
    sample_ = std::move(preview_image.sample);

    gst_tag_list_unref(tags);
    return bool(sample_);
}

namespace
{

extern "C"
gboolean write_to_fd(gchar const* buf, gsize count, GError **error, gpointer data)
{
    int fd = *reinterpret_cast<int*>(data);
    int rc = write(fd, buf, count);
    if (rc != int(count))
    {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "cannot write image data %s", g_strerror(errno));
        return false;
    }
    return true;
}

}

void ThumbnailExtractor::save_screenshot(const std::string& filename)
{
    if (!sample_)
    {
        throw_error("save_screenshot(): Could not retrieve screenshot");
    }

    // Append ".tiff" to output file name if it isn't there already.
    std::string outfile = filename;
    if (!outfile.empty() && !boost::algorithm::ends_with(filename, ".tiff"))
    {
        outfile += ".tiff";
    }

    // Construct a pixbuf from the sample
    qDebug().nospace() << uri_.c_str() << ": Saving image";
    BufferMap buffermap;
    gobj_ptr<GdkPixbuf> image;
    if (sample_raw_)
    {
        GstCaps* sample_caps = gst_sample_get_caps(sample_.get());
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

        buffermap.map(gst_sample_get_buffer(sample_.get()));
        image.reset(gdk_pixbuf_new_from_data(buffermap.data(), GDK_COLORSPACE_RGB, FALSE, 8, width, height,
                                             GST_ROUND_UP_4(width * 3), nullptr, nullptr));
    }
    else
    {
        gobj_ptr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new());

        buffermap.map(gst_sample_get_buffer(sample_.get()));
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

    if (sample_rotation_ != GDK_PIXBUF_ROTATE_NONE)
    {
        GdkPixbuf* rotated = gdk_pixbuf_rotate_simple(image.get(), sample_rotation_);
        if (rotated)
        {
            image.reset(rotated);
        }
    }

    // Saving as TIFF with no compression here to avoid artefacts due to converting to jpg twice.
    // (The main thumbnailer saves as jpg.) By staying lossless here, we
    // keep all the policy decisions about image quality in the main thumbnailer.
    // "compression", "1" means "no compression" for tiff files.
    GError* error = nullptr;
    if (outfile.empty())
    {
qDebug() << "writing to stdout";
        // Write to stdout.
        int fd = 1;
        if (!gdk_pixbuf_save_to_callback(image.get(), write_to_fd, &fd, "tiff", &error, "compression", "1", nullptr))
        {
            throw_error("save_screenshot(): cannot write image", error);
        }
    }
    else
    {
qDebug() << "saving file";
        if (!gdk_pixbuf_save(image.get(), outfile.c_str(), "tiff", &error, "compression", "1", nullptr))
        {
            throw_error("save_screenshot(): cannot save image", error);
        }
    }
    qDebug().nospace() << uri_.c_str() << ": Done";
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

void ThumbnailExtractor::change_state(GstElement* element, GstState state)
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

#pragma GCC diagnostic pop

void ThumbnailExtractor::throw_error(const char* msg, GError* error)
{
    std::string message = class_name + ": " + msg + ", uri: " + uri_;
    if (error != nullptr)
    {
        message += ": ";
        message += error->message;
        g_error_free(error);
    }
    qCritical() << message.c_str();
    throw std::runtime_error(message);
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
