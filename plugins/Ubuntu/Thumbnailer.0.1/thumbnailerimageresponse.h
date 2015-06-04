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

#pragma once

#include <QQuickImageProvider>
#include <QDBusPendingCallWatcher>

#include <memory>

namespace unity
{

namespace thumbnailer
{

namespace qml
{

class ThumbnailerImageResponse : public QQuickImageResponse  // LCOV_EXCL_LINE  // False negative from gcovr
{
    Q_OBJECT
public:
    Q_DISABLE_COPY(ThumbnailerImageResponse)
    enum ResponseType
    {
        Download,
        Thumbnail
    };

    ThumbnailerImageResponse(QString const& id,
                             QSize const& requested_size,
                             QString const& default_image,
                             QDBusPendingCallWatcher *watcher=nullptr);
    ~ThumbnailerImageResponse() = default;



    QQuickTextureFactory* textureFactory() const;
    void set_default_image();
    void finish_later_with_default_image();

public Q_SLOTS:
    void dbus_call_finished(QDBusPendingCallWatcher* watcher);

private:
    QString return_default_image_based_on_mime();
    QString id_;
    QSize requested_size_;
    QQuickTextureFactory * texture_;
    QString default_image_;
    std::unique_ptr<QDBusPendingCallWatcher> watcher_;
};

}  // namespace qml

}  // namespace thumbnailer

}  // namespace unity
