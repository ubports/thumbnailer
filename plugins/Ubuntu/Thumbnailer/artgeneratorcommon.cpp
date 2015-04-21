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

#include "artgeneratorcommon.h"
#include <QFile>
#include <QImageReader>

namespace unity {
namespace thumbnailer {
namespace qml {

QImage imageFromFd(int fd, QSize *realSize, const QSize &requestedSize)
{
    QFile file;
    file.open(fd, QIODevice::ReadOnly);
    QImageReader reader;
    reader.setDevice(&file);
    QSize imageSize = reader.size();
    if (requestedSize.isValid() && (
            (requestedSize.width() > 0 &&
             imageSize.width() > requestedSize.width()) ||
            (requestedSize.height() > 0 &&
             imageSize.height() > requestedSize.height()))) {
        QSize validRequestedSize = requestedSize;
        if (validRequestedSize.width() == 0) {
            validRequestedSize.setWidth(imageSize.width());
        }
        if (validRequestedSize.height() == 0) {
            validRequestedSize.setHeight(imageSize.height());
        }
        imageSize.scale(validRequestedSize, Qt::KeepAspectRatio);
        reader.setScaledSize(imageSize);
    }
    QImage image = reader.read();
    *realSize = image.size();
    return image;
}

}
}
}
