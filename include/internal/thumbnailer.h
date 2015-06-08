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
 */

#pragma once

#include <internal/artdownloader.h>

#include <core/persistent_cache_stats.h>
#include <core/persistent_string_cache.h>
#include <QObject>
#include <QSize>

#include <chrono>
#include <memory>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class RequestBase;

class ThumbnailRequest : public QObject
{
    Q_OBJECT
public:
    Q_DISABLE_COPY(ThumbnailRequest)

    ThumbnailRequest() = default;
    virtual ~ThumbnailRequest() = default;

    enum class FetchStatus
    {
        needs_download,
        downloaded,
        not_found,
        no_network,
        error
    };

    // Returns the empty string if the thumbnail data needs to be
    // downloaded to complete the request. If this happens, call
    // download() and wait for downloadFinished signal to fire, then
    // call thumbnail() again.
    virtual std::string thumbnail() = 0;
    // TODO: Timeout should be configurable?
    virtual void download(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000)) = 0;

    virtual std::string const& key() const = 0;
Q_SIGNALS:
    void downloadFinished();
};

/**
 * This class provides a way to generate and access
 * thumbnails of video, audio and image files.
 *
 * All methods are blocking.
 *
 * All methods are thread safe.
 *
 * Errors are reported as exceptions.
 */

class Thumbnailer
{
public:
    Thumbnailer();
    ~Thumbnailer();

    Thumbnailer(Thumbnailer const&) = delete;
    Thumbnailer& operator=(Thumbnailer const&) = delete;

    /**
     * Gets a thumbnail of the given input file in the requested size.
     *
     * Return value is the thumbnail image as a string.
     * If the thumbnail could not be generated, an empty string is returned.
     */
    std::unique_ptr<ThumbnailRequest> get_thumbnail(std::string const& filename,
                                                    int filename_fd,
                                                    QSize const& requested_size);

    /**
     * Gets album art for the given artist an album.
     */
    std::unique_ptr<ThumbnailRequest> get_album_art(std::string const& artist,
                                                    std::string const& album,
                                                    QSize const& requested_size);

    /**
     * Gets artist art for the given artist and album.
     */
    std::unique_ptr<ThumbnailRequest> get_artist_art(std::string const& artist,
                                                     std::string const& album,
                                                     QSize const& requested_size);

    struct AllStats
    {
        core::PersistentCacheStats full_size_stats;
        core::PersistentCacheStats thumbnail_stats;
        core::PersistentCacheStats failure_stats;
    };

    AllStats stats() const;

private:
    ArtDownloader* downloader() const
    {
        return downloader_.get();
    }

    core::PersistentStringCache::UPtr full_size_cache_;  // Small cache of full (original) size images.
    core::PersistentStringCache::UPtr thumbnail_cache_;  // Large cache of scaled images.
    core::PersistentStringCache::UPtr failure_cache_;    // Cache for failed attempts (value is always empty).
    std::unique_ptr<ArtDownloader> downloader_;

    friend class RequestBase;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
