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
 */

#include <libthumbnailer/thumbnailer.h>

#include <thumbnailerinterface.h>
#include <utils/artgeneratorcommon.h>
#include <service/dbus_names.h>

#include <memory>

#include <QSharedPointer>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class RequestImpl : public QObject
{
    Q_OBJECT
public:
    RequestImpl(QSize const& requested_size, std::unique_ptr<QDBusPendingCallWatcher>&& watcher);

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

    bool finishedSucessfully() const
    {
        return finished_successfully_;
    }

    void waitForFinished()
    {
        watcher_->waitForFinished();
    }

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void dbusCallFinished();

private:
    void finishWithError(QString const& errorMessage);

    QSize requested_size_;
    std::unique_ptr<QDBusPendingCallWatcher> watcher_;
    QString error_message_;
    bool finished_;
    bool finished_successfully_;
    QImage image_;
};

class ThumbnailerImpl : public QObject
{
    Q_OBJECT
public:
    Q_DISABLE_COPY(ThumbnailerImpl)
    ThumbnailerImpl();
    ThumbnailerImpl(QDBusConnection const& connection);

    ~ThumbnailerImpl() = default;

    QSharedPointer<Request> getAlbumArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QSharedPointer<Request> getArtistArt(QString const& artist, QString const& album, QSize const& requestedSize);
    QSharedPointer<Request> getThumbnail(QString const& filename, QSize const& requestedSize);

private:
    std::unique_ptr<QDBusConnection> connection_;
    std::unique_ptr<ThumbnailerInterface> iface_;

    friend class unity::thumbnailer::Thumbnailer;
};

RequestImpl::RequestImpl(QSize const& requested_size, std::unique_ptr<QDBusPendingCallWatcher>&& watcher)
    : requested_size_(requested_size)
    , watcher_(std::move(watcher))
    , finished_(false)
    , finished_successfully_(false)
{
    connect(watcher_.get(), &QDBusPendingCallWatcher::finished, this, &RequestImpl::dbusCallFinished);
}

void RequestImpl::dbusCallFinished()
{
    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher_.get();
    if (!reply.isValid())
    {
        finishWithError("ThumbnailerRequestImpl::dbusCallFinished(): D-Bus error: " + reply.error().message());
        return;
    }

    try
    {
        QSize realSize;
        image_ = imageFromFd(reply.value().fileDescriptor(), &realSize, requested_size_);
        finished_ = true;
        finished_successfully_ = true;
        error_message_ = "";
        Q_EMIT finished();
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
    finished_successfully_ = false;
    image_ = QImage();
    qWarning() << error_message_;
    Q_EMIT finished();
}

ThumbnailerImpl::ThumbnailerImpl()
    : QObject()
{
    connection_.reset(new QDBusConnection(
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, "album_art_generator_dbus_connection")));
    iface_.reset(new ThumbnailerInterface(service::BUS_NAME, service::THUMBNAILER_BUS_PATH, *connection_));
}

ThumbnailerImpl::ThumbnailerImpl(QDBusConnection const& connection)
    : QObject()
{
    // use the given connection instead of instantiating a new one
    iface_.reset(new ThumbnailerInterface(service::BUS_NAME, service::THUMBNAILER_BUS_PATH, connection));
}

QSharedPointer<Request> ThumbnailerImpl::getAlbumArt(QString const& artist,
                                                     QString const& album,
                                                     QSize const& requestedSize)
{
    // perform dbus call
    auto reply = iface_->GetAlbumArt(artist, album, requestedSize);
    std::unique_ptr<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(reply));
    return QSharedPointer<Request>(new Request(new RequestImpl(requestedSize, std::move(watcher))));
}

QSharedPointer<Request> ThumbnailerImpl::getArtistArt(QString const& artist,
                                                      QString const& album,
                                                      QSize const& requestedSize)
{
    // perform dbus call
    auto reply = iface_->GetArtistArt(artist, album, requestedSize);
    std::unique_ptr<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(reply));
    return QSharedPointer<Request>(new Request(new RequestImpl(requestedSize, std::move(watcher))));
}

QSharedPointer<Request> ThumbnailerImpl::getThumbnail(QString const& filename, QSize const& requestedSize)
{
    // perform dbus call
    auto reply = iface_->GetThumbnail(filename, requestedSize);
    std::unique_ptr<QDBusPendingCallWatcher> watcher(new QDBusPendingCallWatcher(reply));
    return QSharedPointer<Request>(new Request(new RequestImpl(requestedSize, std::move(watcher))));
}

}  // namespace internal

Request::Request(internal::RequestImpl* impl)
    : p_(impl)
{
    connect(p_.data(), &internal::RequestImpl::finished, this, &Request::finished);
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

bool Request::finishedSucessfully() const
{
    return p_->finishedSucessfully();
}

void Request::waitForFinished()
{
    p_->waitForFinished();
}

Thumbnailer::Thumbnailer()
    : p_(new internal::ThumbnailerImpl())
{
}

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

}  // namespace thumbnailer

}  // namespace unity

#include "libthumbnailer.moc"
