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

#include "get_local_thumbnail.h"

#include "parse_size.h"
#include <internal/file_io.h>
#include <internal/safe_strerror.h>
#include "util.h"

#include <boost/filesystem.hpp>

#include <cassert>

#include <fcntl.h>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

GetLocalThumbnail::GetLocalThumbnail(QCommandLineParser& parser)
    : Action(parser)
    , size_(0, 0)
{
    parser.addPositionalArgument(QStringLiteral("get"), QStringLiteral("Get thumbnail from local file"), QStringLiteral("get"));
    parser.addPositionalArgument(QStringLiteral("source_file"), QStringLiteral("Path to image, audio, or video file"));
    parser.addPositionalArgument(QStringLiteral("dir"), QStringLiteral("Output directory (default: current dir)"), QStringLiteral("[dir]"));
    QCommandLineOption size_option(QStringList() << QStringLiteral("s")
                                                 << QStringLiteral("size"),
                                   QStringLiteral("Thumbnail size, e.g. \"240x480\" or \"480\" (default: largest available size)"),
                                   QStringLiteral("size"));
    parser.addOption(size_option);

    if (!parser.parse(QCoreApplication::arguments()))
    {
        throw parser.errorText() + "\n\n" + parser.helpText();
    }
    if (parser.isSet(help_option_))
    {
        throw parser.helpText();
    }

    auto args = parser.positionalArguments();
    assert(args.first() == QString("get"));

    if (args.size() < 2 || args.size() > 3)
    {
        throw parser.helpText();
    }
    input_path_ = QString::fromStdString(boost::filesystem::canonical(args[1].toStdString()).native());
    auto dir = args.size() == 3 ? args[2] : current_directory();
    output_dir_ = QString::fromStdString(boost::filesystem::canonical(dir.toStdString()).native());

    if (parser.isSet(size_option))
    {
        size_ = parse_size(parser.value(size_option));
        if (!size_.isValid())
        {
            throw QString("GetLocalThumbnail(): invalid size: " + parser.value(size_option));
        }
    }
}

GetLocalThumbnail::~GetLocalThumbnail()
{
}

void GetLocalThumbnail::run(DBusConnection& conn)
{
    try
    {
        auto reply = conn.thumbnailer().GetThumbnail(input_path_, size_);
        reply.waitForFinished();
        if (!reply.isValid())
        {
            throw reply.error().message();  // LCOV_EXCL_LINE
        }
        QByteArray thumbnail = reply.value();

        string out_path = make_output_path(input_path_.toStdString(), size_, output_dir_.toStdString());
        write_file(out_path, thumbnail);
    }
    // LCOV_EXCL_START
    catch (std::exception const& e)
    {
        throw string("GetLocalThumbnail::run(): ") + e.what();
    }
    // LCOV_EXCL_STOP
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
