/*
 * Copyright (C) 2013 Canonical Ltd.
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
 * Authored by: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 *              Michi Henning <michi.henning@canonical.com>
 */

#include "thumbnailextractor.h"

#include <internal/trace.h>

#include <QUrl>

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{

void extract_thumbnail(QUrl const& in_url, QUrl const& out_url)
{
    ThumbnailExtractor extractor;

    extractor.set_urls(in_url, out_url);
    if (extractor.extract_cover_art())
    {
        // Found embedded cover art.
        extractor.write_image();
        return;
    }

    // Otherwise, extract a still frame.
    assert(extractor.has_video());
    extractor.extract_video_frame();
    extractor.write_image();
}

}  // namespace

int main(int argc, char** argv)
{
    char const * const progname = basename(argv[0]);

    TraceMessageHandler message_handler(progname);

    gst_init(&argc, &argv);

    if (argc != 3)
    {
        cerr << "usage: " << progname << " source-file (output-file.tiff | fd:num)" << endl;
        return 1;
    }

    QUrl in_url(argv[1]);
    if (!in_url.isValid())
    {
        cerr << progname << ": invalid input URL: " << in_url.toString().toStdString()
             << in_url.errorString().toStdString() << endl;
        return 2;
    }

    QUrl out_url(argv[2]);
    if (!out_url.isValid())
    {
        cerr << progname << ": invalid output URL: " << out_url.toString().toStdString()
             << out_url.errorString().toStdString() << endl;
        return 2;
    }

    auto in_scheme = in_url.scheme();
    if (!in_scheme.isEmpty() && in_scheme != "file")
    {
        cerr << progname << ": invalid input URL: " << in_url.toString().toStdString()
             << " (invalid scheme name, requires \"file:\")" << endl;
        return 2;
    }

    auto out_scheme = out_url.scheme();
    if (!out_scheme.isEmpty() && out_scheme != "file" && out_scheme != "fd")
    {
        string s = out_url.toString().toStdString();
        cerr << progname << ": invalid output URL: " << out_url.toString().toStdString()
             << " (invalid scheme name, requires \"file:\" or \"fd:\")" << endl;
        return 2;
    }

    if (out_scheme.isEmpty() || out_scheme == "file")
    {
        // Output file name must end in .tiff.
        auto out_path = out_url.path();
        if (!out_path.endsWith(".tiff", Qt::CaseInsensitive))
        {
            cerr << progname << ": invalid output file name: " << out_path.toStdString()
                 << " (missing .tiff extension)" << endl;
            return 2;
        }
    }
    else
    {
        // For fd: scheme, path must parse as a number.
        bool ok;
        out_url.path().toInt(&ok);
        if (!ok)
        {
            cerr << progname << ": invalid URL: " << out_url.toString().toStdString()
                 << " (expected a number for file descriptor)" << endl;
            return 2;
        }
    }

    try
    {
        extract_thumbnail(in_url, out_url);
    }
    catch (exception const& e)
    {
        cerr << progname << ": Error creating thumbnail: " << e.what() << endl;
        return 2;
    }

    return 0;
}
