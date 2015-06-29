/*
 * Copyright 2015 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <thumbnailerimageresponse.h>
#include <artgeneratorcommon.h>

#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDebug>

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   std::unique_ptr<QDBusPendingCallWatcher>&& watcher)
    : requested_size_(requested_size)
    , watcher_(std::move(watcher))
{
    connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &ThumbnailerImageResponse::dbusCallFinished);
}

ThumbnailerImageResponse::ThumbnailerImageResponse(QString const& error_message)
    : error_message_(error_message)
{
    // Queue the signal emission so there is time for the caller to connect.
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

ThumbnailerImageResponse::~ThumbnailerImageResponse() = default;

QQuickTextureFactory* ThumbnailerImageResponse::textureFactory() const
{
    return texture_;
}

QString ThumbnailerImageResponse::errorString() const
{
    return error_message_;
}

void ThumbnailerImageResponse::cancel()
{
    // Deleting the pending call watcher (which should hold the only
    // reference to the pending call at this point) tells Qt that we
    // are no longer interested in the reply.  The destruction will
    // also clear up the signal connections.
    watcher_.reset();
}

void ThumbnailerImageResponse::dbusCallFinished()
{
    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher_.get();
    if (!reply.isValid())
    {
        error_message_ = reply.error().message();
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): D-Bus error: " << error_message_;
        Q_EMIT finished();
        return;
    }

    try
    {
        QSize realSize;
        QImage image = imageFromFd(reply.value().fileDescriptor(), &realSize, requested_size_);
        texture_ = QQuickTextureFactory::textureFactoryForImage(image);
        Q_EMIT finished();
        return;
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): Album art loader failed: " << e.what();
        error_message_ = e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): unknown exception";
        error_message_ = "unknown error";
    }

    Q_EMIT finished();
    // LCOV_EXCL_STOP
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
