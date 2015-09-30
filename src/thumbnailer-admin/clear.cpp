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
    if (command_ == QLatin1String("clear"))
    {
        parser.addPositionalArgument(QStringLiteral("clear"), QStringLiteral("Clear caches"), QStringLiteral("clear"));
    }
    else if (command_ == QLatin1String("zero-stats"))
    {
        parser.addPositionalArgument(QStringLiteral("zero-stats"), QStringLiteral("Zero statistics counters"), QStringLiteral("zero-stats"));
    }
    else
    {
        parser.addPositionalArgument(QStringLiteral("compact"), QStringLiteral("Compact caches"), QStringLiteral("compact"));
    }
    parser.addPositionalArgument(QStringLiteral("cache_id"), QStringLiteral("Select cache (i=image, t=thumbnail, f=failure)"), QStringLiteral("[cache_id]"));

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
        throw QStringLiteral("too many arguments for ") + command_ + " command" + parser.errorText() + "\n\n" + parser.helpText();
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
        throw QStringLiteral("invalid cache_id: ") + arg + "\n" + parser.helpText();
    }
}

Clear::~Clear()
{
}

void Clear::run(DBusConnection& conn)
{
    decltype(&AdminInterface::Clear) method;
    if (command_ == QLatin1String("clear"))
    {
        method = &AdminInterface::Clear;
    }
    else if (command_ == QLatin1String("zero-stats"))
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
