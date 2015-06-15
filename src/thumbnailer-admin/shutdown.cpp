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

#include "shutdown.h"

#include <cassert>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

Shutdown::Shutdown(QCommandLineParser& parser)
    : Action(parser)
{
    assert(command_ == "shutdown");
    parser.addPositionalArgument("shutdown", "Shut down thumbnailer service", "shutdown");

    if (!parser.parse(QCoreApplication::arguments()))
    {
        throw parser.errorText() + "\n\n" + parser.helpText();
    }
    if (parser.isSet(help_option_))
    {
        throw parser.helpText();
    }

    auto args = parser.positionalArguments();
    if (args.size() > 1)
    {
        throw QString("too many arguments for ") + command_ + " command" + parser.errorText() + "\n\n" + parser.helpText();
    }
}

Shutdown::~Shutdown()
{
}

void Shutdown::run(DBusConnection& conn)
{
    auto reply = conn.admin().Shutdown();
    reply.waitForFinished();
    if (!reply.isValid())
    {
        throw reply.error().message();  // LCOV_EXCL_LINE
    }
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
