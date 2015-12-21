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
    RequestImpl(QString const& details,
                QSize const& requested_size,
                RateLimiter* limiter,
                std::function<QDBusPendingReply<QByteArray>()> const& job,
                bool trace_client);

    ~RequestImpl();

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
        if (cancel_func_())
        {
            Q_ASSERT(!watcher_);
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
    void pump_limiter();
    void finishWithError(QString const& errorMessage);

    QString details_;
    QSize requested_size_;
    RateLimiter* limiter_;
    std::function<QDBusPendingReply<QByteArray>()> job_;
    std::function<void()> send_request_;

    std::unique_ptr<QDBusPendingCallWatcher> watcher_;
    RateLimiter::CancelFunc cancel_func_;
    QString error_message_;
    bool finished_;
    bool is_valid_;
    bool cancelled_;                 // true if cancel() was called by client
    bool cancelled_while_waiting_;   // true if cancel() succeeded because request was not sent yet
    bool trace_client_;
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
    QSharedPointer<Request> createRequest(QString const& details,
                                          QSize const& requested_size,
                                          std::function<QDBusPendingReply<QByteArray>()> const& job);
    std::unique_ptr<ThumbnailerInterface> iface_;
    RateLimiter limiter_;
    bool trace_client_;
};

RequestImpl::RequestImpl(QString const& details,
                         QSize const& requested_size,
                         RateLimiter* limiter,
                         std::function<QDBusPendingReply<QByteArray>()> const& job,
                         bool trace_client)
    : details_(details)
    , requested_size_(requested_size)
    , limiter_(limiter)
    , job_(job)
    , finished_(false)
    , is_valid_(false)
    , cancelled_(false)
    , cancelled_while_waiting_(false)
    , trace_client_(trace_client)
    , public_request_(nullptr)
{
    if (!requested_size.isValid())
    {
        error_message_ = details_ + ": " + "invalid QSize";
        qCritical().noquote() << error_message_;
        finished_ = true;
        return;
    }

    // The limiter does not call send_request_ until the request can be sent
    // without exceeding max_backlog().
    send_request_ = [this]
    {
        watcher_.reset(new QDBusPendingCallWatcher(job_()));
        connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &RequestImpl::dbusCallFinished);
    };
    cancel_func_ = limiter_->schedule(send_request_);
}

RequestImpl::~RequestImpl()
{
    // If watcher_ is still set, this request is destroyed while
    // the request is either still in the queue, or it has been sent
    // but the reply has not yet trickled in. We have to cancel
    // and pump the limiter in that case because we'll never receive
    // the dbusCallFinished callback.
    if (watcher_)
    {
        Q_ASSERT(cancel_func_);
        bool already_sent = !cancel_func_();
        if (already_sent)
        {
            // Delay pumping until we drop back to the event loop. Otherwse,
            // if the caller destroys a whole bunch of requests at once, we'd
            // schedule the next request in the queue before the caller gets
            // a chance to destroy the next request.
            QMetaObject::invokeMethod(this, "pump_limiter", Qt::QueuedConnection);
        }
    }
}

void RequestImpl::pump_limiter()
{
    limiter_->done();
}

void RequestImpl::dbusCallFinished()
{
    Q_ASSERT(!finished_);

    // If this isn't a fake call from cancel(), pump the limiter.
    if (!cancelled_ || !cancelled_while_waiting_)
    {
        // We depend on calls to pump the limiter exactly once for each request that was sent.
        // Whenever a (real) DBus call finishes, we inform the limiter, so it can kick off
        // the next pending job.
        limiter_->done();
    }

    if (cancelled_)
    {
        finishWithError("Request cancelled");
        Q_ASSERT(!watcher_);
        return;
    }

    Q_ASSERT(watcher_);
    Q_ASSERT(!finished_);

    QDBusPendingReply<QByteArray> reply = *watcher_.get();
    if (!reply.isValid())
    {
        finishWithError("Thumbnailer: RequestImpl::dbusCallFinished(): D-Bus error: " + reply.error().message());
        return;
    }

    try
    {
        image_ = QImage::fromData(reply.value(), "JPG");
        finished_ = true;
        is_valid_ = true;
        error_message_ = QLatin1String("");
        Q_ASSERT(public_request_);
        Q_EMIT public_request_->finished();
        if (trace_client_)
        {
            qDebug().noquote() << "Thumbnailer: completed:" << details_;
        }
    }
    // LCOV_EXCL_START
    catch (const std::exception& e)
    {
        finishWithError("Thumbnailer: RequestImpl::dbusCallFinished(): thumbnailer failed: " +
                        QString::fromStdString(e.what()));
    }
    catch (...)
    {
        finishWithError(QStringLiteral("Thumbnailer: RequestImpl::dbusCallFinished(): unknown exception"));
    }
    watcher_.reset();
    // LCOV_EXCL_STOP
}

