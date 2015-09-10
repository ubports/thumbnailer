/*
 * Copyright 2014 Canonical Ltd.
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
 *          James Henstridge <james.henstridge@canonical.com>
 */

#include "albumartgenerator.h"

#include <QDBusConnection>
#include <QDebug>
#include <QUrlQuery>

#include <settings.h>
#include "thumbnailerimageresponse.h"

namespace
{

const char DEFAULT_ALBUM_ART[] = "/usr/share/thumbnailer/icons/album_missing.png";

}  // namespace

namespace unity
{

namespace thumbnailer
{

namespace qml
{

AlbumArtGenerator::AlbumArtGenerator()
    : QQuickAsyncImageProvider()
    , backlog_limiter(Settings().max_backlog())
{
}

QQuickImageResponse* AlbumArtGenerator::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QSize size = requestedSize;
    // TODO: Turn this into an error soonish.
    if (!requestedSize.isValid())
    {
        qWarning().nospace() << "AlbumArtGenerator::requestImageResponse(): deprecated invalid QSize: "
                             << requestedSize << ". This feature will be removed soon. Pass the desired size instead.";
        // Size will be adjusted by the service to 128x128.
    }

    QUrlQuery query(id);
    if (!query.hasQueryItem("artist") || !query.hasQueryItem("album"))
    {
        qWarning() << "AlbumArtGenerator::requestImageResponse(): Invalid albumart uri:" << id;
        return new ThumbnailerImageResponse(requestedSize, DEFAULT_ALBUM_ART);
    }

    if (!thumbnailer)
    {
        // Create connection here and not on the constructor, so it belongs to the proper thread.
        thumbnailer.reset(
            new unity::thumbnailer::qt::Thumbnailer(
                QDBusConnection::connectToBus(
                    QDBusConnection::SessionBus, "album_art_generator_dbus_connection")));
    }

    const QString artist = query.queryItemValue("artist", QUrl::FullyDecoded);
    const QString album = query.queryItemValue("album", QUrl::FullyDecoded);

    // Schedule dbus call
    auto job = [this, artist, album, size]
    {
        return thumbnailer->getAlbumArt(artist, album, size);
    };
    return new ThumbnailerImageResponse(size, DEFAULT_ALBUM_ART, &backlog_limiter, job);
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
