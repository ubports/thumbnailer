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

#include <internal/ubuntuserverdownloader.h>

#include <internal/artreply.h>
#include <settings.h>

#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>

#include <cassert>

#include <netdb.h>
#include <unistd.h>

using namespace std;

// const strings
namespace
{

#define SERVER_DOMAIN_NAME "dash.ubuntu.com"

constexpr const char UBUNTU_SERVER_BASE_URL[] = "https://" SERVER_DOMAIN_NAME;
constexpr const char ALBUM_ART_BASE_URL[] = "musicproxy/v1/album-art";
constexpr const char ARTIST_ART_BASE_URL[] = "musicproxy/v1/artist-art";
}

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

// TODO: Hack to work around QNetworkAccessManager problems when the device is in flight mode.

bool network_is_connected()
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *result;
    if (getaddrinfo(SERVER_DOMAIN_NAME, "80", &hints, &result) != 0)
    {
        return false;  // LCOV_EXCL_LINE
    }
    freeaddrinfo(result);
    return true;
}

}

class UbuntuServerArtReply : public ArtReply
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
    Q_DISABLE_COPY(UbuntuServerArtReply)

    // LCOV_EXCL_START
    // TODO: Hack for QNetworkAccessManager. Creates a failed network reply
    //       used when we conclude that the device is in flight mode.
    UbuntuServerArtReply(QString const& url)
        : ArtReply(nullptr)
        , url_string_(url)
        , error_string_(QStringLiteral("network down"))
        , status_(ArtReply::network_down)
        , reply_(nullptr)
    {
        assert(!url.isEmpty());
    }
    // LCOV_EXCL_STOP

    UbuntuServerArtReply(QString const& url,
                         QNetworkReply* reply,
                         chrono::milliseconds timeout)
        : ArtReply(nullptr)
        , url_string_(url)
        , status_(ArtReply::not_finished)
        , reply_(reply)
    {
        assert(!url.isEmpty());
        assert(reply_);

        connect(&timer_, &QTimer::timeout, this, &UbuntuServerArtReply::timeout);
        timer_.setSingleShot(true);
        timer_.start(timeout.count());
    }

    Status status() const override
    {
        return status_;
    }

    QString error_string() const override
    {
        assert(status_ != ArtReply::Status::not_finished);
        return error_string_;
    }

    QByteArray const& data() const override
    {
        assert(status_ != ArtReply::Status::not_finished);
        return data_;
    }

    QString url_string() const override
    {
        return url_string_;
    }

    void set_status()
    {
        status_ = ArtReply::Status::temporary_error;  // Default, in case none of the tests below match.
        error_string_ = reply_->errorString();

        auto error_code = reply_->error();
        switch (error_code)
        {
            case QNetworkReply::NoError:
                status_ = ArtReply::Status::success;
                return;
            case QNetworkReply::ContentNotFoundError:
            case QNetworkReply::ContentGoneError:
                // Authoritive "no artwork available" response (404 or 410).
                status_ = ArtReply::Status::not_found;
                return;
            case QNetworkReply::ProtocolInvalidOperationError:
                // Probably HTTP error 400 (Bad Request) but could
                // potentially be something else that is surprising,
                // deal with it below.
                break;
            default:
                // All the other errors indicate things that should never happen,
                // such as BackgroundRequestNotAllowedError, or a temporary
                // network error, such as TimeoutError.
                return;
        }

        // Pull out the HTTP response code.
        QVariant att = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (att.type() != QVariant::Type::Int)
        {
            abort();  // TODO: remove this!
            return;  // LCOV_EXCL_LINE
        }
        auto http_code = att.toInt();
        qDebug() << "unexpected HTTP error" << http_code << "for" << url_string_;

        // Classify the HTTP error as permanent or potentially recoverable.
        switch (http_code)
        {
            // No chance of recovery with a retry.
            case 400:  // Bad Request
            case 403:  // Forbidden
                status_ = ArtReply::Status::hard_error;
                break;
            default:
                qDebug() << "unexpected QNetworkReply::NetworkError" << int(error_code) << "for" << url_string_;
                break;
        }
    }

