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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */
#pragma once

#include <QObject>

class QDBusConnection;

namespace unity
{

namespace thumbnailer
{

namespace qt
{

namespace internal
{
class ThumbnailerImpl;
class RequestImpl;
}

/**
\brief Holds a thumbnailer request.

This class stores the state of an in-progress or completed thumbnail request.
*/

class Q_DECL_EXPORT Request : public QObject
{
    Q_OBJECT
public:
    /// @cond
    Q_DISABLE_COPY(Request)
    ~Request();
    /// @endcond

    /**
    \brief Returns whether the request has completed.

    \return `false` if the request is still in progress. Otherwise, the return
    value is `true` (whether the request completed successfully or not).
    */
    bool isFinished() const;

    /**
    \brief Returns the thumbnail.
    \return A valid QImage if the request was successful and an empty `QImage`, otherwise.
    */
    QImage image() const;

    /**
    \brief Returns the error message for a failed request.
    \return The error message in case of a failure and an empty `QString`, otherwise.
    */
    QString errorMessage() const;

    /**
    \brief Returns whether the request completed successfully.
    \return `true` if the request completed successfully. Otherwise, if the request is still
    in progress or has failed, the return value is `false`.
    */
    bool isValid() const;

    /**
    \brief Blocks the calling thread until the request completes.

    \warning Calling this function from the main (GUI) thread might cause your user interface to freeze.
    */
    void waitForFinished();

Q_SIGNALS:
    /**
    \brief This signal is emitted when the request completes.
    */
    void finished();

private:
    QScopedPointer<internal::RequestImpl> p_;
    explicit Request(internal::RequestImpl* impl);

    friend class internal::ThumbnailerImpl;
};

/**
\brief Class to obtain thumbnail images for various media types.

Class Thumbnailer provides thumbnail images for local media (image, audio, and video) files, as well
as album covers and artist images for many musicians and bands.

Most common image formats, such as PNG, JPEG, BMP, and so on, are recognized. For streaming media, the
recognized formats depend in the installed GStreamer codecs.

For local media files, thumbnails are extracted directly from the file. (For audio files, this
requires the file to contain embedded artwork.) For album covers and artist images,
artwork is downloaded from a remote image server (<B>dash.ubuntu.com</B>) that maintains a large
database of albums and musicians.

The requested size for a thumbnail specifies a bounding box (in pixels) of type `QSize` to which
the thumbnail will be scaled.
(The aspect ratio of the original image is preserved.) Passing a `QSize(0, `<i>n</i>`)` or `QSize(`<i>n</i>`, 0)`
defines a square bounding box of <i>n</i> pixels. To obtain an image in its original size, pass `QSize(0, 0)`.
Sizes with one or both dimensions less than zero return an error.

Original images are never scaled up, so the returned thumbnail may be smaller than its requested size.

All methods are asynchronous and are guaranteed not to block.

The return value is a shared pointer to a \link unity::thumbnailer::qt::Request Request\endlink instance that
provides access to the scaled thumbnail (or an error message).
*/

class Q_DECL_EXPORT Thumbnailer final
{
public:
    /// @cond
    Q_DISABLE_COPY(Thumbnailer)

    Thumbnailer();
    explicit Thumbnailer(QDBusConnection const& connection);
    ~Thumbnailer();
    /// @endcond

    /**
    \brief Retrieves a thumbnail for an album cover from the remote image server.
    \param artist The name of the artist.
    \param album The name of the album.
    \param requestedSize The bounding box for the thumbnail.
    \return A `QSharedPointer` to a unity::thumbnailer::qt::Request holding the request state.
    */
    QSharedPointer<Request> getAlbumArt(QString const& artist, QString const& album, QSize const& requestedSize);

    /**
    \brief Retrieves a thumbnail for an artist from the remote image server.
    \param artist The name of the artist.
    \param album The name of the album.
    \param requestedSize The bounding box for the thumbnail.
    \return A `QSharedPointer` to a unity::thumbnailer::qt::Request holding the request state.
    */
    QSharedPointer<Request> getArtistArt(QString const& artist, QString const& album, QSize const& requestedSize);

    /**
    \brief Extracts a thumbnail from a media file.
    \param filePath The path to the file to extract the thumbnail from.
    \param requestedSize The bounding box for the thumbnail.
    \return A `QSharedPointer` to a unity::thumbnailer::qt::Request holding the request state.
    */
    QSharedPointer<Request> getThumbnail(QString const& filePath, QSize const& requestedSize);

private:
    QScopedPointer<internal::ThumbnailerImpl> p_;
};

}  // namespace qt

}  // namespace thumbnailer

}  // namespace unity
