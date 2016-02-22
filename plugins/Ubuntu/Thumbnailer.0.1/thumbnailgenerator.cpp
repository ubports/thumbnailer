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

#include <QDebug>
#include <QUrl>

#include "thumbnailerimageresponse.h"

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ThumbnailGenerator::ThumbnailGenerator(std::shared_ptr<unity::thumbnailer::qt::Thumbnailer> const& thumbnailer)
    : QQuickAsyncImageProvider()
    , thumbnailer(thumbnailer)
{
}

QQuickImageResponse* ThumbnailGenerator::requestImageResponse(const QString& id, const QSize& requestedSize)
{
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

    auto request = thumbnailer->getThumbnail(src_path, requestedSize);
    return new ThumbnailerImageResponse(request);
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
