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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#pragma once

#include <internal/thumbnailer.h>
#include "inactivityhandler.h"
#include "stats.h"

#include <QDBusContext>

namespace unity
{

namespace thumbnailer
{

namespace service
{

class AdminInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    AdminInterface(std::shared_ptr<unity::thumbnailer::internal::Thumbnailer> const& thumbnailer,
                   std::shared_ptr<InactivityHandler> const& inactivity_handler,
                   QObject* parent = nullptr)
        : QObject(parent)
        , thumbnailer_(thumbnailer)
        , inactivity_handler_(inactivity_handler)
    {
    }
    ~AdminInterface() = default;  // LCOV_EXCL_LINE  // False negative from gcovr.

    AdminInterface(AdminInterface const&) = delete;
    AdminInterface& operator=(AdminInterface&) = delete;

public Q_SLOTS:
    AllStats Stats();
    void ClearStats(int cache_id);
    void Clear(int cache_id);
    void Compact(int cache_id);
    void Shutdown();

private:
    std::shared_ptr<unity::thumbnailer::internal::Thumbnailer> const& thumbnailer_;
    std::shared_ptr<InactivityHandler> inactivity_handler_;
};

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
