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

#include "handler.h"

#include <internal/raii.h>
#include <internal/safe_strerror.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QThreadPool>

#include <atomic>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{
char const ART_ERROR[] = "com.canonical.Thumbnailer.Error.Failed";

struct ByteArrayOrError
{
    QByteArray ba;
    QString error;
};

}

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct HandlerPrivate
{
    QDBusConnection const bus;
    QDBusMessage const message;
    shared_ptr<QThreadPool> const check_pool;
    shared_ptr<QThreadPool> const create_pool;
    shared_ptr<RateLimiter> const limiter;
    CredentialsCache& creds;
    InactivityHandler& inactivity_handler;
    shared_ptr<ThumbnailRequest> request;
    chrono::system_clock::time_point const start_time;      // Overall start time
    chrono::system_clock::time_point finish_time;           // Overall finish time
    chrono::system_clock::time_point schedule_start_time;   // Time at which download/extract is scheduled.
    chrono::system_clock::time_point download_start_time;   // Time at which download/extract is started.
    chrono::system_clock::time_point download_finish_time;  // Time at which download/extract has completed.
    QString const details;
    QString const status;
    RateLimiter::CancelFunc cancel_func;

    atomic_bool cancelled;                                  // Must be atomic because destructor asynchronously writes to it.
    QFutureWatcher<ByteArrayOrError> checkWatcher;
    QFutureWatcher<ByteArrayOrError> createWatcher;

    HandlerPrivate(QDBusConnection const& bus,
                   QDBusMessage const& message,
                   shared_ptr<QThreadPool> const& check_pool,
                   shared_ptr<QThreadPool> const& create_pool,
                   shared_ptr<RateLimiter> const& limiter,
                   CredentialsCache& creds,
                   InactivityHandler& inactivity_handler,
                   unique_ptr<ThumbnailRequest>&& request,
                   QString const& details)
        : bus(bus)
        , message(message)
        , check_pool(check_pool)
        , create_pool(create_pool)
        , limiter(limiter)
        , creds(creds)
        , inactivity_handler(inactivity_handler)
        , request(move(request))
        , start_time(chrono::system_clock::now())
        , details(details)
        , cancelled(false)
    {
    }
};

Handler::Handler(QDBusConnection const& bus,
                 QDBusMessage const& message,
                 shared_ptr<QThreadPool> const& check_pool,
                 shared_ptr<QThreadPool> const& create_pool,
                 shared_ptr<RateLimiter> const& limiter,
                 CredentialsCache& creds,
                 InactivityHandler& inactivity_handler,
                 unique_ptr<ThumbnailRequest>&& request,
                 QString const& details)
    : p(new HandlerPrivate(bus, message,
                           check_pool, create_pool,
                           limiter, creds, inactivity_handler,
                           move(request), details))
{
    connect(&p->checkWatcher, &QFutureWatcher<ByteArrayOrError>::finished, this, &Handler::checkFinished);
    connect(p->request.get(), &ThumbnailRequest::downloadFinished, this, &Handler::downloadFinished);
    connect(&p->createWatcher, &QFutureWatcher<ByteArrayOrError>::finished, this, &Handler::createFinished);
    p->inactivity_handler.request_started();
}

Handler::~Handler()
{
    p->cancelled = true;
    if (p->cancel_func)
    {
        p->cancel_func();
    }
    // ensure that jobs occurring in the thread pool complete.
    p->checkWatcher.waitForFinished();
    p->createWatcher.waitForFinished();
    p->inactivity_handler.request_completed();
    p->request.reset();
}

string const& Handler::key() const
{
    return p->request->key();
}

void Handler::begin()
{
    p->creds.get(p->message.service(),
                 [this](CredentialsCache::Credentials const& credentials)
                 {
                     gotCredentials(credentials);
                 });
}

void Handler::gotCredentials(CredentialsCache::Credentials const& credentials)
{
    if (p->cancelled)
    {
        Q_EMIT finished();
        return;
    }

    if (!credentials.valid)
    {
        // LCOV_EXCL_START
        sendError("Handler::gotCredentials(): " + details() + ": could not retrieve peer credentials");
        return;
        // LCOV_EXCL_STOP
    }

    try
    {
        p->request->check_client_credentials(credentials.user, credentials.label);
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError("Handler::gotCredentials(): " + details() + ": " + e.what());
        return;
    }
    // LCOV_EXCL_STOP

    auto do_check = [this]() -> ByteArrayOrError
    {
        try
        {
            return ByteArrayOrError{check(), nullptr};
        }
        // LCOV_EXCL_START
        catch (std::exception const& e)
        {
            return ByteArrayOrError{QByteArray(), e.what()};
        }
        // LCOV_EXCL_STOP
    };
    p->checkWatcher.setFuture(QtConcurrent::run(p->check_pool.get(), do_check));
}

// check() determines whether the requested thumbnail exists in
// the cache.  It is called synchronously in the thread pool.
//
// If the thumbnail is available, it is returned as a file descriptor,
// which will be returned to the user.
//
// If not, we continue to the asynchronous download stage.

QByteArray Handler::check()
{
    if (p->cancelled)
    {
        return QByteArray();
    }
    return p->request->thumbnail();
}

