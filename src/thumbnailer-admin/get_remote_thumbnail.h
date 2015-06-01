/*
 * Copyright (C) 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
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

class GetRemoteThumbnail : public Action
{
public:
    UNITY_DEFINES_PTRS(GetRemoteThumbnail);

    GetRemoteThumbnail(QCommandLineParser& parser);
    virtual ~GetRemoteThumbnail();

    virtual void run(DBusConnection& conn) override;

private:
    QSize size_;
    QString artist_;
    QString album_;
    QString output_dir_;
};

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
