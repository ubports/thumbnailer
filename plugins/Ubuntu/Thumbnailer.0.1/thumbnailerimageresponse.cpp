/*
 * Copyright 2015 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <thumbnailerimageresponse.h>
#include <utils/artgeneratorcommon.h>

#include <QDebug>

namespace thumb_qt = unity::thumbnailer::qt;

namespace unity
{

namespace thumbnailer
{

namespace qml
{

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   QString const& default_image,
                                                   RateLimiter* backlog_limiter,
                                                   std::function<QSharedPointer<thumb_qt::Request>()> job)
    : requested_size_(requested_size)
    , backlog_limiter_(backlog_limiter)
    , job_(job)
    , default_image_(default_image)
{
    auto send_request = [this, job]
    {
        using namespace std;
        request_ = job();
        connect(request_.data(), &thumb_qt::Request::finished, this, &ThumbnailerImageResponse::requestFinished);
    };
    cancel_func_ = backlog_limiter_->schedule(send_request);
}

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   QString const& default_image)
    : requested_size_(requested_size)
    , default_image_(default_image)
    , cancel_func_([]{})
{
    // Queue the signal emission so there is time for the caller to connect.
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

ThumbnailerImageResponse::~ThumbnailerImageResponse()
{
    if (request_)
    {
        //request_->cancel();
    }
}

QQuickTextureFactory* ThumbnailerImageResponse::textureFactory() const
{
    if (request_ && request_->isValid())
    {
        return QQuickTextureFactory::textureFactoryForImage(request_->image());
    }
    else
    {
        char const* env_default = getenv("THUMBNAILER_TEST_DEFAULT_IMAGE");
        QImage aux;
        aux.load(env_default ? QString(env_default) : default_image_);
        return QQuickTextureFactory::textureFactoryForImage(aux);
    }
}

void ThumbnailerImageResponse::cancel()
{
    // Remove request from queue if it is still in there.
    cancel_func_();

    if (request_)
    {
        //request_->cancel();
    }
}

void ThumbnailerImageResponse::requestFinished()
{
    backlog_limiter_->done();

    if (!request_->isValid())
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): D-Bus error: " << request_->errorMessage();
    }
    Q_EMIT finished();
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
