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

#pragma once

#include <unity/util/ResourcePtr.h>

#include <string>
#include <QProcess>

class ArtServer final {
public:
    ArtServer();
    ~ArtServer();

    std::string const& server_url() const;
    void block_access();
    void unblock_access();

private:
    QProcess server_;
    unity::util::ResourcePtr<int, void(*)(int)> socket_;
    std::string server_url_;
    std::string blocked_server_url_;
    bool blocked_ = false;

    void update_env();
};
