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

#include "inactivityhandler.h"

#include <QCoreApplication>

#include <sstream>
#include <string>

const int MAX_INACTIVITY_TIME = 30000;  // max inactivity time before exiting the app, in milliseconds

using namespace unity::thumbnailer::service;

int get_env_inactivity_time(int default_value)
{
    char const* c_idle_time = getenv("THUMBNAILER_MAX_IDLE");
    if (c_idle_time)
    {
        std::string str_idle_time(c_idle_time);
        try
        {
            int env_value = std::stoi(str_idle_time);
            return env_value;
        }
        catch (std::exception& e)
        {
            std::ostringstream s;
            s << "InactivityHandler::InactivityHandler(): Value for env variable THUMBNAILER_MAX_IDLE \""
              << str_idle_time << "\" is not correct. It must be an integer.";
            throw std::invalid_argument(s.str());
        }
    }
    return default_value;
}

InactivityHandler::InactivityHandler(DBusInterface& iface)
    : QObject(&iface)
{
    timer_.setInterval(get_env_inactivity_time(MAX_INACTIVITY_TIME));

    // connect dbus interface inactivity signals to the QTimer start and stop
    connect(&iface, &DBusInterface::endInactivity, &timer_, &QTimer::stop);
    connect(&iface, &DBusInterface::startInactivity, &timer_, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(&timer_, &QTimer::timeout, QCoreApplication::instance(), &QCoreApplication::quit);
}
