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
 *          Pawel Stolowski <pawel.stolowski@canonical.com>
 */

#include "artistartgenerator.h"

#include <QDebug>
#include <QUrlQuery>

#include "thumbnailerimageresponse.h"

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ArtistArtGenerator::ArtistArtGenerator(std::shared_ptr<unity::thumbnailer::qt::Thumbnailer> const& thumbnailer)
    : QQuickAsyncImageProvider()
    , thumbnailer(thumbnailer)
{
}

QQuickImageResponse* ArtistArtGenerator::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    QUrlQuery query(id);
    if (!query.hasQueryItem(QStringLiteral("artist")) || !query.hasQueryItem(QStringLiteral("album")))
    {
        qWarning() << "ArtistArtGenerator::requestImageResponse(): Invalid artistart uri:" << id;
        return new ThumbnailerImageResponse("Invalid artistart ID: " + id);
    }

    const QString artist = query.queryItemValue(QStringLiteral("artist"), QUrl::FullyDecoded);
    const QString album = query.queryItemValue(QStringLiteral("album"), QUrl::FullyDecoded);

    auto request = thumbnailer->getArtistArt(artist, album, requestedSize);
    return new ThumbnailerImageResponse(request);
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
