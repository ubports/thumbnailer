/*
 * Copyright (C) 2013 Canonical Ltd.
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
 * Authored by: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 *              Michi Henning <michi.henning@canonical.com>
 *              James Henstridge <james.henstridge@canonical.com>
 */

#include <internal/thumbnailer.h>

#include <internal/artreply.h>
#include <internal/cachehelper.h>
#include <internal/check_access.h>
#include <internal/image.h>
#include <internal/imageextractor.h>
#include <internal/make_directories.h>
#include <internal/raii.h>
#include <internal/safe_strerror.h>
#include <internal/ubuntuserverdownloader.h>
#include <settings.h>

#include <boost/filesystem.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <gio/gio.h>
#pragma GCC diagnostic pop

#include <unity/UnityExceptions.h>

#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

enum class Location
{
    local,
    remote
};

}  // namespace

class RequestBase : public ThumbnailRequest
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
    Q_OBJECT
#ifdef __clang__
#pragma clang diagnostic pop
#endif
public:
    virtual ~RequestBase() = default;
    QByteArray thumbnail() override;
    FetchStatus status() const override;

    string const& key() const override
    {
        return key_;
    }

    void check_client_credentials(uid_t, std::string const&) override
    {
    }

    enum class CachePolicy
    {
        cache_fullsize,
        dont_cache_fullsize
    };

    struct ImageData
    {
        FetchStatus status;
        Image image;
        CachePolicy cache_policy;
        Location location;

        ImageData(Image const& image, CachePolicy policy, Location location)
            : status(FetchStatus::downloaded)
            , image(image)
            , cache_policy(policy)
            , location(location)
        {
        }

        ImageData(FetchStatus status, CachePolicy policy, Location location)
            : status(status)
            , cache_policy(policy)
            , location(location)
        {
        }

        ImageData(ImageData&&) = default;
        ImageData& operator=(ImageData&&) = default;
    };

    void set_error_message(string const& msg)
    {
        error_message_ = msg;
    }

    string error_message() const
    {
        return error_message_;
    }

protected:
    RequestBase(Thumbnailer* thumbnailer,
                string const& key,
                QSize const& requested_size,
                chrono::milliseconds timeout);
    virtual ImageData fetch(QSize const& size_hint) noexcept = 0;

    ArtDownloader* downloader() const
    {
        return thumbnailer_->downloader_.get();
    }

    // LCOV_EXCL_START
    string printable_key() const
    {
        // Substitute "\\0" for all occurrences of '\0' in key_.
        string new_key;
        string::size_type start_pos = 0;
        string::size_type end_pos;
        while ((end_pos = key_.find('\0', start_pos)) != string::npos)
        {
            new_key += key_.substr(start_pos, end_pos - start_pos);
            new_key += "\\0";
            start_pos = end_pos + 1;
        }
        if (start_pos != string::npos)
        {
            new_key += key_.substr(start_pos, end_pos);
        }
        return new_key;
    }
    // LCOV_EXCL_STOP

    Thumbnailer* thumbnailer_;
    string key_;
    string error_message_;
    // TODO: Make this a const member again once we remove the hack
    //       that adjusts invalid QSizes to 128x128.
    //QSize const requested_size_;
    QSize requested_size_;
    chrono::milliseconds timeout_;

private:
    FetchStatus status_;
};

namespace
{

class LocalThumbnailRequest : public RequestBase
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
    Q_OBJECT
#ifdef __clang__
#pragma clang diagnostic pop
#endif
public:
    LocalThumbnailRequest(Thumbnailer* thumbnailer,
                          string const& filename,
                          QSize const& requested_size,
                          chrono::milliseconds timeout);
    void check_client_credentials(uid_t user, std::string const& label) override;

protected:
    ImageData fetch(QSize const& size_hint) noexcept override;
    void download(std::chrono::milliseconds timeout) override;

private:
    string filename_;
    unique_ptr<ImageExtractor> image_extractor_;
};

class AlbumRequest : public RequestBase
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
    Q_OBJECT
