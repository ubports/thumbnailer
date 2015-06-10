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

#include <QCoreApplication>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusReply>
#include <QEvent>
#include <QDebug>
#include <QMimeDatabase>

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
    connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &ThumbnailerImageResponse::callFinished);
}

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   QString const& default_image)
    : requested_size_(requested_size)
    , default_image_(default_image)
{
    loadDefaultImage();
    // Queue the signal emission so there is time for the caller to connect.
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

ThumbnailerImageResponse::~ThumbnailerImageResponse() = default;

QQuickTextureFactory* ThumbnailerImageResponse::textureFactory() const
{
    return texture_;
}

void ThumbnailerImageResponse::cancel()
{
    qDebug() << "Image response cancelled";
    watcher_.reset();
}

void ThumbnailerImageResponse::loadDefaultImage()
{
    char const* env_default = getenv("THUMBNAILER_TEST_DEFAULT_IMAGE");
    QImage result;
    result.load(env_default ? QString(env_default) : default_image_);
    texture_ = QQuickTextureFactory::textureFactoryForImage(result);
}

void ThumbnailerImageResponse::callFinished()
{
    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher_.get();
    if (!reply.isValid())
    {
        qWarning() << "ThumbnailerImageResponse::callFinished(): D-Bus error: " << reply.error().message();
        loadDefaultImage();
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
    catch (const std::exception& e)
    {
        qWarning() << "ThumbnailerImageResponse::callFinished(): Album art loader failed: " << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerImageResponse::callFinished(): unknown exception";
    }

    loadDefaultImage();
    Q_EMIT finished();
    // LCOV_EXCL_STOP
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
