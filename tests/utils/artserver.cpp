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

#include <QDebug>

#include <stdexcept>
#include <stdlib.h>

ArtServer::ArtServer()
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
    apiroot_ = "http://127.0.0.1:" + port.toStdString();
    setenv("THUMBNAILER_UBUNTU_APIROOT", apiroot_.c_str(), true);
    setenv("THUMBNAILER_TEST_DEFAULT_IMAGE", THUMBNAILER_TEST_DEFAULT_IMAGE, true);
}

ArtServer::~ArtServer()
{
    server_.terminate();
    if (!server_.waitForFinished())
    {
        qCritical() << "Failed to terminate fake art server";
    }
    unsetenv("THUMBNAILER_UBUNTU_APIROOT");
}

std::string const& ArtServer::apiroot() const
{
    return apiroot_;
}
