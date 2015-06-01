/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of version 3 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "handler.h"
#include <internal/safe_strerror.h>
#include <internal/raii.h>

#include <QDebug>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QThreadPool>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace unity::thumbnailer::internal;

namespace
{
char const ART_ERROR[] = "com.canonical.Thumbnailer.Error.Failed";

struct FdOrError
{
    QDBusUnixFileDescriptor fd;
    QString error;
};

QDBusUnixFileDescriptor write_to_tmpfile(std::string const& image)
{
    static auto find_tmpdir = []
    {
        char const* dirp = getenv("TMPDIR");
        std::string dir = dirp ? dirp : "/tmp";
        return dir;
    };
    static std::string dir = find_tmpdir();

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
        std::string tmpfile = dir + "/thumbnail.XXXXXX";
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
        std::string err = "Handler: cannot create tmpfile in " + dir + ": " + safe_strerror(errno);
        throw std::runtime_error(err);
        // LCOV_EXCL_STOP
    }
    auto rc = write(fd.get(), &image[0], image.size());
    if (rc == -1)
    {
        // LCOV_EXCL_START
        std::string err = "Handler: cannot write image data in " + dir + ": " + safe_strerror(errno);
        throw std::runtime_error(err);
        // LCOV_EXCL_STOP
    }
    else if (std::string::size_type(rc) != image.size())
    {
        // LCOV_EXCL_START
        std::string err = "Handler: short write for image data in " + dir + "(requested = " +
                          std::to_string(image.size()) + ", actual = " + std::to_string(rc) + ")";
        throw std::runtime_error(err);
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
    std::shared_ptr<QThreadPool> check_pool;
    std::shared_ptr<QThreadPool> create_pool;
    RateLimiter& limiter;
    std::unique_ptr<ThumbnailRequest> request;

    bool cancelled = false;
    QFutureWatcher<FdOrError> checkWatcher;
    QFutureWatcher<FdOrError> createWatcher;

    HandlerPrivate(QDBusConnection const& bus,
                   QDBusMessage const& message,
                   std::shared_ptr<QThreadPool> check_pool,
                   std::shared_ptr<QThreadPool> create_pool,
                   RateLimiter& limiter,
                   std::unique_ptr<ThumbnailRequest>&& request)
        : bus(bus)
        , message(message)
        , check_pool(check_pool)
        , create_pool(create_pool)
        , limiter(limiter)
        , request(std::move(request))
    {
    }
};

Handler::Handler(QDBusConnection const& bus,
                 QDBusMessage const& message,
                 std::shared_ptr<QThreadPool> check_pool,
                 std::shared_ptr<QThreadPool> create_pool,
                 RateLimiter& limiter,
                 std::unique_ptr<ThumbnailRequest>&& request)
    : p(new HandlerPrivate(bus, message, check_pool, create_pool, limiter, std::move(request)))
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
    qDebug() << "Handler" << this << "destroyed";
}

std::string const& Handler::key() const
{
    return p->request->key();
}

void Handler::begin()
{
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
    std::string art_image = p->request->thumbnail();

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
    catch (std::exception const& e)
    {
        sendError(e.what());
        return;
    }
    if (!fd_error.error.isNull())
    {
        sendError(fd_error.error);
        return;
    }
    // Did we find a valid thumbnail in the cache?
    if (fd_error.fd.isValid())
    {
        sendThumbnail(fd_error.fd);
    }
    else
    {
        // otherwise move on to the download phase.
        p->limiter.schedule([&]{ p->request->download(); });
    }
}

void Handler::downloadFinished()
{
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

// create() picks up after the asynchrnous download stage completes.
// It effectively repeats the check() stage, except that thumbnailing
// failures are now errors.  It is called synchronously in the thread
// pool.
QDBusUnixFileDescriptor Handler::create()
{
    std::string art_image = p->request->thumbnail();

    if (art_image.empty())
    {
        throw std::runtime_error("Handler::create(): Could not get thumbnail");
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
    catch (std::exception const& e)
    {
        sendError(e.what());
        return;
    }
    if (!fd_error.error.isEmpty())
    {
        sendError(fd_error.error);
        return;
    }
    if (fd_error.fd.isValid())
    {
        sendThumbnail(fd_error.fd);
    }
    else
    {
        sendError("Handler::create() did not produce a valid file descriptor");
    }
}

void Handler::sendThumbnail(QDBusUnixFileDescriptor const& unix_fd)
{
    p->bus.send(p->message.createReply(QVariant::fromValue(unix_fd)));
    Q_EMIT finished();
}

void Handler::sendError(QString const& error)
{
    p->bus.send(p->message.createErrorReply(ART_ERROR, error));
    Q_EMIT finished();
}
}
}
}
