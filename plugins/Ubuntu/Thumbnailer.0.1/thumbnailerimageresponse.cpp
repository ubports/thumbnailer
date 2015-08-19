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
#include <utils/artgeneratorcommon.h>

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
                                                   QString const& default_image,
                                                   std::unique_ptr<QDBusPendingCallWatcher>&& watcher)
    : requested_size_(requested_size)
    , default_image_(default_image)
    , watcher_(std::move(watcher))
{
    connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &ThumbnailerImageResponse::dbusCallFinished);
}

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   QString const& default_image)
    : requested_size_(requested_size)
    , default_image_(default_image)
{
    // Queue the signal emission so there is time for the caller to connect.
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

ThumbnailerImageResponse::~ThumbnailerImageResponse() = default;

QQuickTextureFactory* ThumbnailerImageResponse::textureFactory() const
{
    if (!image_.isNull()) {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    } else {
        char const* env_default = getenv("THUMBNAILER_TEST_DEFAULT_IMAGE");
        QImage aux;
        aux.load(env_default ? QString(env_default) : default_image_);
        return QQuickTextureFactory::textureFactoryForImage(aux);
    }
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
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): D-Bus error: " << reply.error().message();
        Q_EMIT finished();
        return;
    }

    try
    {
        QSize realSize;
        image_ = internal::imageFromFd(reply.value().fileDescriptor(), &realSize, requested_size_);
        Q_EMIT finished();
        return;
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): Album art loader failed: " << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): unknown exception";
    }

    Q_EMIT finished();
    // LCOV_EXCL_STOP
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
