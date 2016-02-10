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
#include "dbus_connection.h"
#include "get_local_thumbnail.h"
#include "get_remote_thumbnail.h"
#include "show_stats.h"
#include "shutdown.h"

#include <boost/filesystem.hpp>

#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <stdio.h>

using namespace std;
using namespace unity::thumbnailer::tools;

namespace
{

string prog_name;

template<typename A>
Action::UPtr create_action(QCommandLineParser& parser)
{
    return Action::UPtr(new A(parser));
}

typedef map<char const*, pair<Action::UPtr(*)(QCommandLineParser&), char const*>> ActionMap;

// Table that maps commands to their actions.
// Add new commands to this table, and implement each command in
// a class derived from Action.

ActionMap const valid_actions =
{
    { "stats",       { &create_action<ShowStats>,          "Show statistics" } },
    { "zero-stats",  { &create_action<Clear>,              "Zero statistics counters" } },
    { "get",         { &create_action<GetLocalThumbnail>,  "Get thumbnail from local file" } },
    { "get-artist",  { &create_action<GetRemoteThumbnail>, "Get artist thumbnail" } },
    { "get-album",   { &create_action<GetRemoteThumbnail>, "Get album thumbnail" } },
    { "clear",       { &create_action<Clear>,              "Clear caches" } },
    { "compact",     { &create_action<Clear>,              "Compact caches" } },
    { "shutdown",    { &create_action<Shutdown>,           "Shut down thumbnailer service" } }
};

QString command_summary()
{
    stringstream s;
    s << "Commands:" << endl;
    for (auto const& a : valid_actions)
    {
        s << "  " << setw(11) << left << a.first << " " << a.second.second << endl;
    }
    return QString::fromStdString(s.str());
}

// Check if we have a valid command. If so, instantiate the
// corresponding action and execute it.

void parse_and_execute()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Thumbnailer administrative tool"));
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("The command to execute."));
    parser.parse(QCoreApplication::arguments());

    auto args = parser.positionalArguments();
    if (args.empty())
    {
        throw parser.helpText() + "\n" + command_summary();
    }
    QString cmd = args.first();
    for (auto a : valid_actions)
    {
        if (QString(a.first) == cmd)
        {
            auto action = a.second.first(parser);
            DBusConnection conn;
            action->run(conn);
            return;
        }
    }
    fprintf(stderr, "%s: %s: invalid command\n", prog_name.c_str(), qPrintable(cmd));
    throw parser.helpText() + "\n" + command_summary();
}

}  // namespace

int main(int argc, char* argv[])
{
    int rc = EXIT_FAILURE;
    try
    {
        QCoreApplication app(argc, argv);
        boost::filesystem::path p = app.applicationName().toStdString();
        prog_name = p.filename().native();
        parse_and_execute();
        rc = EXIT_SUCCESS;
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        cerr << prog_name << ": " << e.what() << endl;
    }
    catch (QString const& msg)
    {
        cerr << prog_name << ": " << msg.toStdString() << endl;
    }
    catch (string const& msg)
    {
        cerr << prog_name << ": " << msg << endl;
    }
    catch (char const* msg)
    {
        cerr << prog_name << ": " << msg << endl;
    }
    // LCOV_EXCL_STOP
    // No catch for ... here. It's better to dump core.
    return rc;
}
