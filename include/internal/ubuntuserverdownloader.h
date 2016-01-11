/*
 * Copyright (C) 2014 Canonical Ltd.
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

#include <internal/artdownloader.h>

#include <QNetworkAccessManager>

#include <map>
#include <memory>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class UbuntuServerDownloader final : public ArtDownloader  // LCOV_EXCL_LINE  // False negative from gcovr
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif
    Q_OBJECT
#ifdef __clang__
#pragma clang diagnostic pop
#endif
public:
    Q_DISABLE_COPY(UbuntuServerDownloader)

    explicit UbuntuServerDownloader(QObject* parent = nullptr);
    virtual ~UbuntuServerDownloader() = default;

    std::shared_ptr<ArtReply> download_album(QString const& artist,
                                             QString const& album,
                                             std::chrono::milliseconds timeout) override;
    std::shared_ptr<ArtReply> download_artist(QString const& artist,
                                              QString const& album,
                                              std::chrono::milliseconds timeout) override;

    // NOTE: this method is just used for testing purposes.
    // We need to expose the internal QNetworkAccessManager in order to
    // set the networkAccessible accessible and test the code
    // that manages error disconnected situations
    std::shared_ptr<QNetworkAccessManager> network_manager() const;

private:
    std::shared_ptr<ArtReply> download_url(QUrl const& url, std::chrono::milliseconds timeout);

    QString api_key_;
    std::shared_ptr<QNetworkAccessManager> network_manager_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
