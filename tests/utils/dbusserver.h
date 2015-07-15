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

#include <utils/thumbnailerinterface.h>
#include <utils/admininterface.h>

#include <QProcess>
#include <QSharedPointer>

#include <memory>

namespace QtDBusTest
{
class DBusTestRunner;
class QProcessDBusService;
}

class DBusServer final {
public:
    DBusServer();
    ~DBusServer();

    std::unique_ptr<ThumbnailerInterface> thumbnailer_;
    std::unique_ptr<AdminInterface> admin_;

    QProcess& service_process();

private:
    std::unique_ptr<QtDBusTest::DBusTestRunner> runner_;
    QSharedPointer<QtDBusTest::QProcessDBusService> service_;
};
