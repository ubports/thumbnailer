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
#include <internal/trace.h>

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QThreadPool>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{
char const ART_ERROR[] = "com.canonical.Thumbnailer.Error.Failed";

struct FdOrError
{
    QDBusUnixFileDescriptor fd;
    QString error;
};

QDBusUnixFileDescriptor write_to_tmpfile(string const& image)
{
    static auto find_tmpdir = []
    {
        char const* dirp = getenv("TMPDIR");
        string dir = dirp ? dirp : "/tmp";
        return dir;
    };
    static string dir = find_tmpdir();

    FdPtr fd(open(dir.c_str(), O_TMPFILE | O_RDWR | O_CLOEXEC), do_close);
    // Different kernel verions return different errno if they don't
    // recognize O_TMPFILE, so we check for all failures here. If it
    // is a real failure (rather than the flag not being recognized),
    // mkstemp will fail too.
    if (fd.get() < 0)
    {
        // We are running on an old kernel without O_TMPFILE support:
        // the flag has been ignored, and treated as an attempt to
        // open /tmp.
        //
        // As a fallback, use mkstemp() and unlink the resulting file.
        // LCOV_EXCL_START
        string tmpfile = dir + "/thumbnail.XXXXXX";
        fd.reset(mkostemp(const_cast<char*>(tmpfile.data()), O_CLOEXEC));
        if (fd.get() >= 0)
        {
            unlink(tmpfile.data());
        }
        // LCOV_EXCL_STOP
    }
    if (fd.get() < 0)
    {
        // LCOV_EXCL_START
        string err = "Handler: cannot create tmpfile in " + dir + ": " + safe_strerror(errno);
        throw runtime_error(err);
        // LCOV_EXCL_STOP
    }
    auto rc = write(fd.get(), &image[0], image.size());
    if (rc == -1)
    {
        // LCOV_EXCL_START
        string err = "Handler: cannot write image data in " + dir + ": " + safe_strerror(errno);
        throw runtime_error(err);
        // LCOV_EXCL_STOP
    }
    else if (string::size_type(rc) != image.size())
    {
        // LCOV_EXCL_START
        string err = "Handler: short write for image data in " + dir + "(requested = " +
                          to_string(image.size()) + ", actual = " + to_string(rc) + ")";
        throw runtime_error(err);
        // LCOV_EXCL_STOP
    }
    lseek(fd.get(), SEEK_SET, 0);  // No error check needed, can't fail.

    return QDBusUnixFileDescriptor(fd.get());
}
}

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct HandlerPrivate
{
    QDBusConnection bus;
    QDBusMessage const message;
    shared_ptr<QThreadPool> check_pool;
    shared_ptr<QThreadPool> create_pool;
    RateLimiter& limiter;
    CredentialsCache& creds;
    unique_ptr<ThumbnailRequest> request;
    chrono::system_clock::time_point start_time;            // Overall start time
    chrono::system_clock::time_point finish_time;           // Overall finish time
    chrono::system_clock::time_point schedule_start_time;   // Time at which download/extract is scheduled.
    chrono::system_clock::time_point download_start_time;   // Time at which download/extract is started.
    chrono::system_clock::time_point download_finish_time;  // Time at which download/extract has completed.
    QString details;
    QString status;

    bool cancelled = false;
    QFutureWatcher<FdOrError> checkWatcher;
    QFutureWatcher<FdOrError> createWatcher;

    HandlerPrivate(QDBusConnection const& bus,
                   QDBusMessage const& message,
                   shared_ptr<QThreadPool> check_pool,
                   shared_ptr<QThreadPool> create_pool,
                   RateLimiter& limiter,
                   CredentialsCache& creds,
                   unique_ptr<ThumbnailRequest>&& request,
                   QString const& details)
        : bus(bus)
        , message(message)
        , check_pool(check_pool)
        , create_pool(create_pool)
        , limiter(limiter)
        , creds(creds)
        , request(move(request))
        , details(details)
    {
        start_time = chrono::system_clock::now();
    }
};

Handler::Handler(QDBusConnection const& bus,
                 QDBusMessage const& message,
                 shared_ptr<QThreadPool> check_pool,
                 shared_ptr<QThreadPool> create_pool,
                 RateLimiter& limiter,
                 CredentialsCache& creds,
                 unique_ptr<ThumbnailRequest>&& request,
                 QString const& details)
    : p(new HandlerPrivate(bus, message, check_pool, create_pool, limiter, creds, move(request), details))
{
    connect(&p->checkWatcher, &QFutureWatcher<FdOrError>::finished, this, &Handler::checkFinished);
    connect(p->request.get(), &ThumbnailRequest::downloadFinished, this, &Handler::downloadFinished);
    connect(&p->createWatcher, &QFutureWatcher<FdOrError>::finished, this, &Handler::createFinished);
}