void Handler::checkFinished()
{
    if (p->cancelled)
    {
        return;
    }

    ByteArrayOrError ba_error;
    try
    {
        ba_error = p->checkWatcher.result();
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError("Handler::checkFinished(): exception: " + details() + ": " + e.what());
        return;
    }
    if (!ba_error.error.isNull())
    {
        sendError("Handler::checkFinished(): result error: " + details() + ": " + ba_error.error);
        return;
    }
    // LCOV_EXCL_STOP

    // Did we find a valid thumbnail in the cache or generated it locally from an image file?
    if (ba_error.ba.size() != 0)
    {
        sendThumbnail(ba_error.ba);
        return;
    }

    p->schedule_start_time = chrono::system_clock::now();
    if (p->request->status() == ThumbnailRequest::FetchStatus::needs_download)
    {
        try
        {
            // otherwise move on to the download phase.
            p->cancel_func = p->limiter->schedule([&]
            {
                if (!p->cancelled)
                {
                    p->download_start_time = chrono::system_clock::now();
                    p->request->download();
                }
            });
        }
        // LCOV_EXCL_START
        catch (std::exception const& e)
        {
            sendError("Handler::checkFinished(): download error: " + details() + e.what());
        }
        // LCOV_EXCL_STOP
        return;
    }

    sendError("Handler::checkFinished(): no artwork for " + details() + ": " + status_as_string());
}

void Handler::downloadFinished()
{
    p->download_finish_time = chrono::system_clock::now();
    p->limiter->done();

    if (p->cancelled)
    {
        return;
    }

    auto do_create = [this]() -> ByteArrayOrError
    {
        try
        {
            return ByteArrayOrError{create(), nullptr};
        }
        catch (std::exception const& e)
        {
            return ByteArrayOrError{QByteArray(), e.what()};
        }
    };
    p->createWatcher.setFuture(QtConcurrent::run(p->create_pool.get(), do_create));
}

// create() picks up after the asynchronous download stage completes.
// It effectively repeats the check() stage, except that thumbnailing
// failures are now errors.  It is called synchronously in the thread
// pool.

QByteArray Handler::create()
{
    if (p->cancelled)
    {
        return QByteArray();
    }

    QByteArray art_image = p->request->thumbnail();
    if (art_image.size() == 0)
    {
        throw runtime_error("could not get thumbnail for " + details().toStdString() + ": " +
                            status_as_string().toStdString());
    }
    return art_image;
}

void Handler::createFinished()
{
    if (p->cancelled)
    {
        return;
    }

    ByteArrayOrError ba_error;
    try
    {
        ba_error = p->createWatcher.result();
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError(QStringLiteral("Handler::createFinished(): ") + details() + ": " + e.what());
        return;
    }
    if (!ba_error.error.isEmpty())
    {
        sendError("Handler::createFinished(): " + ba_error.error);
        return;
    }
    // LCOV_EXCL_STOP
    sendThumbnail(ba_error.ba);
}

void Handler::sendThumbnail(QByteArray const& ba)
{
    p->bus.send(p->message.createReply(QVariant(ba)));
    p->finish_time = chrono::system_clock::now();
    Q_EMIT finished();
}

void Handler::sendError(QString const& error)
{
    if (p->request->status() == ThumbnailRequest::FetchStatus::hard_error ||
        p->request->status() == ThumbnailRequest::FetchStatus::temporary_error)
    {
        qWarning() << error;
    }
    p->bus.send(p->message.createErrorReply(ART_ERROR, error));
    p->finish_time = chrono::system_clock::now();
    Q_EMIT finished();
}

chrono::microseconds Handler::completion_time() const
{
    assert(p->finish_time != chrono::system_clock::time_point());
    return chrono::duration_cast<chrono::microseconds>(p->finish_time - p->start_time);
}

chrono::microseconds Handler::queued_time() const
{
    // Returned duration may be zero if the request wasn't kept waiting in the queue.
    return chrono::duration_cast<chrono::microseconds>(p->download_start_time - p->schedule_start_time);
}

chrono::microseconds Handler::download_time() const
{
    // Not a typo: we really mean to check finish_time, not download_finish_time in the assert.
    assert(p->finish_time != chrono::system_clock::time_point());
    if (p->download_start_time == chrono::system_clock::time_point())
    {
        return chrono::microseconds(0);  // We had a cache hit and didn't download
    }
    return chrono::duration_cast<chrono::microseconds>(p->download_finish_time - p->download_start_time);
}

QString Handler::details() const
{
    return p->details;
}

ThumbnailRequest::FetchStatus Handler::status() const
{
    return p->request->status();
}

QString Handler::status_as_string() const
{
    switch (p->request->status())
    {
    case ThumbnailRequest::FetchStatus::cache_hit:
        return QStringLiteral("HIT");
    case ThumbnailRequest::FetchStatus::scaled_from_fullsize:
        return QStringLiteral("FULL-SIZE HIT");
    case ThumbnailRequest::FetchStatus::cached_failure:
        return QStringLiteral("FAILED PREVIOUSLY");
    case ThumbnailRequest::FetchStatus::needs_download:
        return QStringLiteral("NEEDS DOWNLOAD");          // LCOV_EXCL_LINE
    case ThumbnailRequest::FetchStatus::downloaded:
        return QStringLiteral("MISS");
    case ThumbnailRequest::FetchStatus::not_found:
        return QStringLiteral("NO ARTWORK");
    case ThumbnailRequest::FetchStatus::network_down:
        return QStringLiteral("NETWORK DOWN");            // LCOV_EXCL_LINE
    case ThumbnailRequest::FetchStatus::hard_error:
        return QStringLiteral("ERROR");                   // LCOV_EXCL_LINE
    case ThumbnailRequest::FetchStatus::temporary_error:
        return QStringLiteral("TEMPORARY ERROR");
    case ThumbnailRequest::FetchStatus::timeout:
        return QStringLiteral("TIMEOUT");
    default:
        abort();  // LCOV_EXCL_LINE  // Impossible.
    }
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
