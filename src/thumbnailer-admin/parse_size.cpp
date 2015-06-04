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

#include "parse_size.h"

#include <QStringList>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

QSize parse_size(QString const& s)
{
    try
    {
        // Check if we have something of the form 240x480.
        QStringList wxh = s.split('x');
        if (wxh.size() == 2)
        {
            int width = stoi(wxh.first().toStdString());
            int height = stoi(wxh.last().toStdString());
            return QSize(width, height);
        }
        else
        {
            int size = stoi(wxh.first().toStdString());
            return QSize(size, size);
        }
    }
    catch (...)
    {
        return QSize();
    }
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
