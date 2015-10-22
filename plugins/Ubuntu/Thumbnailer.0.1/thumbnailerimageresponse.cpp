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
                                                   QSharedPointer<thumb_qt::Request> const& request)
    : requested_size_(requested_size)
    , request_(request)
    , default_image_(default_image)
{
    Q_ASSERT(request);
    connect(request_.data(), &thumb_qt::Request::finished, this, &ThumbnailerImageResponse::requestFinished);
}

ThumbnailerImageResponse::ThumbnailerImageResponse(QSize const& requested_size,
                                                   QString const& default_image)
    : requested_size_(requested_size)
    , default_image_(default_image)
{
    // Queue the signal emission so there is time for the caller to connect.
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}

ThumbnailerImageResponse::~ThumbnailerImageResponse()
{
    // TODO: Once we remove fallback image support, this test for request_ != nullptr
    //       (here and elsewhere) needs to be removed because request_ is nullptr
    //       only if the default image constructor above was called.
    if (request_)
    {
        request_->cancel();
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
    if (request_)
    {
        request_->cancel();
    }
}

void ThumbnailerImageResponse::requestFinished()
{
    if (request_->isCancelled())
    {
        return;
    }
    if (!request_->isValid())
    {
        qWarning() << "ThumbnailerImageResponse::dbusCallFinished(): D-Bus error: " << request_->errorMessage();
    }
    Q_EMIT finished();
}

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
