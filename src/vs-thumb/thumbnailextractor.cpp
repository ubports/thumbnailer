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

#include <QDebug>
#include <unity/util/ResourcePtr.h>

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <cstring>

#include <fcntl.h>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

std::string const class_name = "ThumbnailExtractor";

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

CoverImage find_cover(GstTagList* tags, const char* tag_name)
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
        throw_error("ThumbnailExtractor(): Could not create playbin");  // LCOV_EXCL_LINE
    }
    playbin_.reset(static_cast<GstElement*>(g_object_ref_sink(pb)));

    GstElement* audio_sink = gst_element_factory_make("fakesink", "audio-fake-sink");
    if (!audio_sink)
    {
        throw_error("ThumbnailExtractor(): Could not create audio sink");  // LCOV_EXCL_LINE
    }
    GstElement* video_sink = gst_element_factory_make("fakesink", "video-fake-sink");
    if (!video_sink)
    {
        // LCOV_EXCL_START
        g_object_unref(audio_sink);
        throw_error("ThumbnailExtractor(): Could not create video sink");
        // LCOV_EXCL_STOP
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
    still_frame_.reset();
}

void ThumbnailExtractor::set_uri(const std::string& uri)
{
    assert(!uri.empty());
    reset();
    uri_= uri;
    g_object_set(playbin_.get(), "uri", uri.c_str(), nullptr);
    change_state(playbin_.get(), GST_STATE_PAUSED);

    if (!gst_element_query_duration(playbin_.get(), GST_FORMAT_TIME, &duration_))
    {
        duration_ = -1;  // LCOV_EXCL_LINE
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

// Extract a still frame from a video. Rotate the frame as needed and leave it in still_frame_ in RGB format.

bool ThumbnailExtractor::extract_video_frame()
{
    // Seek some distance into the video so we don't always get black or a 20th Century Fox logo.
    gint64 seek_point = 10 * GST_SECOND;
    if (duration_ >= 0)
    {
        seek_point = 2 * duration_ / 7;
    }
    gst_element_seek_simple(playbin_.get(), GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), seek_point);
    gst_element_get_state(playbin_.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);

    // Retrieve sample from the playbin.
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> desired_caps(
        gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", "pixel-aspect-ratio", GST_TYPE_FRACTION, 1,
                            1, nullptr),
        gst_caps_unref);
    GstSample* s;
    g_signal_emit_by_name(playbin_.get(), "convert-sample", desired_caps.get(), &s);
    if (!s)
    {
        throw_error("extract_video_frame(): failed to extract still frame");  // LCOV_EXCL_LINE
    }
    sample_.reset(s);

    // Convert raw sample into a pixbuf and store it in still_frame_.
    GstCaps* sample_caps = gst_sample_get_caps(sample_.get());
    if (!sample_caps)
    {
        throw_error("write_image(): Could not retrieve caps for sample buffer");  // LCOV_EXCL_LINE
    }
    GstStructure* sample_struct = gst_caps_get_structure(sample_caps, 0);
    int width = 0;
    int height = 0;
    gst_structure_get_int(sample_struct, "width", &width);
    gst_structure_get_int(sample_struct, "height", &height);
    if (width <= 0 || height <= 0)
    {
        throw_error("write_image(): Could not retrieve image dimensions");  // LCOV_EXCL_LINE
    }

    BufferMap buffermap;
    buffermap.map(gst_sample_get_buffer(sample_.get()));
    still_frame_.reset(gdk_pixbuf_new_from_data(buffermap.data(), GDK_COLORSPACE_RGB, FALSE, 8, width, height,
                                         GST_ROUND_UP_4(width * 3), nullptr, nullptr));

    // Does the sample need to be rotated?
    GdkPixbufRotation sample_rotation = GDK_PIXBUF_ROTATE_NONE;
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-video-tags", 0, &tags);
    if (tags)
    {
        // TODO: The "flip-rotate-*" transforms defined in gst/gsttaglist.h need to be added here.
        char* orientation = nullptr;
        if (gst_tag_list_get_string_index(tags, GST_TAG_IMAGE_ORIENTATION, 0, &orientation) && orientation != nullptr)
        {
            if (!strcmp(orientation, "rotate-90"))
            {
                sample_rotation = GDK_PIXBUF_ROTATE_CLOCKWISE;
            }
            else if (!strcmp(orientation, "rotate-180"))
            {
                sample_rotation = GDK_PIXBUF_ROTATE_UPSIDEDOWN;
            }
            else if (!strcmp(orientation, "rotate-270"))
            {
                sample_rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
            }
            else
            {
                qCritical() << "extract_video_frame(): unknown rotation value:" << orientation;  // LCOV_EXCL_LINE
                // No error here, a flipped/rotated image is better than none.
            }
        }
        gst_tag_list_unref(tags);
    }

    if (sample_rotation != GDK_PIXBUF_ROTATE_NONE)
    {
        GdkPixbuf* rotated = gdk_pixbuf_rotate_simple(still_frame_.get(), sample_rotation);
        if (rotated)
        {
            still_frame_.reset(rotated);
        }
        else
        {
            qCritical() << "extract_video_frame(): gdk_pixbuf_rotate_simple() failed, "
                           "probably out of memory";  // LCOV_EXCL_LINE
        }
    }

    return true;
}

#pragma GCC diagnostic pop

// Try to find an embedded image in an audio or video file.
// If an image cover was found, set sample_ to point at the image data and return true.