public Q_SLOTS:
    void download_finished()
    {
        // TODO: Hack two work around QNetworkAccessManager problems when device is in flight mode.
        //       reply_ is nullptr only if the network is down.
        if (!reply_)
        {
            // LCOV_EXCL_START
            Q_EMIT finished();
            return;
            // LCOV_EXCL_STOP
        }

        timer_.stop();

        set_status();
        if (status_ == ArtReply::Status::success)
        {
            data_ = reply_->readAll();
        }
        Q_EMIT finished();
    }

    void timeout()
    {
        status_ = ArtReply::Status::temporary_error;
        reply_->abort();
        error_string_ = QStringLiteral("Request timed out");
    }

private:
    QString const url_string_;
    QString error_string_;
    QByteArray data_;
    ArtReply::Status status_;
    QNetworkReply* reply_;
    QTimer timer_;
};

// helper methods to retrieve image urls
QUrl get_art_url(
    QString const& base_url, QString const& artist, QString const& album, QString const& api_key)
{
    QString prefix_api_root = UBUNTU_SERVER_BASE_URL;
    char const* apiroot_c = getenv("THUMBNAILER_UBUNTU_APIROOT");
    if (apiroot_c)
    {
        prefix_api_root = apiroot_c;
    }

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("artist"), artist);
    q.addQueryItem(QStringLiteral("album"), album);
    q.addQueryItem(QStringLiteral("key"), api_key);

    QUrl url(prefix_api_root + "/" + base_url);
    url.setQuery(q);
    return url;
}

QUrl get_album_art_url(QString const& artist, QString const& album, QString const& api_key)
{
    return get_art_url(ALBUM_ART_BASE_URL, artist, album, api_key);
}

QUrl get_artist_art_url(QString const& artist, QString const& album, QString const& api_key)
{
    return get_art_url(ARTIST_ART_BASE_URL, artist, album, api_key);
}

namespace
{

QString api_key()
{
    // the API key is not expected to change, so don't monitor it
    Settings settings;

    auto key = QString::fromStdString(settings.art_api_key());
    if (key.isEmpty())
    {
        qCritical() << "Failed to get API key";  // LCOV_EXCL_LINE
    }
    return key;
}

}  // namespace

UbuntuServerDownloader::UbuntuServerDownloader(QObject* parent)
    : ArtDownloader(parent)
    , api_key_(api_key())
    , network_manager_(make_shared<QNetworkAccessManager>(this))
{
}

shared_ptr<ArtReply> UbuntuServerDownloader::download_album(QString const& artist,
                                                            QString const& album,
                                                            chrono::milliseconds timeout)
{
    return download_url(get_album_art_url(artist, album, api_key_), timeout);
}

shared_ptr<ArtReply> UbuntuServerDownloader::download_artist(QString const& artist,
                                                             QString const& album,
                                                             chrono::milliseconds timeout)
{
    return download_url(get_artist_art_url(artist, album, api_key_), timeout);
}

shared_ptr<ArtReply> UbuntuServerDownloader::download_url(QUrl const& url, chrono::milliseconds timeout)
{
    assert_valid_url(url);

    // TODO: Hack to work around QNetworkAccessManager problems when in flight mode.
    shared_ptr<UbuntuServerArtReply> art_reply;
    if (network_is_connected())
    {
        QNetworkReply* reply = network_manager_->get(QNetworkRequest(url));
        art_reply = make_shared<UbuntuServerArtReply>(url.toString(), reply, timeout);
        connect(reply, &QNetworkReply::finished, art_reply.get(), &UbuntuServerArtReply::download_finished);
    }
    else
    {
        // LCOV_EXCL_START
        art_reply = make_shared<UbuntuServerArtReply>(url.toString());
        QMetaObject::invokeMethod(art_reply.get(), "download_finished", Qt::QueuedConnection);
        // LCOV_EXCL_STOP
    }
    return art_reply;
}

std::shared_ptr<QNetworkAccessManager> UbuntuServerDownloader::network_manager() const
{
    return network_manager_;
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity

#include "ubuntuserverdownloader.moc"
