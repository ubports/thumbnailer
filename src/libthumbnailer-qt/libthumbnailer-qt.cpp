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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 *              Michi Henning <michi.henning@canonical.com>
 */

#include <unity/thumbnailer/qt/thumbnailer-qt.h>

#include <ratelimiter.h>
#include <settings.h>
#include <thumbnailerinterface.h>
#include <utils/artgeneratorcommon.h>
#include <service/dbus_names.h>

#include <memory>

#include <QSharedPointer>

namespace unity
{

namespace thumbnailer
{

namespace qt
{

namespace internal
{

class RequestImpl : public QObject
{
    Q_OBJECT
public:
    RequestImpl(QSize const& requested_size,
                RateLimiter* limiter,
                std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job);

    ~RequestImpl() = default;

    bool isFinished() const
    {
        return finished_;
    }

    QImage image() const
    {
        return image_;
    }

    QString errorMessage() const
    {
        return error_message_;
    }

    bool isValid() const
    {
        return is_valid_;
    }

    void waitForFinished()
    {
        if (finished_)
        {
            return;
        }

        // If we are called before the request made it out of the limiter queue,
        // we have not sent the request yet and, therefore, don't have a watcher.
        // In that case we send the request right here after removing it
        // from the limiter queue. This guarantees that we always have
        // a watcher to wait on.
        if (!watcher_)
        {
            cancel_func_();
            watcher_.reset(new QDBusPendingCallWatcher(job_()));
            connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &RequestImpl::dbusCallFinished);
        }
        watcher_->waitForFinished();
    }

    void setRequest(unity::thumbnailer::qt::Request* request)
    {
        public_request_ = request;
    }

    void cancel();

private Q_SLOTS:
    void dbusCallFinished();

private:
    void finishWithError(QString const& errorMessage);

    QSize requested_size_;
    RateLimiter* limiter_;
    std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job_;
    std::unique_ptr<QDBusPendingCallWatcher> watcher_;
    std::function<void()> cancel_func_;
    QString error_message_;
    bool finished_;
    bool is_valid_;
    QImage image_;
    unity::thumbnailer::qt::Request* public_request_;
};

class ThumbnailerImpl
{
public:
    Q_DISABLE_COPY(ThumbnailerImpl)
    explicit ThumbnailerImpl(QDBusConnection const& connection);
    ~ThumbnailerImpl() = default;

    QSharedPointer<Request> getAlbumArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QSharedPointer<Request> getArtistArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QSharedPointer<Request> getThumbnail(QString const& filename, QSize const& requestedSize);

private:
    QSharedPointer<Request> createRequest(QSize const& requested_size,
                                          std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job);
    std::unique_ptr<ThumbnailerInterface> iface_;
    RateLimiter limiter_;
};

RequestImpl::RequestImpl(QSize const& requested_size,
                         RateLimiter* limiter,
                         std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job)
    : requested_size_(requested_size)
    , limiter_(limiter)
    , job_(job)
    , finished_(false)
    , is_valid_(false)
    , public_request_(nullptr)
{
    auto send_request = [this, job]
    {
        watcher_.reset(new QDBusPendingCallWatcher(job()));
        connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &RequestImpl::dbusCallFinished);
    };
    cancel_func_ = limiter_->schedule(send_request);
}

void RequestImpl::dbusCallFinished()
{
    Q_ASSERT(watcher_);

    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher_.get();
    if (!reply.isValid())
    {
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): D-Bus error: " + reply.error().message());
        return;
    }

    try
    {
        QSize realSize;
        image_ = unity::thumbnailer::internal::imageFromFd(reply.value().fileDescriptor(), &realSize, requested_size_);
        finished_ = true;
        is_valid_ = true;
        error_message_ = "";
        Q_ASSERT(public_request_);
        qDebug() << "emitting finished";
        Q_EMIT public_request_->finished();
        limiter_->done();
        return;
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): thumbnailer failed: " +
                        QString::fromStdString(e.what()));
    }
    catch (...)
    {
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): unknown exception");
    }

    finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): unknown error");
    // LCOV_EXCL_STOP
}

void RequestImpl::finishWithError(QString const& errorMessage)
{
    error_message_ = errorMessage;
    finished_ = true;
    is_valid_ = false;
    image_ = QImage();
    qWarning() << error_message_;
    Q_ASSERT(public_request_);
    qDebug() << "emitting error finished";
    Q_EMIT public_request_->finished();
    limiter_->done();
}

void RequestImpl::cancel()
{
    cancel_func_();

    if (!finished_)
    {
        // Deleting the pending call watcher (which should hold the only
        // reference to the pending call at this point) tells Qt that we
        // are no longer interested in the reply.  The destruction will
        // also clear up the signal connections.
        watcher_.reset();

        finishWithError("Request cancelled");
    }
}

ThumbnailerImpl::ThumbnailerImpl(QDBusConnection const& connection)
    : limiter_(Settings().max_backlog())
{
    iface_.reset(new ThumbnailerInterface(service::BUS_NAME, service::THUMBNAILER_BUS_PATH, connection));
}

QSharedPointer<Request> ThumbnailerImpl::getAlbumArt(QString const& artist,
                                                     QString const& album,
                                                     QSize const& requestedSize)
{
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetAlbumArt(artist, album, requestedSize);
    };
    return createRequest(requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getArtistArt(QString const& artist,
                                                      QString const& album,
                                                      QSize const& requestedSize)
{
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetArtistArt(artist, album, requestedSize);
    };
    return createRequest(requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getThumbnail(QString const& filename, QSize const& requestedSize)
{
    auto job = [this, filename, requestedSize]
    {
        return iface_->GetThumbnail(filename, requestedSize);
    };
    return createRequest(requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::createRequest(QSize const& requested_size,
                                                       std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job)
{
    //std::unique_ptr<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(reply));
    auto request_impl = new RequestImpl(requested_size, &limiter_, job);
    auto request = QSharedPointer<Request>(new Request(request_impl));
    request_impl->setRequest(request.data());
    return request;
}

}  // namespace internal

Request::Request(internal::RequestImpl* impl)
    : p_(impl)
{
}

Request::~Request() = default;

bool Request::isFinished() const
{
    return p_->isFinished();
}

QImage Request::image() const
{
    return p_->image();
}

QString Request::errorMessage() const
{
    return p_->errorMessage();
}

bool Request::isValid() const
{
    return p_->isValid();
}

void Request::waitForFinished()
{
    p_->waitForFinished();
}

void Request::cancel()
{
    p_->cancel();
}

// LCOV_EXCL_START
Thumbnailer::Thumbnailer()
    : Thumbnailer(QDBusConnection::sessionBus())
{
}
// LCOV_EXCL_STOP

Thumbnailer::Thumbnailer(QDBusConnection const& connection)
    : p_(new internal::ThumbnailerImpl(connection))
{
}

Thumbnailer::~Thumbnailer() = default;

QSharedPointer<Request> Thumbnailer::getAlbumArt(QString const& artist,
                                                 QString const& album,
                                                 QSize const& requestedSize)
{
    return p_->getAlbumArt(artist, album, requestedSize);
}

QSharedPointer<Request> Thumbnailer::getArtistArt(QString const& artist,
                                                  QString const& album,
                                                  QSize const& requestedSize)
{
    return p_->getArtistArt(artist, album, requestedSize);
}

QSharedPointer<Request> Thumbnailer::getThumbnail(QString const& filePath, QSize const& requestedSize)
{
    return p_->getThumbnail(filePath, requestedSize);
}

}  // namespace qt

}  // namespace thumbnailer

}  // namespace unity

#include "libthumbnailer-qt.moc"