bool ThumbnailExtractor::extract_cover_art()
{
    GstTagList* tags = nullptr;
    g_signal_emit_by_name(playbin_.get(), "get-audio-tags", 0, &tags);
    if (!tags)
    {
        return false;  // LCOV_EXCL_LINE
    }
    unity::util::ResourcePtr<decltype(tags), decltype(&gst_tag_list_unref)> tag_guard(tags, gst_tag_list_unref);

    sample_.reset();

    // Look for a normal image (cover or other image).
    auto image = std::move(find_cover(tags, GST_TAG_IMAGE));
    if (image.sample && image.type == cover)
    {
        sample_ = std::move(image.sample);
        return true;
    }

    // We didn't find a full-size cover image. Try to find a preview image instead.
    auto preview_image = find_cover(tags, GST_TAG_PREVIEW_IMAGE);
    if (preview_image.sample && preview_image.type == cover)
    {
        // Michi: I have no idea how to create an audio file with this tag :-(
        // LCOV_EXCL_START
        sample_ = std::move(preview_image.sample);
        return true;
        // LCOV_EXCL_STOP
    }

    // See if we found some other normal image.
    if (image.sample)
    {
        sample_ = std::move(image.sample);
        return true;
    }

    // We might have found a non-cover preview image.
    sample_ = std::move(preview_image.sample);

    return bool(sample_);
}

namespace
{

extern "C"
gboolean write_to_fd(gchar const* buf, gsize count, GError **error, gpointer data)
{
    int fd = *reinterpret_cast<int*>(data);
    errno = 0;
    int rc = write(fd, buf, count);
    if (rc != int(count))
    {
        // LCOV_EXCL_START
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "cannot write image data: %s", errno == 0 ? "short write" : g_strerror(errno));
        return false;
        // LCOV_EXCL_STOP
    }
    return true;
}

}

void ThumbnailExtractor::write_image(const std::string& filename)
{
    assert(still_frame_ || sample_);

    // Figure out where to write to.

    int fd = 1;               // By default, write to stdout.
    if (!filename.empty())
    {
        fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1)
        {
            auto msg = std::string("write_image(): cannot open ") + filename + ": " + strerror(errno);
            qCritical().nospace() << QString::fromStdString(msg);
            throw std::runtime_error(msg);
        }
    }
    auto close_func = [](int fd) { if (fd != 1) ::close(fd); };
    unity::util::ResourcePtr<int, decltype(close_func)> fd_guard(fd, close_func);  // Don't leak fd.

    if (still_frame_)
    {
        // We extracted a still frame from a video. We save as tiff without compression because that is
        // lossless and efficient. (There is no point avoiding the tiff encoding step because
        // still frame extraction is so slow that the gain would be insignificant.)
        GError* error = nullptr;
        if (!gdk_pixbuf_save_to_callback(still_frame_.get(), write_to_fd, &fd, "tiff", &error, "compression", "1", nullptr))
        {
            throw_error("write_image(): cannot write image", error);  // LCOV_EXCL_LINE
        }
        return;
    }

    // If there is no embedded artwork, we are done.
    if (!sample_)
    {
        return;
    }

    // We found embedded artwork. The embedded data is already in some image format, such jpg or png.
    // If we are writing to stdout (to communicate with the thumbnailer), we just dump the image
    // as is; the thumbnailer will decode it.
    BufferMap buffermap;
    buffermap.map(gst_sample_get_buffer(sample_.get()));

    if (fd == 1)
    {
        int rc = write(fd, buffermap.data(), buffermap.size());
        if (gsize(rc) != buffermap.size())
        {
            auto msg = std::string("write_image(): cannot write to ") + filename + ": ";
            msg += errno == -1 ?  strerror(errno) : "short write";
            qCritical().nospace() << QString::fromStdString(msg);
            throw std::runtime_error(msg);
        }
        return;
    }

    // We were told to save to a file. Convert the sample data and write in tiff format.
    PixbufUPtr image_buf;
    gobj_ptr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new());
    GError* error = nullptr;
    if (gdk_pixbuf_loader_write(loader.get(), buffermap.data(), buffermap.size(), &error) &&
        gdk_pixbuf_loader_close(loader.get(), &error))
    {
        GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());
        if (pixbuf)
        {
            image_buf.reset(static_cast<GdkPixbuf*>(g_object_ref(pixbuf)));
        }
    }
    else
    {
        throw_error("write_image(): decoding image", error);  // LCOV_EXCL_LINE
    }

    if (!gdk_pixbuf_save_to_callback(image_buf.get(), write_to_fd, &fd, "tiff", &error, "compression", "1", nullptr))
    {
        throw_error("write_image(): cannot write image to stdout", error);  // LCOV_EXCL_LINE
    }
    return;
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
    while (true)  // LCOV_EXCL_LINE  // False negative from gcovr.
    {
        std::unique_ptr<GstMessage, decltype(&gst_message_unref)> message(
            gst_bus_timed_pop_filtered(bus.get(), GST_CLOCK_TIME_NONE,
                                       static_cast<GstMessageType>(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR)),
            gst_message_unref);
        if (!message)
        {
            break;  // LCOV_EXCL_LINE
        }

        switch (GST_MESSAGE_TYPE(message.get()))
        {
            case GST_MESSAGE_ASYNC_DONE:
                if (GST_MESSAGE_SRC(message.get()) == GST_OBJECT(element))
                {
                    return;
                }
                break;  // LCOV_EXCL_LINE
            case GST_MESSAGE_ERROR:
            {
                GError* error = nullptr;
                gst_message_parse_error(message.get(), &error, nullptr);
                throw_error("change_state(): reading async messages", error);
                // NOTREACHED
            }
            default:
                /* ignore other message types */
                break;  // LCOV_EXCL_LINE
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
