/*
 * Copyright (C) 2015 Canonical Ltd.
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
 * Authored by: Pawel Stolowski <pawel.stolowski@canonical.com>
 *              Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
#include <QObject>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <chrono>
#include <memory>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class ArtReply;

class ArtDownloader : public QObject
{
    Q_OBJECT
public:
    Q_DISABLE_COPY(ArtDownloader)

    explicit ArtDownloader(QObject* parent = nullptr);
    virtual ~ArtDownloader() = default;

    virtual std::shared_ptr<ArtReply> download_album(QString const& artist,
                                                     QString const& album,
                                                     std::chrono::milliseconds timeout) = 0;
    virtual std::shared_ptr<ArtReply> download_artist(QString const& artist,
                                                      QString const& album,
                                                      std::chrono::milliseconds timeout) = 0;

protected:
    void assert_valid_url(QUrl const& url) const;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
