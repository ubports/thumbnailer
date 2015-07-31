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

#include "clear.h"

#include <cassert>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

Clear::Clear(QCommandLineParser& parser)
    : Action(parser)
    , cache_id_(0)
{
    assert(command_ == "clear" || command_ == "zero-stats" || command_ == "compact");
    if (command_ == "clear")
    {
        parser.addPositionalArgument("clear", "Clear caches", "clear");
    }
    else if (command_ == "zero-stats")
    {
        parser.addPositionalArgument("zero-stats", "Zero statistics counters", "zero-stats");
    }
    else
    {
        parser.addPositionalArgument("compact", "Compact caches", "compact");
    }
    parser.addPositionalArgument("cache_id", "Select cache (i=image, t=thumbnail, f=failure)", "[cache_id]");

    if (!parser.parse(QCoreApplication::arguments()))
    {
        throw parser.errorText() + "\n\n" + parser.helpText();
    }
    if (parser.isSet(help_option_))
    {
        throw parser.helpText();
    }

    auto args = parser.positionalArguments();
    if (args.size() > 2)
    {
        throw QString("too many arguments for ") + command_ + " command" + parser.errorText() + "\n\n" + parser.helpText();
    }

    if (args.size() == 2)
    {
        QStringList valid_args{ "", "i", "t", "f" };  // Must stay in sync with CacheSelector enum in thumbnailer.h!
        auto arg = args[1];
        for (int i = 0; i < valid_args.size(); ++i)
        {
            if (arg == valid_args[i])
            {
                cache_id_ = i;
                return;
            }
        }
        throw QString("invalid cache_id: ") + arg + "\n" + parser.helpText();
    }
}

Clear::~Clear()
{
}

void Clear::run(DBusConnection& conn)
{
    decltype(&AdminInterface::Clear) method;
    if (command_ == "clear")
    {
        method = &AdminInterface::Clear;
    }
    else if (command_ == "zero-stats")
    {
        method = &AdminInterface::ClearStats;
    }
    else
    {
        method = &AdminInterface::Compact;
    }
    auto reply = (conn.admin().*method)(cache_id_);
    reply.waitForFinished();
    if (!reply.isValid())
    {
        throw reply.error().message();  // LCOV_EXCL_LINE
    }
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
