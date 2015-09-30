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

#include <QDebug>
#include <QUrlQuery>

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

AlbumArtGenerator::AlbumArtGenerator(std::shared_ptr<unity::thumbnailer::qt::Thumbnailer> const& thumbnailer)
    : QQuickAsyncImageProvider()
    , thumbnailer(thumbnailer)
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
    if (!query.hasQueryItem(QStringLiteral("artist")) || !query.hasQueryItem(QStringLiteral("album")))
    {
        qWarning() << "AlbumArtGenerator::requestImageResponse(): Invalid albumart uri:" << id;
        return new ThumbnailerImageResponse(requestedSize, DEFAULT_ALBUM_ART);
    }

    const QString artist = query.queryItemValue(QStringLiteral("artist"), QUrl::FullyDecoded);
    const QString album = query.queryItemValue(QStringLiteral("album"), QUrl::FullyDecoded);

    auto request = thumbnailer->getAlbumArt(artist, album, size);
    return new ThumbnailerImageResponse(size, DEFAULT_ALBUM_ART, request);
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
