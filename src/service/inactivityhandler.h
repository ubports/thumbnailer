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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

namespace unity
{

namespace thumbnailer
{

namespace service
{

class InactivityHandler : public QObject
{
    Q_OBJECT
public:
    InactivityHandler(std::function<void()> const& timer_func);
    ~InactivityHandler();

    void request_started();
    void request_completed();

public Q_SLOTS:
    void timer_expired();

private:
    std::function<void()> timer_func_;
    int num_active_requests_;
    QTimer timer_;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
