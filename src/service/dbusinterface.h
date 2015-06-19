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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#pragma once

#include "credentialscache.h"
#include "handler.h"
#include "ratelimiter.h"

#include <internal/settings.h>

#include <QDBusContext>
#include <QThreadPool>

namespace unity
{

namespace thumbnailer
{

namespace service
{

class DBusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    DBusInterface(std::shared_ptr<unity::thumbnailer::internal::Thumbnailer> const& thumbnailer,
                  QObject* parent = nullptr);
    ~DBusInterface();

    DBusInterface(DBusInterface const&) = delete;
    DBusInterface& operator=(DBusInterface&) = delete;

public Q_SLOTS:
    QDBusUnixFileDescriptor GetAlbumArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QDBusUnixFileDescriptor GetArtistArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QDBusUnixFileDescriptor GetThumbnail(QString const& filename,
                                         QSize const& requestedSize);

private:
    void queueRequest(Handler* handler);

private Q_SLOTS:
    void requestFinished();

Q_SIGNALS:
    void endInactivity();
    void startInactivity();

private:
    CredentialsCache& credentials();

    std::shared_ptr<unity::thumbnailer::internal::Thumbnailer> const& thumbnailer_;
    std::shared_ptr<QThreadPool> check_thread_pool_;
    std::shared_ptr<QThreadPool> create_thread_pool_;
    std::unique_ptr<CredentialsCache> credentials_;
    std::map<Handler*, std::unique_ptr<Handler>> requests_;
    std::map<std::string, std::vector<Handler*>> request_keys_;
    unity::thumbnailer::internal::Settings settings_;
    RateLimiter download_limiter_;
    RateLimiter extraction_limiter_;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
