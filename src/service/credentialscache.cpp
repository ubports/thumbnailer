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

#include <assert.h>
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

CredentialsCache::CredentialsCache(QDBusConnection const& bus)
    : bus_daemon_(DBUS_BUS_NAME, DBUS_BUS_PATH, bus)
    , apparmor_enabled_(aa_is_enabled())
{
}

CredentialsCache::~CredentialsCache() = default;

void CredentialsCache::get(QString const& peer, Callback callback)
{
    // Return the credentials directly if they are cached
    try
    {
        Credentials const& credentials = cache_.at(peer);
        callback(credentials);
        return;
    }
    catch (std::out_of_range const &)
    {
        // ignore
    }

    // If the credentials are already being requested, add ourselves
    // to the callback list.
    try
    {
        unique_ptr<Request>& request = pending_.at(peer);
        request->callbacks.push_back(callback);
        return;
    }
    catch (std::out_of_range const &)
    {
        // ignore
    }

    // Ask the bus daemon for the peer's credentials
    unique_ptr<Request> request(
        new Request(bus_daemon_.GetConnectionCredentials(peer)));
    QObject::connect(&request->watcher, &QDBusPendingCallWatcher::finished,
                     [this, peer](QDBusPendingCallWatcher *watcher) {
                         this->received_credentials(peer, *watcher);
                     });
    request->callbacks.push_back(callback);
    pending_.emplace(peer, std::move(request));
}

void CredentialsCache::received_credentials(QString const& peer, QDBusPendingReply<QVariantMap> reply)
{
    Credentials credentials;
    if (reply.isError())
    {
        qWarning() << "CredentialsCache::received_credentials: error retrieving credentials for" << peer << ":" << reply.error().message();
    }
    else
    {
        credentials.valid = true;
        // The contents of this map are described in the specification here:
        // http://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-get-connection-credentials
        credentials.user = reply.value().value(UNIX_USER_ID).value<uint32_t>();
        if (apparmor_enabled_)
        {
            QByteArray label = reply.value().value(LINUX_SECURITY_LABEL).value<QByteArray>();
            if (label.size() > 0) {
                // The label is null terminated.
                assert(label[label.size()-1] == '\0');
                label.truncate(label.size() - 1);
                // Trim the mode off the end of the label.
                int pos = label.lastIndexOf(' ');
                if (pos > 0 && label.endsWith(')') && label[pos+1] == '(')
                {
                    label.truncate(pos);
                }
                credentials.label = string(label.constData(), label.size());
            }
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
