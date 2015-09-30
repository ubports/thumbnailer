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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#pragma once

#include "credentialscache.h"
#include "inactivityhandler.h"
#include <internal/thumbnailer.h>
#include <ratelimiter.h>

#include <memory>
#include <string>

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QSize>

class QThreadPool;

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct HandlerPrivate;

class Handler : public QObject
{
    Q_OBJECT
public:
    Handler(QDBusConnection const& bus,
            QDBusMessage const& message,
            std::shared_ptr<QThreadPool> const& check_pool,
            std::shared_ptr<QThreadPool> const& create_pool,
            RateLimiter& limiter,
            CredentialsCache& creds,
            InactivityHandler& inactivity_handler,
            std::unique_ptr<internal::ThumbnailRequest>&& request,
            QString const& details);
    ~Handler();

    Handler(Handler const&) = delete;
    Handler& operator=(Handler&) = delete;

    std::string const& key() const;
    std::chrono::microseconds completion_time() const;  // End-to-end time taken.
    std::chrono::microseconds queued_time() const;      // Time spent waiting in download/extract queue.
    std::chrono::microseconds download_time() const;    // Time of that for download/extract, incl. queueing time.
    QString details() const;
    QString status() const;

public Q_SLOTS:
    void begin();

private Q_SLOTS:
    void checkFinished();
    void downloadFinished();
    void createFinished();

Q_SIGNALS:
    void finished();

private:
    void sendThumbnail(QDBusUnixFileDescriptor const& unix_fd);
    void sendError(QString const& error);
    void gotCredentials(CredentialsCache::Credentials const& credentials);
    QDBusUnixFileDescriptor check();
    QDBusUnixFileDescriptor create();

    std::unique_ptr<HandlerPrivate> p;
};

}  // namespace unity

}  // namespace thumbnailer

}  // namespace service
