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

#include "action.h"

namespace unity
{

namespace thumbnailer
{

namespace tools
{

class ShowStats : public Action
{
public:
    UNITY_DEFINES_PTRS(ShowStats);

    ShowStats(QCommandLineParser& parser);
    virtual ~ShowStats();

    virtual void run(DBusConnection& conn) override;

private:
    void show_stats(unity::thumbnailer::service::CacheStats const& st);

    bool show_histogram_ = false;
    bool show_image_stats_ = true;
    bool show_thumbnail_stats_ = true;
    bool show_failure_stats_ = true;
};

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
