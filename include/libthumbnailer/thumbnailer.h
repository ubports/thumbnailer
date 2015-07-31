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
#include <QImage>

class QDBusConnection;

namespace unity
{

namespace thumbnailer
{

namespace internal
{
class ThumbnailerImpl;
class RequestImpl;
}

/**
\brief Holds a thumbnailer request.
This object represents and holds all the information related to a thumbnailer request.
When the user invokes any of the available methods in unity::thumbnailer::Thumbnailer he obtains
a QSharedPointer to a unity::thumbnailer::Request, which sends a finished()signal upon its completion.
After it finished the user can query the obtained image, if the request was successful or, in case of error,
get the error string containing the failure report.
*/
class Request : public QObject
{
    Q_OBJECT
public:
    /// @cond
    Q_DISABLE_COPY(Request)
    ~Request();
    /// @endcond

    /**
     \brief Returns if the request already finished.
     At the instantiation of the request this property will be set to false.
     \return true if the request finished, false otherwise
    */
    bool isFinished() const;

    /**
     \brief Returns the thumbnailer image.
     \return A valid QImage if the request was successful, otherwise it returns an empty QImage.
    */
    QImage image() const;

    /**
     \brief Returns the error message after the request finished.
     \return The error string in case of failure, an empty QString otherwise
    */
    QString errorMessage() const;

    /**
     \brief Returns if the request finished successfully.
     \return true if the request finished successfully, false in case of any failure.
    */
    bool finishedSucessfully() const;

    /**
     \brief Blocks the calling thread until the request finishes.
     This method is useful for those users who want to follow a synchronous behavior.
    */
    void waitForFinished();

Q_SIGNALS:
    /**
     \brief Emitted when the request finishes.
    */
    void finished();

private:
    QScopedPointer<internal::RequestImpl> p_;
    explicit Request(internal::RequestImpl* impl);

    friend class internal::ThumbnailerImpl;
};

/**
\brief This class provides a way to generate and access thumbnails of video, audio and image files.
After the user calls any of his public methods to obtain thumbnails it creates a QSharedPointer to a
unity::thumbnailer::Request object which will hold the information and state of that particular request.
*/
class Thumbnailer
{
public:
    /// @cond
    Q_DISABLE_COPY(Thumbnailer)

    Thumbnailer();
    virtual ~Thumbnailer();
    /// @endcond

    /**
     \brief Gets a thumbnail for an album art.
     \param artist the artist name
     \param album the album name
     \param requestedSize the size of the thumbnail we want to obtain.
     \return A QSharedPointer to a unity::thumbnailer::Request holding the request state.
    */
    QSharedPointer<Request> getAlbumArt(QString const& artist, QString const& album, QSize const& requestedSize);

    /**
     \brief Gets a thumbnail for an artist art.
     \param artist the artist name
     \param album the album name
     \param requestedSize the size of the thumbnail we want to obtain.
     \return A QSharedPointer to a unity::thumbnailer::Request holding the request state.
    */
    QSharedPointer<Request> getArtistArt(QString const& artist, QString const& album, QSize const& requestedSize);

    /**
     \brief Gets a thumbnail for the given file.
     \param filePath absolute path to the file to obtain the thumbnail from.
     \param requestedSize the size of the thumbnail we want to obtain.
     \return A QSharedPointer to a unity::thumbnailer::Request holding the request state.
    */
    QSharedPointer<Request> getThumbnail(QString const& filePath, QSize const& requestedSize);

    /// @cond
    // NOTE: this method is provided for testing purposes only.
    void setDbusConnection(QDBusConnection const& connection);
    /// @endcond

private:
    QScopedPointer<internal::ThumbnailerImpl> p_;
};

}  // namespace thumbnailer

}  // namespace unity