#ifdef __clang__
#pragma clang diagnostic pop
#endif
public:
    AlbumRequest(Thumbnailer* thumbnailer,
                 string const& artist,
                 string const& album,
                 QSize const& requested_size,
                 chrono::milliseconds timeout);

protected:
    ImageData fetch(QSize const& size_hint) noexcept override;
    void download(std::chrono::milliseconds timeout) override;

private:
    string artist_;
    string album_;
    shared_ptr<ArtReply> artreply_;
};

class ArtistRequest : public RequestBase
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
    Q_OBJECT
#ifdef __clang__
#pragma clang diagnostic pop
#endif
public:
    ArtistRequest(Thumbnailer* thumbnailer,
                  string const& artist,
                  string const& album,
                  QSize const& requested_size,
                  chrono::milliseconds timeout);

protected:
    ImageData fetch(QSize const& size_hint) noexcept override;
    void download(std::chrono::milliseconds timeout) override;

private:
    string artist_;
    string album_;
    shared_ptr<ArtReply> artreply_;
};

}  // namespace

RequestBase::RequestBase(Thumbnailer* thumbnailer,
                         string const& key,
                         QSize const& requested_size,
                         chrono::milliseconds timeout)
    : thumbnailer_(thumbnailer)
    , key_(key)
    , requested_size_(requested_size)
    , timeout_(timeout)
    , status_(FetchStatus::needs_download)
{
}

// Main look-up logic for thumbnails.
//
// key_ is set by the subclass to uniquely identify what is being
// thumbnailed.  For online art, this includes the artist and album.
// For local thumbnails, this includes the path name, inode, mtime,
// and ctime.
//
// We first look in the cache to see if we have a thumbnail already
// for the provided key and size.  If not, we check whether a
// full-size image was downloaded previously and is still hanging
// around. If no image is available in the full size cache, we call
// the fetch() routine (implemented by the subclass), which will
// either (a) report that the data needs to be downloaded, (b) return
// the full size image ready for scaling, or (c) report an error.
//
// If the data needs downloading, we return immediately.  Similarly,
// we return in case of an error.  If the data is available, it may be
// stored in the full size cache.
//
// At this point we have the image data, so scale it to the desired
// size, store the scaled version to the thumbnail cache and return
// it.

