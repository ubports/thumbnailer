/*
 * Copyright (C) 2015 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <internal/artdownloader.h>

#include <QUrl>

#include <stdexcept>

using namespace unity::thumbnailer::internal;

ArtDownloader::ArtDownloader(QObject* parent)
    : QObject(parent)
{
}

void ArtDownloader::assert_valid_url(QUrl const& url) const
{
    // LCOV_EXCL_START
    if (!url.isValid())
    {
        throw std::logic_error("ArtDownloader::assert_valid_url(): The url provided is not valid");
    }
    // LCOV_EXCL_STOP
}
