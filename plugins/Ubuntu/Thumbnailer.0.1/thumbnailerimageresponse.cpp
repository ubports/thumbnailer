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

#include <internal/trace.h>

#include <QCoreApplication>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusReply>
#include <QEvent>
#include <QMimeDatabase>

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ThumbnailerImageResponse::ThumbnailerImageResponse(QString const& id,
                                                   QSize const& requested_size,
                                                   QString const& default_image,
                                                   QDBusPendingCallWatcher *watcher)
    : id_(id)
    , requested_size_(requested_size)
    , texture_(nullptr)
    , default_image_(default_image)
    , watcher_(watcher)
{
    if (watcher_)
    {
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &ThumbnailerImageResponse::dbus_call_finished);
    }
    char const* c_default_image = getenv("THUMBNAILER_TEST_DEFAULT_IMAGE");
    if (c_default_image)
    {
        default_image_ = QString(c_default_image);
    }
}

QQuickTextureFactory* ThumbnailerImageResponse::textureFactory() const
{
    return texture_;
}

void ThumbnailerImageResponse::set_default_image()
{
    QImage result;
    result.load(default_image_);
    texture_ = QQuickTextureFactory::textureFactoryForImage(result);
}

void ThumbnailerImageResponse::finish_later_with_default_image()
{
    set_default_image();
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

void ThumbnailerImageResponse::dbus_call_finished(QDBusPendingCallWatcher* watcher)
{
    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;
    if (!reply.isValid())
    {
        qWarning() << "ThumbnailerImageResponse::dbus_call_finished(): D-Bus error: " << reply.error().message();
        set_default_image();
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
        qWarning() << "ThumbnailerImageResponse::dbus_call_finished(): Album art loader failed: " << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerImageResponse::dbus_call_finished(): unknown exception";
    }

    set_default_image();
    Q_EMIT finished();
    // LCOV_EXCL_STOP
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