QByteArray RequestBase::thumbnail()
{
    try
    {
        // TODO: Turn this into an error soonish. This is logged here as well
        //       as on the Qt/QML size so we don't need to look at the logs for
        //       all applications to spot when we get an invalid size.
        if (!requested_size_.isValid())
        {
            qWarning().nospace() << "deprecated invalid size: "
                                 << requested_size_
                                 << ". This feature will be removed soon. Pass the desired size instead.";
            requested_size_.setWidth(128);
            requested_size_.setHeight(128);
        }

        // Enforce size limitation.
        auto target_size = QSize(thumbnailer_->max_size_, thumbnailer_->max_size_);
        if (requested_size_.width() != 0)
        {
            target_size.setWidth(min(requested_size_.width(), thumbnailer_->max_size_));
        }
        if (requested_size_.height() != 0)
        {
            target_size.setHeight(min(requested_size_.height(), thumbnailer_->max_size_));
        }

        string sized_key = key_;
        sized_key += '\0';
        sized_key += to_string(target_size.width());
        sized_key += '\0';
        sized_key += to_string(target_size.height());

        // Check if we have the thumbnail in the cache already.
        assert(thumbnailer_);
        assert(thumbnailer_->thumbnail_cache_);
        auto thumbnail = thumbnailer_->thumbnail_cache_->get(sized_key);
        if (thumbnail)
        {
            status_ = FetchStatus::cache_hit;
            return QByteArray::fromStdString(*thumbnail);
        }

        // Don't have the thumbnail yet, see if we have the original image around.
        auto full_size = thumbnailer_->full_size_cache_->get(key_);
        Image scaled_image;
        if (full_size)
        {
            status_ = ThumbnailRequest::FetchStatus::scaled_from_fullsize;
            scaled_image = Image(*full_size, target_size);
        }
        else
        {
            // Try and download or read the artwork, provided that we don't
            // have this image in the failure cache. We use get()
            // here instead of contains_key(), so the stats for the
            // failure cache are updated.
            if (thumbnailer_->failure_cache_->get(key_))
            {
                status_ = ThumbnailRequest::FetchStatus::cached_failure;
                return "";
            }

            ImageData image_data = fetch(target_size);
            status_ = image_data.status;
            switch (status_)
            {
                case FetchStatus::downloaded:      // Success, we'll return the thumbnail below.
                    break;
                case FetchStatus::needs_download:  // Caller will call download().
                    if (image_data.location == Location::remote)
                    {
                        // If we had the last failure within the retry limit,
                        // don't attempt the download.
                        auto now = chrono::system_clock::now();
                        if (now <= thumbnailer_->nw_fail_time_ + chrono::hours(thumbnailer_->retry_error_hours_))
                        {
                            qWarning() << "RequestBase::thumbnail(): server access retry time not reached yet";
                            status_ = ThumbnailRequest::FetchStatus::temporary_error;
                        }
                    }
                    return "";
                case FetchStatus::network_down:    // Try again next time.
                    return "";  // LCOV_EXCL_LINE  // No way to test this without physically disabling network.
                case FetchStatus::not_found:
                {
                    // Authoritative answer that artwork does not exist.
                    // For local files, we don't set an expiry time because, if the file is
                    // changed (say, such that artwork is added), the file's key will change too.
                    // For remote files, we try again after one week.
                    chrono::time_point<std::chrono::system_clock> later;  // Infinite expiry time
                    if (image_data.location == Location::remote)
                    {
                        later = chrono::system_clock::now() + chrono::hours(thumbnailer_->retry_not_found_hours_);
                    }
                    thumbnailer_->failure_cache_->put(key_, "", later);
                    return "";
                }
                case FetchStatus::hard_error:
                {
                    // No chance of recovery, the problem is with the request data.
                    thumbnailer_->failure_cache_->put(key_, "");
                    return "";
                }
                case FetchStatus::temporary_error:
                {
                    // Extraction failure for local media is always a hard error.
                    assert(image_data.location == Location::remote);

                    // Some non-authoritative failure, such as the server not responding.
                    // We record the time of this failure.
                    qWarning() << "RequestBase::thumbnail(): unexpected download error, delaying server access for"
                               << thumbnailer_->retry_error_hours_ << "hours";
                    thumbnailer_->nw_fail_time_ = chrono::system_clock::now();
                    return "";
                }
                default:
                    abort();  // LCOV_EXCL_LINE  // Impossible
            }

            // We keep the full-size version around for a while because it
            // is likely that the caller will ask for small thumbnail
            // first (for initial search results), followed by a larger
            // thumbnail (for a preview). If so, we don't download the
            // artwork a second time.
            if (image_data.cache_policy == CachePolicy::cache_fullsize)
            {
                Image full_size_image = image_data.image;
                auto w = full_size_image.width();
                auto h = full_size_image.height();
                auto max_size = thumbnailer_->max_size_;
                if (max(w, h) > max_size)
                {
                    // Don't put ridiculously large images into the full-size cache.
                    full_size_image = full_size_image.scale(QSize(max_size, max_size));
                }
                thumbnailer_->full_size_cache_->put(key_, full_size_image.to_jpeg(90));  // Keep high-quality image.
            }
            // If the image is already within the target dimensions, this
            // will be a no-op.
            scaled_image = image_data.image.scale(target_size);
        }

        string jpeg = scaled_image.to_jpeg();
        thumbnailer_->thumbnail_cache_->put(sized_key, jpeg);
        return QByteArray::fromStdString(jpeg);
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        // Something totally unexpected happened.
        string msg = "RequestBase::thumbnail(): key = " + printable_key() + ": " + e.what();
        qCritical() << QString::fromStdString(msg);
        throw unity::ResourceException(msg);
    }
    // LCOV_EXCL_STOP
}

ThumbnailRequest::FetchStatus RequestBase::status() const
{
    return status_;
}

