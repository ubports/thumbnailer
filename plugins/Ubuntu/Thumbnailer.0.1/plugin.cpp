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
 * Authors: James Henstridge <james.henstridge@canonical.com>
*/

#include "plugin.h"
#include "albumartgenerator.h"
#include "artistartgenerator.h"
#include "thumbnailgenerator.h"

namespace unity
{
namespace thumbnailer
{
namespace qml
{

void ThumbnailerPlugin::registerTypes(const char* uri)
{
    qmlRegisterTypeNotAvailable(uri, 0, 1, "__ThumbnailerIgnoreMe",
                                "Ignore this: QML plugins must contain at least one type");
}

void ThumbnailerPlugin::initializeEngine(QQmlEngine* engine, const char* uri)
{
    QQmlExtensionPlugin::initializeEngine(engine, uri);

    try
    {
        engine->addImageProvider("albumart", new AlbumArtGenerator());
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register albumart image provider:" << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register albumart image provider.";
    }
    // LCOV_EXCL_STOP

    try
    {
        engine->addImageProvider("artistart", new ArtistArtGenerator());
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register artistart image provider:" << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register artistart image provider.";
    }
    // LCOV_EXCL_STOP

    try
    {
        engine->addImageProvider("thumbnailer", new ThumbnailGenerator());
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register thumbnailer image provider:" << e.what();
    }
    catch (...)
    {
        qWarning() << "ThumbnailerPlugin::initializeEngine(): Failed to register thumbnailer image provider.";
    }
    // LCOV_EXCL_STOP
}
}
}
}
