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

#include "credentialscache.h"

#include <QDBusPendingCallWatcher>
#include <vector>
#include <sys/apparmor.h>

using namespace std;

namespace {

char const DBUS_BUS_NAME[] = "org.freedesktop.DBus";
char const DBUS_BUS_PATH[] = "/org/freedesktop/DBus";

char const UNIX_USER_ID[] = "UnixUserID";
char const LINUX_SECURITY_LABEL[] = "LinuxSecurityLabel";

}

namespace unity
{

namespace thumbnailer
{

namespace service
{

struct CredentialsCache::Request
{
    QDBusPendingCallWatcher watcher;
    std::vector<CredentialsCache::Callback> callbacks;

    Request(QDBusPendingReply<QVariantMap> call) : watcher(call) {}
};

CredentialsCache::CredentialsCache(QDBusConnection const& bus, QObject *parent)
    : QObject(parent)
    , bus_daemon_(DBUS_BUS_NAME, DBUS_BUS_PATH, bus)
    , apparmor_enabled_(aa_is_enabled())
{
}

CredentialsCache::~CredentialsCache() = default;

void CredentialsCache::get(QString const& peer, Callback callback)
{
    // Have we already cached these credentials?
    try
    {
        Credentials const& credentials = cache_.at(peer);
        callback(credentials);
        return;
    }
    catch (std::out_of_range const &)
    {
    }

    // Is there a pending request for these credentials?
    try
    {
        unique_ptr<Request>& request = pending_.at(peer);
        request->callbacks.push_back(callback);
        return;
    }
    catch (std::out_of_range const &)
    {
    }

    unique_ptr<Request> request(
        new Request(bus_daemon_.GetConnectionCredentials(peer)));
    connect(&request->watcher, &QDBusPendingCallWatcher::finished,
            [this,peer](QDBusPendingCallWatcher *watcher) {
                this->received_credentials(peer, *watcher);
            });
    request->callbacks.push_back(callback);
    pending_.emplace(peer, std::move(request));
}

void CredentialsCache::received_credentials(QString const& peer, QDBusPendingReply<QVariantMap> reply)
{
    Credentials credentials;
    if (!reply.isError())
    {
        credentials.valid = true;
        // The contents of this map are described in the specification here:
        // http://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-get-connection-credentials
        credentials.user = reply.value().value(UNIX_USER_ID).value<uint32_t>();
        if (apparmor_enabled_)
        {
            QByteArray label = reply.value().value(LINUX_SECURITY_LABEL).value<QByteArray>();
            credentials.label = string(label.constData(), label.size());
        }
        else
        {
            // If AppArmor is not enabled, treat peer as unconfined.
            credentials.label = "unconfined";
        }
    }

    // FIXME: manage size of cache
    cache_.emplace(peer, credentials);

    // Notify anyone waiting on the request and remove it from the map:
    unique_ptr<Request>& request = pending_.at(peer);
    for (auto& callback : request->callbacks)
    {
        callback(credentials);
    }
    pending_.erase(peer);
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
