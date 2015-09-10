/*
 * Copyright 2013 Canonical Ltd.
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
 * Authors: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 */

#include "thumbnailgenerator.h"

#include <QDBusConnection>
#include <QDebug>
#include <QMimeDatabase>
#include <QUrl>

#include <settings.h>
#include "thumbnailerimageresponse.h"

namespace
{

const char* DEFAULT_VIDEO_ART = "/usr/share/thumbnailer/icons/video_missing.png";
const char* DEFAULT_ALBUM_ART = "/usr/share/thumbnailer/icons/album_missing.png";

QString default_image_based_on_mime(QString const &id)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(id);

    if (mime.name().contains("audio"))
    {
        return DEFAULT_ALBUM_ART;
    }
    else if (mime.name().contains("video"))
    {
        return DEFAULT_VIDEO_ART;  // LCOV_EXCL_LINE  // Being lazy here: default art is about to go away.
    }
    return DEFAULT_ALBUM_ART;
}

}  // namespace

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ThumbnailGenerator::ThumbnailGenerator()
    : QQuickAsyncImageProvider()
    , backlog_limiter(Settings().max_backlog())
{
}

QQuickImageResponse* ThumbnailGenerator::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QSize size = requestedSize;
    // TODO: Turn this into an error soonish.
    if (!requestedSize.isValid())
    {
        qWarning().nospace() << "ThumbnailGenerator::requestImageResponse(): deprecated invalid QSize: "
                             << requestedSize << ". This feature will be removed soon. Pass the desired size instead.";
        // Size will be adjusted by the service to 128x128.
    }

    /* Allow appending a query string (e.g. ?something=timestamp)
     * to the id and then ignore it.
     * This is workaround to force reloading a thumbnail when it has
     * the same file name on disk but we know the content has changed.
     * It is necessary because in such a situation the QML image cache
     * will kick in and this ImageProvider will never get called.
     * The only "solution" is setting Image.cache = false, but in some
     * cases we don't want to do that for performance reasons, so this
     * is the only way around the issue for now. */
    QString src_path = QUrl(id).path();

    if (!thumbnailer)
    {
        // Create connection here and not on the constructor, so it belongs to the proper thread.
        thumbnailer.reset(
            new unity::thumbnailer::qt::Thumbnailer(
                QDBusConnection::connectToBus(
                    QDBusConnection::SessionBus, "thumbnail_generator_dbus_connection")));
    }

    // Schedule dbus call
    auto job = [this, src_path, size]
    {
        return thumbnailer->getThumbnail(src_path, size);
    };
    return new ThumbnailerImageResponse(size, default_image_based_on_mime(id), &backlog_limiter, job);
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
