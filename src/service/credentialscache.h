/*
 * Copyright (C) 2015 Canonical, Ltd.
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
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 */

#pragma once

#include "businterface.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QString>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <sys/types.h>

namespace unity
{

namespace thumbnailer
{

namespace service
{


class CredentialsCache : public QObject {
    Q_OBJECT
public:
    struct Credentials
    {
        bool valid = false;
        uid_t user = 0;
        // Not using QString, because this is not necessarily unicode.
        std::string label;
    };
    typedef std::function<void(Credentials const&)> Callback;

    CredentialsCache(QDBusConnection const& bus, QObject *parent);
    ~CredentialsCache();

    CredentialsCache(CredentialsCache const&) = delete;
    CredentialsCache& operator=(CredentialsCache const&) = delete;

    // Retrieve the security credentials for the given D-Bus peer.
    void get(QString const& peer, Callback callback);

private:
    struct Request;

    BusInterface bus_daemon_;
    bool apparmor_enabled_;

    std::map<QString,Credentials> cache_;
    std::map<QString,std::unique_ptr<Request>> pending_;

    void received_credentials(QString const& peer, QDBusPendingReply<QVariantMap> reply);
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
