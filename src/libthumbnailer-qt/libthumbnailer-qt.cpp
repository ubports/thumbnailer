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

#include <QSharedPointer>

#include <memory>

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
    RequestImpl(QString const& trace, QSize const& requested_size,
                RateLimiter* limiter,
                std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> const& job);

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
        if (finished_ || cancelled_)
        {
            return;
        }

        // If we are called before the request made it out of the limiter queue,
        // we have not sent the request yet and, therefore, don't have a watcher.
        // In that case we send the request right here after removing it
        // from the limiter queue. This guarantees that we always have
        // a watcher to wait on.
        if (!sent_)
        {
            Q_ASSERT(!watcher_);
            cancel_func_();
            limiter_->schedule_now(send_request_);
        }
        watcher_->waitForFinished();
    }

    void setRequest(unity::thumbnailer::qt::Request* request)
    {
        public_request_ = request;
    }

    void cancel();

    bool isCancelled() const
    {
        return cancelled_;
    }

private Q_SLOTS:
    void dbusCallFinished();

private:
    void finishWithError(QString const& errorMessage);

    QSize requested_size_;
    RateLimiter* limiter_;
    std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> job_;
    std::function<void()> send_request_;

    std::unique_ptr<QDBusPendingCallWatcher> watcher_;
    RateLimiter::CancelFunc cancel_func_;
    QString error_message_;
    bool finished_;
    bool is_valid_;
    bool cancelled_;
    bool sent_;     // Becomes true once rate limiter has given the request to DBus.
    QImage image_;
    unity::thumbnailer::qt::Request* public_request_;
    QString trace_;
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
    QSharedPointer<Request> createRequest(QString const& trace, QSize const& requested_size,
                                          std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> const& job);
    std::unique_ptr<ThumbnailerInterface> iface_;
    RateLimiter limiter_;
};

RequestImpl::RequestImpl(QString const& trace, QSize const& requested_size,
                         RateLimiter* limiter,
                         std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> const& job)
    : requested_size_(requested_size)
    , limiter_(limiter)
    , job_(job)
    , finished_(false)
    , is_valid_(false)
    , cancelled_(false)
    , sent_(false)
    , public_request_(nullptr)
    , trace_(trace)
{
    // The limiter does not call send_request_ until the request can be sent
    // without exceeding max_backlog().
    send_request_ = [this]
    {
        watcher_.reset(new QDBusPendingCallWatcher(job_()));
        connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &RequestImpl::dbusCallFinished);
        sent_ = true;
    };
    cancel_func_ = limiter_->schedule(send_request_);
}

void RequestImpl::dbusCallFinished()
{
    Q_ASSERT(watcher_);
    Q_ASSERT(sent_);
    Q_ASSERT(!finished_);

    limiter_->done();

    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher_.get();
    if (!reply.isValid())
    {
        qDebug() << "reply invalid, cancelled: " << cancelled_;
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): D-Bus error: " + reply.error().message());
        return;
    }

    try
    {
        QSize realSize;
        image_ = unity::thumbnailer::internal::imageFromFd(reply.value().fileDescriptor(), &realSize, requested_size_);
        finished_ = true;
        is_valid_ = true;
        error_message_ = QLatin1String("");
        watcher_.reset();
        Q_ASSERT(public_request_);
        Q_EMIT public_request_->finished();
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): thumbnailer failed: " +
                        QString::fromStdString(e.what()));
    }
    catch (...)
    {
        finishWithError(QStringLiteral("ThumbnailerRequestImpl::dbusCallFinished(): unknown exception"));
    }
    // LCOV_EXCL_STOP
}

void RequestImpl::finishWithError(QString const& errorMessage)
{
    error_message_ = errorMessage;
    finished_ = true;
    is_valid_ = false;
    image_ = QImage();
    qWarning() << error_message_;
    watcher_.reset();
    Q_ASSERT(public_request_);
    Q_EMIT public_request_->finished();
}

void RequestImpl::cancel()
{
    if (finished_ || sent_ || cancelled_)
    {
        return;  // Too late, do nothing.
    }

    qDebug() << "cancelled:" << trace_;
    cancel_func_();
    image_ = QImage();
    cancelled_ = true;
    finished_ = true;
    is_valid_ = false;
    error_message_ = QLatin1String("Request cancelled");
    // Deleting the pending call watcher (which holds the only
    // reference to the pending call at this point) tells Qt that we
    // are no longer interested in the reply.  The destruction will
    // also clear up the signal connections.
    watcher_.reset();
    Q_ASSERT(public_request_);
    Q_EMIT public_request_->finished();
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
    QString trace;
    QTextStream stream(&trace, QIODevice::WriteOnly);
    stream << "getAlbumArt: artist: " << artist << " album: " << album
           << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
    qDebug() << trace;
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetAlbumArt(artist, album, requestedSize);
    };
    return createRequest(trace, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getArtistArt(QString const& artist,
                                                      QString const& album,
                                                      QSize const& requestedSize)
{
    QString trace;
    QTextStream stream(&trace, QIODevice::WriteOnly);
    stream << "getArtistArt: artist: " << artist << " album: " << album
           << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
    qDebug() << trace;
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetArtistArt(artist, album, requestedSize);
    };
    return createRequest(trace, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getThumbnail(QString const& filename, QSize const& requestedSize)
{
    QString trace;
    QTextStream stream(&trace, QIODevice::WriteOnly);
    stream << "getThumbnail: filename: " << filename
           << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
    qDebug() << trace;
    auto job = [this, filename, requestedSize]
    {
        return iface_->GetThumbnail(filename, requestedSize);
    };
    return createRequest(trace, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::createRequest(QString const& trace, QSize const& requested_size,
                                                       std::function<QDBusPendingReply<QDBusUnixFileDescriptor>()> const& job)
{
    auto request_impl = new RequestImpl(trace, requested_size, &limiter_, job);
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

bool Request::isCancelled() const
{
    return p_->isCancelled();
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
