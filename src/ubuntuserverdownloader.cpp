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

bool network_down_error(QNetworkReply::NetworkError error)
{
    switch (error)
    {
        // add here all the cases that you consider as network errors
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::OperationCanceledError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::UnknownNetworkError:
            return true;
        default:
            return false;
    }
}

class UbuntuServerArtReply : public ArtReply
{
    Q_OBJECT

public:
    Q_DISABLE_COPY(UbuntuServerArtReply)

    // LCOV_EXCL_START
    UbuntuServerArtReply(QString const& url)
        : ArtReply(nullptr)
        , is_running_(false)
        , error_string_("network down")
        , error_(QNetworkReply::TemporaryNetworkFailureError)
        , url_string_(url)
        , succeeded_(false)
        , network_down_error_(true)
        , reply_(nullptr)
    {
    }
    // LCOV_EXCL_STOP

    UbuntuServerArtReply(QString const& url,
                         QNetworkReply* reply,
                         chrono::milliseconds timeout)
        : ArtReply(nullptr)
        , is_running_(false)
        , error_(QNetworkReply::NoError)
        , url_string_(url)
        , succeeded_(false)
        , network_down_error_(false)
        , reply_(reply)
    {
        assert(reply_);
        connect(&timer_, &QTimer::timeout, this, &UbuntuServerArtReply::timeout);
        timer_.setSingleShot(true);
        timer_.start(timeout.count());
    }

    bool succeeded() const override
    {
        return succeeded_;
    };

    bool is_running() const override
    {
        return is_running_;
    };

    QString error_string() const override
    {
        return error_string_;
    }

    bool not_found_error() const override
    {
        switch (error_)
        {
            // add here all the cases that you consider as source not found
            case QNetworkReply::ContentNotFoundError:
            case QNetworkReply::ContentGoneError:
                return true;
            default:
                return false;
        }
    }

    QByteArray const& data() const override
    {
        return data_;
    }

    QString url_string() const override
    {
        return url_string_;
    }

    bool network_down() const override
    {
        return network_down_error_;
    }

public Q_SLOTS:
    void download_finished()
    {
        // TODO: Hack two work around QNetworkAccessManager problems when device is in flight mode.
        //       reply_ is nullptr only if the network is down.
        if (!reply_)
        {
            Q_EMIT finished();  // LCOV_EXCL_LINE
            return;
        }

        timer_.stop();

        QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
        assert(reply);

        is_running_ = false;
        error_ = reply->error();
        if (error_)
        {
            error_string_ = reply->errorString();
            network_down_error_ = network_down_error(error_);
        }
        else
        {
            data_ = reply->readAll();
            succeeded_ = true;
        }
        Q_EMIT finished();
        reply->deleteLater();
    }

    void timeout()
    {
        reply_->abort();
    }

private:
    bool is_running_;
    QString error_string_;
    QNetworkReply::NetworkError error_;
    QByteArray data_;
    QString url_string_;
    bool succeeded_;
    bool network_down_error_;
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
    q.addQueryItem("artist", artist);
    q.addQueryItem("album", album);
    q.addQueryItem("key", api_key);

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

UbuntuServerDownloader::UbuntuServerDownloader(QObject* parent)
    : ArtDownloader(parent)
    , network_manager_(make_shared<QNetworkAccessManager>(this))
{
    set_api_key();
}

void UbuntuServerDownloader::set_api_key()
{
    // the API key is not expected to change, so don't monitor it
    Settings settings;

    api_key_ = QString::fromStdString(settings.art_api_key());
    if (api_key_.isEmpty())
    {
        // TODO do something with the error
        qCritical() << "Failed to get API key";  // LCOV_EXCL_LINE
    }
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
