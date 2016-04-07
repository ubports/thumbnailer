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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include "thumbnailextractor.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <gst/gst.h>
#include <gio/gio.h>
#include <glib.h>
#pragma GCC diagnostic pop

#include <QUrl>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace std;
using namespace unity::thumbnailer::internal;

int main(int argc, char** argv)
{
    gst_init(&argc, &argv);

    std::unique_ptr<GMainLoop, decltype(&g_main_loop_unref)> main_loop(
        g_main_loop_new(nullptr, false), g_main_loop_unref);
    bool success = true;
    ThumbnailExtractor extractor;
    for (int i = 1; i < argc; i++)
    {
        QUrl url(argv[i]);
        if (!url.isValid())
        {
            cerr << "Error parsing " << url.toString().toStdString() << ": " << url.errorString().toStdString() << endl;
            return 1;
        }
        cout << "Extracting from " << url.toString().toStdString() << endl;
        try
        {
            extractor.set_urls(url, QUrl("/dev/null"));
            if (extractor.has_video())
            {
                success = extractor.extract_video_frame();
            }
            else
            {
                success = extractor.extract_cover_art();
            }
        }
        catch (exception const& e)
        {
            cerr << "Error Extracting content \"" << argv[i] << "\": " << e.what() << endl;
            return 1;
        }
        if (!success) break;
    }

    return success ? 0 : 1;
}