LocalThumbnailRequest::LocalThumbnailRequest(Thumbnailer* thumbnailer,
                                             string const& filename,
                                             QSize const& requested_size,
                                             chrono::milliseconds timeout)
    : RequestBase(thumbnailer, "", requested_size, timeout)
    , filename_(filename)
{
    // We canonicalise the path name both to avoid caching the file
    // multiple times, and to ensure our access checks are against the
    // real file rather than a symlink.
    filename_ = boost::filesystem::canonical(filename).native();

    struct stat st;
    if (stat(filename_.c_str(), &st) < 0)
    {
        // LCOV_EXCL_START
        throw runtime_error("LocalThumbnailRequest(): Could not stat " + filename_ + ": " + safe_strerror(errno));
        // LCOV_EXCL_STOP
    }

    if (!S_ISREG(st.st_mode))
    {
        throw runtime_error("LocalThumbnailRequest(): '" + filename_ + "' is not a regular file");
    }

    // The full cache key for the file is the concatenation of path
    // name, inode, modification time, and inode modification time
    // (because permissions may change).  If the file exists with the
    // same path on different removable media, or the file was
    // modified since we last cached it, the key will be
    // different. There is no point in trying to remove such stale
    // entries from the cache. Instead, we just let the normal
    // eviction mechanism take care of them (because stale thumbnails
    // due to file removal or file update are rare).
    key_ = filename_;
    key_ += '\0';
    key_ += to_string(st.st_ino);
    key_ += '\0';
    key_ += to_string(st.st_mtim.tv_sec) + "." + to_string(st.st_mtim.tv_nsec);
    key_ += '\0';
    key_ += to_string(st.st_ctim.tv_sec) + "." + to_string(st.st_ctim.tv_nsec);
}

void LocalThumbnailRequest::check_client_credentials(uid_t user,
                                                     std::string const& label)
{
    if (user != geteuid())
    {
        throw runtime_error("LocalThumbnailRequest::fetch(): Request comes from a different user ID");
    }
    if (!apparmor_can_read(label, filename_)) {
        // LCOV_EXCL_START
        qDebug() << "Apparmor label" << QString::fromStdString(label) << "has no access to" << QString::fromStdString(filename_);
        throw runtime_error("LocalThumbnailRequest::fetch(): AppArmor policy forbids access to " + filename_);
        // LCOV_EXCL_STOP
    }

}

