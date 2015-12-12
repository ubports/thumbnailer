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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include "artserver.h"
#include <testsetup.h>
#include <internal/env_vars.h>
#include <internal/raii.h>

#include <QDebug>

#include <stdexcept>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

ArtServer::ArtServer()
    : socket_(-1, unity::thumbnailer::internal::do_close)
{
    server_.setStandardInputFile(QProcess::nullDevice());
    server_.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    server_.start(FAKE_ART_SERVER, QStringList());
    if (!server_.waitForStarted())
    {
        throw std::runtime_error("ArtServer::ArtServer(): wait for server start failed");
    }
    if (!server_.waitForReadyRead())
    {
        throw std::runtime_error("ArtServer::ArtServer(): wait for read from server failed");
    }
    auto port = QString::fromUtf8(server_.readAllStandardOutput()).trimmed();
    server_url_ = "http://127.0.0.1:" + port.toStdString();

    // Create a bound TCP socket with no listen queue.  Attempts to
    // connect to this port can not succeed.  And as long as we hold
    // the socket open, no other socket can reuse the port number.
    socket_.reset(socket(AF_INET, SOCK_STREAM, 0));
    if (socket_.get() < 0)
    {
        throw std::runtime_error("ArtServer::ArtServer(): Could not create socket");
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    if (bind(socket_.get(), reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(struct sockaddr_in)) < 0)
    {
        throw std::runtime_error("ArtServer::ArtServer(): Could not bind socket");
    }
    socklen_t length = sizeof(struct sockaddr_in);
    if (getsockname(socket_.get(), reinterpret_cast<struct sockaddr*>(&addr),
                    &length) < 0 || length > sizeof(struct sockaddr_in))
    {
        throw std::runtime_error("ArtServer::ArtServer(): Could not get socket name");
    }
    blocked_server_url_ = "http://127.0.0.1:" + std::to_string(addr.sin_port);

    setenv("THUMBNAILER_TEST_DEFAULT_IMAGE", THUMBNAILER_TEST_DEFAULT_IMAGE, true);
    update_env();
}

ArtServer::~ArtServer()
{
    server_.terminate();
    if (!server_.waitForFinished())
    {
        qCritical() << "Failed to terminate fake art server";
    }
    unsetenv(unity::thumbnailer::internal::UBUNTU_SERVER_URL);
}

std::string const& ArtServer::server_url() const
{
    if (blocked_)
    {
        return blocked_server_url_;
    }
    else
    {
        return server_url_;
    }
}

void ArtServer::block_access()
{
    blocked_ = true;
    update_env();
}

void ArtServer::unblock_access()
{
    blocked_ = false;
    update_env();
}

void ArtServer::update_env()
{
    setenv(unity::thumbnailer::internal::UBUNTU_SERVER_URL, server_url().c_str(), true);
}
