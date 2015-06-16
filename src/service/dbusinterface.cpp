/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of version 3 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dbusinterface.h"

#include <internal/trace.h>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{
char const ART_ERROR[] = "com.canonical.Thumbnailer.Error.Failed";
}

namespace unity
{

namespace thumbnailer
{

namespace service
{

DBusInterface::DBusInterface(shared_ptr<Thumbnailer> const& thumbnailer, QObject* parent)
    : QObject(parent)
    , thumbnailer_(thumbnailer)
    , check_thread_pool_(make_shared<QThreadPool>())
    , create_thread_pool_(make_shared<QThreadPool>())
    , download_limiter_(settings_.max_downloads())
    , extraction_limiter_(settings_.max_extractions())
{
}

DBusInterface::~DBusInterface()
{
}

CredentialsCache& DBusInterface::credentials()
{
    if (!credentials_)
    {
        credentials_.reset(new CredentialsCache(connection()));
    }
    return *credentials_.get();
}

QDBusUnixFileDescriptor DBusInterface::GetAlbumArt(QString const& artist,
                                                   QString const& album,
                                                   QSize const& requestedSize)
{
    try
    {
        QString details;
        QTextStream s(&details);
        s << "album: " << artist << "/" << album << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
        auto request = thumbnailer_->get_album_art(artist.toStdString(), album.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 download_limiter_, credentials(),
                                 std::move(request), details));
    }
    // LCOV_EXCL_START
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetArtistArt(): " + artist + "/" + album + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, e.what());
    }
    // LCOV_EXCL_STOP
    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor DBusInterface::GetArtistArt(QString const& artist,
                                                    QString const& album,
                                                    QSize const& requestedSize)
{
    try
    {
        QString details;
        QTextStream s(&details);
        s << "album: " << artist << "/" << album << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
        auto request = thumbnailer_->get_artist_art(artist.toStdString(), album.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 download_limiter_, credentials(),
                                 std::move(request), details));
    }
    // LCOV_EXCL_START
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetArtistArt(): " + artist + "/" + album + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, msg);
    }
    // LCOV_EXCL_STOP
    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor DBusInterface::GetThumbnail(QString const& filename,
                                                    QDBusUnixFileDescriptor const& filename_fd,
                                                    QSize const& requestedSize)
{
    std::unique_ptr<ThumbnailRequest> request;

    try
    {
        QString details;
        QTextStream s(&details);
        s << "thumbnail: " << filename << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
        auto request = thumbnailer_->get_thumbnail(filename.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 extraction_limiter_, credentials(),
                                 std::move(request), details));
    }
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetThumbnail(): " + filename + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, msg);
    }
    return QDBusUnixFileDescriptor();
}

void DBusInterface::queueRequest(Handler* handler)
{
    requests_.emplace(handler, std::unique_ptr<Handler>(handler));
    Q_EMIT endInactivity();
    connect(handler, &Handler::finished, this, &DBusInterface::requestFinished);
    setDelayedReply(true);

    std::vector<Handler*> &requests_for_key = request_keys_[handler->key()];
    if (requests_for_key.size() == 0)
    {
        /* There are no other concurrent requests for this item, so
         * begin immediately. */
        handler->begin();
    }
    else
    {
        /* There are other requests for this item, so chain this
         * request to wait for them to complete first.  This way we
         * can take advantage of any cached downloads or failures. */
        // TODO: should record tiem spent in queue
        connect(requests_for_key.back(), &Handler::finished,
                handler, &Handler::begin);
    }
    requests_for_key.push_back(handler);
}

void DBusInterface::requestFinished()
{
    Handler* handler = static_cast<Handler*>(sender());
    try
    {
        auto& h = requests_.at(handler);
        h.release();
        requests_.erase(handler);
    }
    // LCOV_EXCL_START
    catch (std::out_of_range const& e)
    {
        qWarning() << "finished() called on unknown handler" << handler;
    }
    // LCOV_EXCL_STOP

    // Remove ourselves from the chain of requests
    std::vector<Handler*> &requests_for_key = request_keys_[handler->key()];
    requests_for_key.erase(
        std::remove(requests_for_key.begin(), requests_for_key.end(), handler),
        requests_for_key.end());
    if (requests_for_key.size() == 0)
    {
        request_keys_.erase(handler->key());
    }

    if (requests_.empty())
    {
        Q_EMIT startInactivity();
    }
    // Queue deletion of handler when we re-enter the event loop.
    handler->deleteLater();

    QString msg;
    QTextStream s(&msg);
    s.setRealNumberNotation(QTextStream::FixedNotation);
    s << handler->details() << ": " << double(handler->completion_time().count()) / 1000000 << " sec";
    auto download_time = double(handler->download_time().count()) / 1000000;
    if (download_time > 0)
    {
        s << " [" << download_time << " sec]";
    }
    s << " (" << handler->status() << ")";
    qDebug() << msg;
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