void RequestImpl::finishWithError(QString const& errorMessage)
{
    error_message_ = errorMessage;
    finished_ = true;
    is_valid_ = false;
    image_ = QImage();
    if (!cancelled_)
    {
        qWarning().noquote() << error_message_;  // Cancellation is an expected outcome, no warning for that.
    }
    else if (trace_client_)
    {
        qDebug().noquote() << "Thumbnailer: cancelled:" << details_;
    }
    watcher_.reset();
    Q_ASSERT(public_request_);
    Q_EMIT public_request_->finished();
}

void RequestImpl::cancel()
{
    if (trace_client_)
    {
        qDebug().noquote() << "Thumbnailer: cancelling:" << details_;
    }

    if (finished_ || cancelled_)
    {
        qDebug().noquote() << "Thumbnailer: already finished or cancelled:" << details_;
        return;  // Too late, do nothing.
    }

    if (finished_ || cancelled_)
    {
        return;  // Too late, do nothing.
    }

    cancelled_ = true;
    cancelled_while_waiting_ = cancel_func_();
    if (cancelled_while_waiting_)
    {
        // We fake the call completion, in order to pump the limiter only from within
        // the dbus completion callback. We cannot call limiter_->done() here
        // because that would schedule the next request in the queue.
        QMetaObject::invokeMethod(this, "dbusCallFinished", Qt::QueuedConnection);
    }
}

ThumbnailerImpl::ThumbnailerImpl(QDBusConnection const& connection)
    : limiter_(Settings().max_backlog())
    , trace_client_(Settings().trace_client())
{
    trace_client_ = true;
    iface_.reset(new ThumbnailerInterface(service::BUS_NAME, service::THUMBNAILER_BUS_PATH, connection));
}

QSharedPointer<Request> ThumbnailerImpl::getAlbumArt(QString const& artist,
                                                     QString const& album,
                                                     QSize const& requestedSize)
{
    QString details;
    QTextStream s(&details, QIODevice::WriteOnly);
    s << "getAlbumArt: (" << requestedSize.width() << "," << requestedSize.height()
      << ") \"" << artist << "\", \"" << album << "\"";
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetAlbumArt(artist, album, requestedSize);
    };
    return createRequest(details, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getArtistArt(QString const& artist,
                                                      QString const& album,
                                                      QSize const& requestedSize)
{
    QString details;
    QTextStream s(&details, QIODevice::WriteOnly);
    s << "getArtistArt: (" << requestedSize.width() << "," << requestedSize.height()
      << ") \"" << artist << "\", \"" << album << "\"";
    auto job = [this, artist, album, requestedSize]
    {
        return iface_->GetArtistArt(artist, album, requestedSize);
    };
    return createRequest(details, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::getThumbnail(QString const& filename, QSize const& requestedSize)
{
    QString details;
    QTextStream s(&details, QIODevice::WriteOnly);
    s << "getThumbnail: (" << requestedSize.width() << "," << requestedSize.height() << ") " << filename;
    auto job = [this, filename, requestedSize]
    {
        return iface_->GetThumbnail(filename, requestedSize);
    };
    return createRequest(details, requestedSize, job);
}

QSharedPointer<Request> ThumbnailerImpl::createRequest(QString const& details,
                                                       QSize const& requested_size,
                                                       std::function<QDBusPendingReply<QByteArray>()> const& job)
{
    if (trace_client_)
    {
        qDebug().noquote() << "Thumbnailer:" << details;
    }
    auto request_impl = new RequestImpl(details, requested_size, &limiter_, job, trace_client_);
    auto request = QSharedPointer<Request>(new Request(request_impl));
    request_impl->setRequest(request.data());
    if (request->isFinished() && !request->isValid())
    {
        QMetaObject::invokeMethod(request.data(), "finished", Qt::QueuedConnection);
    }
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