RequestBase::ImageData LocalThumbnailRequest::fetch(QSize const& size_hint) noexcept
{
    // Default in case something below throws.
    ImageData image_data(FetchStatus::hard_error, CachePolicy::dont_cache_fullsize, Location::local);
    try
    {
        if (image_extractor_)
        {
            // The image data has been extracted via vs-thumb. Update image_data in case read() throws.
            image_data.cache_policy = CachePolicy::cache_fullsize;
            return ImageData(Image(image_extractor_->read()), CachePolicy::cache_fullsize, Location::local);
        }

        // Work out content type.
        gobj_ptr<GFile> file(g_file_new_for_path(filename_.c_str()));
        assert(file);  // Cannot fail according to doc.

        gobj_ptr<GFileInfo> info(g_file_query_info(file.get(), G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   /* cancellable */ NULL,
                                                   /* error */ NULL));
        string content_type = "application/octet-stream";
        if (info)
        {
            string content_type = g_file_info_get_attribute_string(info.get(), G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            if (content_type.empty())
            {
                return image_data;  // LCOV_EXCL_LINE
            }
        }

        if (content_type == "application/octet-stream")
        {
            // The FAST_CONTENT_TYPE detector will return 'application/octet-stream'
            // for all files without an extension (as it only uses the extension to
            // determine file type). In these cases, we fall back to the full content
            // type detector.
            gobj_ptr<GFileInfo> full_info(g_file_query_info(file.get(), G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                            G_FILE_QUERY_INFO_NONE,
                                                            /* cancellable */ NULL,
                                                            /* error */ NULL));  // TODO: need decent error reporting
            if (!full_info)
            {
                return image_data;  // LCOV_EXCL_LINE
            }

            content_type = g_file_info_get_attribute_string(full_info.get(), G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
            if (content_type.empty())
            {
                return image_data;  // LCOV_EXCL_LINE
            }
        }

        // Call the appropriate image extractor and return the image data as JPEG (not scaled).
        // We indicate that full-size images are to be cached only for audio and video files,
        // for which extraction is expensive. For local images, we don't cache full size.

        if (content_type.find("audio/") == 0 || content_type.find("video/") == 0)
        {
            return ImageData(FetchStatus::needs_download, CachePolicy::cache_fullsize, Location::local);
        }
        if (content_type.find("image/") == 0)
        {
            FdPtr fd(open(filename_.c_str(), O_RDONLY | O_CLOEXEC), do_close);
            if (fd.get() < 0)
            {
                // LCOV_EXCL_START
                throw runtime_error("LocalThumbnailRequest::fetch(): Could not open " + filename_ + ": " + safe_strerror(errno));
                // LCOV_EXCL_STOP
            }
            Image scaled(fd.get(), size_hint);
            return ImageData(scaled, CachePolicy::dont_cache_fullsize, Location::local);
        }
    }
    catch (std::exception const& e)
    {
        set_error_message(e.what());
        return image_data;
    }
    return ImageData(FetchStatus::not_found, image_data.cache_policy, Location::local);
}

void LocalThumbnailRequest::download(chrono::milliseconds timeout)
{
    if (timeout.count() == 0)
    {
        timeout = timeout_;
    }
    image_extractor_.reset(new ImageExtractor(filename_, timeout));
    connect(image_extractor_.get(), &ImageExtractor::finished, this, &LocalThumbnailRequest::downloadFinished,
            Qt::DirectConnection);
    image_extractor_->extract();
}

AlbumRequest::AlbumRequest(Thumbnailer* thumbnailer,
                           string const& artist,
                           string const& album,
                           QSize const& requested_size,
                           chrono::milliseconds timeout)
    : RequestBase(thumbnailer, artist + '\0' + album + '\0' + "album", requested_size, timeout)
    , artist_(artist)
    , album_(album)
{
}

namespace
{

// Logic for AlbumRequest::fetch() and ArtistRequest::fetch() is the same,
// so we use this helper function for both.

RequestBase::ImageData common_fetch(RequestBase* request, shared_ptr<ArtReply> const& artreply) noexcept
{
    assert(request);

    RequestBase::ImageData image_data(RequestBase::FetchStatus::needs_download,
                                      RequestBase::CachePolicy::cache_fullsize,
                                      Location::remote);
    if (!artreply)
    {
        return image_data;
    }
    request->set_error_message(artreply->error_string().toStdString());  // Default for below.
    switch (artreply->status())
    {
        case ArtReply::Status::success:
        {
            try
            {
                auto raw_data = artreply->data();
                Image full_size(string(raw_data.constData(), raw_data.size()));
                return RequestBase::ImageData(full_size, RequestBase::CachePolicy::cache_fullsize, Location::remote);
            }
            catch (std::exception const& e)
            {
                request->set_error_message(e.what());
                image_data.status = RequestBase::FetchStatus::hard_error;
                return image_data;
            }
        }
        case ArtReply::Status::not_found:
            image_data.status = RequestBase::FetchStatus::not_found;
            return image_data;
        case ArtReply::Status::temporary_error:
            image_data.status = RequestBase::FetchStatus::temporary_error;
            return image_data;
        case ArtReply::Status::hard_error:
            image_data.status = RequestBase::FetchStatus::hard_error;
            return image_data;
        case ArtReply::Status::network_down:
            // LCOV_EXCL_START
            image_data.status = RequestBase::FetchStatus::network_down;
            return image_data;
            // LCOV_EXCL_STOP
        default:
            abort();  // LCOV_EXCL_LINE  // Impossible
    }
}

}  // namespace

RequestBase::ImageData AlbumRequest::fetch(QSize const& /*size_hint*/) noexcept
{
    return common_fetch(this, artreply_);
}

void AlbumRequest::download(chrono::milliseconds timeout)
{
    if (timeout.count() == 0)
    {
        timeout = timeout_;
    }
    artreply_ = downloader()->download_album(QString::fromStdString(artist_), QString::fromStdString(album_), timeout);
    connect(artreply_.get(), &ArtReply::finished, this, &AlbumRequest::downloadFinished, Qt::DirectConnection);
}

ArtistRequest::ArtistRequest(Thumbnailer* thumbnailer,
                             string const& artist,
                             string const& album,
                             QSize const& requested_size,
                             chrono::milliseconds timeout)
    : RequestBase(thumbnailer, artist + '\0' + album + '\0' + "artist", requested_size, timeout)
    , artist_(artist)
    , album_(album)
{
}

RequestBase::ImageData ArtistRequest::fetch(QSize const& /*size_hint*/) noexcept
{
    return common_fetch(this, artreply_);
}

void ArtistRequest::download(chrono::milliseconds timeout)
{
    if (timeout.count() == 0)
    {
        timeout = timeout_;
    }
    artreply_ = downloader()->download_artist(QString::fromStdString(artist_), QString::fromStdString(album_), timeout);
    connect(artreply_.get(), &ArtReply::finished, this, &ArtistRequest::downloadFinished, Qt::DirectConnection);
}

namespace
{

// Key under which we store the last time we had a network failure.
// No entry in the failure cache ever has this key because keys for
// local files include inode number and modification times.
string const LAST_NETWORK_FAIL_TIME_KEY = "/*** LAST_NETWORK_FAIL_TIME ***/";

}

Thumbnailer::Thumbnailer()
    : downloader_(new UbuntuServerDownloader())
{
    string xdg_base = g_get_user_cache_dir();  // Always returns something, even HOME and XDG_CACHE_HOME are not set.
    string cache_dir = xdg_base + "/unity-thumbnailer";
    make_directories(cache_dir, 0700);

    try
    {
        Settings settings;
        full_size_cache_ = move(PersistentCacheHelper::open(cache_dir + "/images",
                                                            settings.full_size_cache_size() * 1024 * 1024,
                                                            core::CacheDiscardPolicy::lru_only));
        thumbnail_cache_ = move(PersistentCacheHelper::open(cache_dir + "/thumbnails",
                                                            settings.thumbnail_cache_size() * 1024 * 1024,
                                                            core::CacheDiscardPolicy::lru_only));
        failure_cache_ = move(PersistentCacheHelper::open(cache_dir + "/failures",
                                                          settings.failure_cache_size() * 1024 * 1024,
                                                          core::CacheDiscardPolicy::lru_ttl));
        max_size_ = settings.max_thumbnail_size();
        retry_not_found_hours_ = settings.retry_not_found_hours();
        retry_error_hours_ = settings.retry_error_hours();
        extraction_timeout_ = chrono::milliseconds(settings.extraction_timeout() * 1000);

        // For transient remote errors, we read the time at which the last failure
        // happened. The destructor writes that time back out, so we remember
        // the time across re-starts. We call contains_key() to avoid generating
        // a bogus cache miss in the stats.
        if (failure_cache_->contains_key(LAST_NETWORK_FAIL_TIME_KEY))
        {
            auto last_network_fail_time = failure_cache_->get(LAST_NETWORK_FAIL_TIME_KEY);
            nw_fail_time_ = chrono::system_clock::time_point(chrono::seconds(stoll(*last_network_fail_time)));
        }
    }
    catch (std::exception const& e)
    {
        throw runtime_error(string("Thumbnailer(): Cannot instantiate cache: ") + e.what());
    }
}

Thumbnailer::~Thumbnailer()
{
    try
    {
        auto seconds = chrono::duration_cast<chrono::seconds>(nw_fail_time_.time_since_epoch()).count();
        failure_cache_->put(LAST_NETWORK_FAIL_TIME_KEY, to_string(seconds));
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        qDebug() << "~Thumbnailer(): cannot update network failure time:" << e.what();
    }
    catch (...)
    {
        qDebug() << "~Thumbnailer(): cannot update network failure time: unknown exception";
    }
    // LCOV_EXCL_STOP
}

unique_ptr<ThumbnailRequest> Thumbnailer::get_thumbnail(string const& filename,
                                                        QSize const& requested_size)
{
    assert(!filename.empty());

    try
    {
        return unique_ptr<ThumbnailRequest>(
                new LocalThumbnailRequest(this, filename, requested_size, extraction_timeout_));
    }
    catch (std::exception const&)
    {
        throw unity::ResourceException("Thumbnailer::get_thumbnail()");
    }
}

unique_ptr<ThumbnailRequest> Thumbnailer::get_album_art(string const& artist,
                                                        string const& album,
                                                        QSize const& requested_size)
{
    if (album.empty())
    {
        throw unity::InvalidArgumentException("Thumbnailer::get_album_art(): album is empty");
    }

    try
    {
        return unique_ptr<ThumbnailRequest>(new AlbumRequest(this, artist, album, requested_size, extraction_timeout_));
    }
    // LCOV_EXCL_START  // Currently won't throw, we are defensive here.
    catch (std::exception const&)
    {
        throw unity::ResourceException("Thumbnailer::get_album_art()");  // LCOV_EXCL_LINE
    }
    // LCOV_EXCL_STOP
}

unique_ptr<ThumbnailRequest> Thumbnailer::get_artist_art(string const& artist,
                                                         string const& album,
                                                         QSize const& requested_size)
{
    if (artist.empty())
    {
        throw unity::InvalidArgumentException("Thumbnailer::get_artist_art(): artist is empty");
    }

    try
    {
        return unique_ptr<ThumbnailRequest>(new ArtistRequest(this, artist, album, requested_size, extraction_timeout_));
    }
    // LCOV_EXCL_START  // Currently won't throw, we are defensive here.
    catch (std::exception const&)
    {
        throw unity::ResourceException("Thumbnailer::get_artist_art()");
    }
    // LCOV_EXCL_STOP
}

Thumbnailer::AllStats Thumbnailer::stats() const
{
    return AllStats{full_size_cache_->stats(), thumbnail_cache_->stats(), failure_cache_->stats()};
}

Thumbnailer::CacheVec Thumbnailer::select_caches(CacheSelector selector) const
{
    CacheVec v;
    switch (selector)
    {
        case Thumbnailer::CacheSelector::full_size_cache:
            v.push_back(full_size_cache_.get());
            break;
        case Thumbnailer::CacheSelector::thumbnail_cache:
            v.push_back(thumbnail_cache_.get());
            break;
        case Thumbnailer::CacheSelector::failure_cache:
            v.push_back(failure_cache_.get());
            break;
        default:
            v.push_back(full_size_cache_.get());
            v.push_back(thumbnail_cache_.get());
            v.push_back(failure_cache_.get());
            break;
    }
    return v;
}

namespace
{

char const* cache_name(Thumbnailer::CacheSelector selector)
{
    switch (selector)
    {
        case Thumbnailer::CacheSelector::full_size_cache:
            return "image cache";
        case Thumbnailer::CacheSelector::thumbnail_cache:
            return "thumbnail cache";
        case Thumbnailer::CacheSelector::failure_cache:
            return "failure cache";
        default:
            return "all caches";
    }
}

}

void Thumbnailer::clear_stats(CacheSelector selector)
{
    for (auto c : select_caches(selector))
    {
        c->clear_stats();
    }
    qDebug() << "reset statistics for" << cache_name(selector);
}

void Thumbnailer::clear(CacheSelector selector)
{
    for (auto c : select_caches(selector))
    {
        c->invalidate();
    }
    if (selector == Thumbnailer::CacheSelector::failure_cache ||
        selector == Thumbnailer::CacheSelector::all)
    {
        // Force retry on next remote retrieval even if we
        // are still within the timeout period.
        nw_fail_time_ = chrono::system_clock::time_point();
    }
    qDebug() << "cleared" << cache_name(selector);
}

void Thumbnailer::compact(CacheSelector selector)
{
    qDebug() << "compacting" << cache_name(selector);
    for (auto c : select_caches(selector))
    {
        c->compact();
    }
    qDebug() << "completed compacting" << cache_name(selector);
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity

#include "thumbnailer.moc"
