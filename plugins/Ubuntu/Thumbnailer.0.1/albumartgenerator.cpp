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

#include "artgeneratorcommon.h"
#include <service/dbus_names.h>
#include "thumbnailerimageresponse.h"

namespace unity
{

namespace thumbnailer
{

namespace qml
{

AlbumArtGenerator::AlbumArtGenerator()
    : QQuickAsyncImageProvider()
{
}

QQuickImageResponse* AlbumArtGenerator::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    // TODO: Turn this into an error soonish.
    if (!requestedSize.isValid())
    {
        qWarning().nospace() << "AlbumArtGenerator::requestImageResponse(): deprecated invalid QSize: "
                             << requestedSize << ". This feature will be removed soon. Pass the desired size instead.";
    }

    QUrlQuery query(id);
    if (!query.hasQueryItem("artist") || !query.hasQueryItem("album"))
    {
        qWarning() << "AlbumArtGenerator::requestImageResponse(): Invalid albumart uri:" << id;
        return new ThumbnailerImageResponse("Invalid albumart uri");
    }

    if (!connection)
    {
        // Create connection here and not on the constructor, so it belongs to the proper thread.
        connection.reset(new QDBusConnection(
            QDBusConnection::connectToBus(QDBusConnection::SessionBus, "album_art_generator_dbus_connection")));
        iface.reset(new ThumbnailerInterface(service::BUS_NAME, service::THUMBNAILER_BUS_PATH, *connection));
    }

    const QString artist = query.queryItemValue("artist", QUrl::FullyDecoded);
    const QString album = query.queryItemValue("album", QUrl::FullyDecoded);

    // perform dbus call
    auto reply = iface->GetAlbumArt(artist, album, requestedSize);
    std::unique_ptr<QDBusPendingCallWatcher> watcher(
        new QDBusPendingCallWatcher(reply));
    return new ThumbnailerImageResponse(requestedSize, std::move(watcher));
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