Handler::~Handler()
{
    p->cancelled = true;
    // ensure that jobs occurring in the thread pool complete.
    p->checkWatcher.waitForFinished();
    p->createWatcher.waitForFinished();
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
    if (!credentials.valid)
    {
        // LCOV_EXCL_START
        sendError("Handler::gotCredentials(): " + details() + ": could not retrieve peer credentials");
        return;
        // LCOV_EXCL_STOP
    }
    try
    {
        p->request->check_client_credentials(
            credentials.user, credentials.label);
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError("Handler::gotCredentials(): " + details() + ": " + e.what());
        return;
    }
    // LCOV_EXCL_STOP

    auto do_check = [this]() -> FdOrError
    {
        try
        {
            return FdOrError{check(), nullptr};
        }
        // LCOV_EXCL_START
        catch (std::exception const& e)
        {
            return FdOrError{QDBusUnixFileDescriptor(), e.what()};
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

QDBusUnixFileDescriptor Handler::check()
{
    string art_image = p->request->thumbnail();

    if (art_image.empty())
    {
        return QDBusUnixFileDescriptor();
    }
    return write_to_tmpfile(art_image);
}

void Handler::checkFinished()
{
    if (p->cancelled)
    {
        return;
    }

    FdOrError fd_error;
    try
    {
        fd_error = p->checkWatcher.result();
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError("Handler::checkFinished(): result error: " + details() + ": " + e.what());
        return;
    }
    if (!fd_error.error.isNull())
    {
        sendError("Handler::checkFinished(): result error: " + details() + ": " + fd_error.error);
        return;
    }
    // LCOV_EXCL_STOP

    // Did we find a valid thumbnail in the cache or generated it locally from an image file?
    if (fd_error.fd.isValid())
    {
        sendThumbnail(fd_error.fd);
        return;
    }

    if (p->request->status() == ThumbnailRequest::FetchStatus::needs_download)
    {
        try
        {
            // otherwise move on to the download phase.
            p->schedule_start_time = chrono::system_clock::now();
            p->limiter.schedule([&]{ p->download_start_time = chrono::system_clock::now(); p->request->download(); });
        }
        // LCOV_EXCL_START
        catch (std::exception const& e)
        {
            sendError("Handler::checkFinished(): download error: " + details() + e.what());
        }
        // LCOV_EXCL_STOP
        return;
    }

    sendError("Handler::check_finished(): no artwork for " + details() + ": " + status());
}

void Handler::downloadFinished()
{
    p->download_finish_time = chrono::system_clock::now();
    p->limiter.done();

    if (p->cancelled)
    {
        return;
    }

    auto do_create = [this]() -> FdOrError
    {
        try
        {
            return FdOrError{create(), nullptr};
        }
        catch (std::exception const& e)
        {
            return FdOrError{QDBusUnixFileDescriptor(), e.what()};
        }
    };
    p->createWatcher.setFuture(QtConcurrent::run(p->create_pool.get(), do_create));
}

// create() picks up after the asynchronous download stage completes.
// It effectively repeats the check() stage, except that thumbnailing
// failures are now errors.  It is called synchronously in the thread
// pool.

QDBusUnixFileDescriptor Handler::create()
{
    string art_image = p->request->thumbnail();

    if (art_image.empty())
    {
        throw runtime_error("could not get thumbnail for " + details().toStdString() + ": " + status().toStdString());
    }
    return write_to_tmpfile(art_image);
}

void Handler::createFinished()
{
    if (p->cancelled)
    {
        return;
    }

    FdOrError fd_error;
    try
    {
        fd_error = p->createWatcher.result();
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        sendError(QString("Handler::createFinished(): ") + details() + ": " + e.what());
        return;
    }
    if (!fd_error.error.isEmpty())
    {
        sendError("Handler::createFinished(): " + fd_error.error);
        return;
    }
    // LCOV_EXCL_STOP
    if (fd_error.fd.isValid())
    {
        sendThumbnail(fd_error.fd);
    }
    else
    {
        sendError("Handler::createFinished(): invalid file descriptor for " + details());  // LCOV_EXCL_LINE
    }
}

void Handler::sendThumbnail(QDBusUnixFileDescriptor const& unix_fd)
{
    p->bus.send(p->message.createReply(QVariant::fromValue(unix_fd)));
    p->finish_time = chrono::system_clock::now();
    Q_EMIT finished();
}

void Handler::sendError(QString const& error)
{
    if (p->request->status() == ThumbnailRequest::FetchStatus::error)
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

QString Handler::status() const
{
    switch (p->request->status())
    {
    case ThumbnailRequest::FetchStatus::cache_hit:
        return "HIT";
    case ThumbnailRequest::FetchStatus::scaled_from_fullsize:
        return "FULL-SIZE HIT";
    case ThumbnailRequest::FetchStatus::cached_failure:
        return "FAILED PREVIOUSLY";
    case ThumbnailRequest::FetchStatus::needs_download:
        return "NEEDS DOWNLOAD";  // LCOV_EXCL_LINE
    case ThumbnailRequest::FetchStatus::downloaded:
        return "MISS";
    case ThumbnailRequest::FetchStatus::not_found:
        return "NO ARTWORK";
    case ThumbnailRequest::FetchStatus::no_network:
        return "NETWORK DOWN";  // LCOV_EXCL_LINE
    case ThumbnailRequest::FetchStatus::error:
        return "ERROR";  // LCOV_EXCL_LINE
    default:
        abort();  // LCOV_EXCL_LINE  // Impossible.
    }
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
