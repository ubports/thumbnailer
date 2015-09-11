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
 */

#include <cstdio>
#include <string>
#include <memory>
#include <stdexcept>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <gst/gst.h>
#include <gio/gio.h>
#include <glib.h>
#pragma GCC diagnostic pop

#include "thumbnailextractor.h"

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{

string command_line_arg_to_uri(string const& arg)
{
    unique_ptr<GFile, decltype(&g_object_unref)> file(g_file_new_for_commandline_arg(arg.c_str()), g_object_unref);
    if (!file)
    {
        throw runtime_error("Could not create parse argument as file");
    }
    char* c_uri = g_file_get_uri(file.get());
    if (!c_uri)
    {
        throw runtime_error("Could not convert to uri");
    }
    string uri(c_uri);
    g_free(c_uri);
    return uri;
}

}

int main(int argc, char** argv)
{
    gst_init(&argc, &argv);

    std::unique_ptr<GMainLoop, decltype(&g_main_loop_unref)> main_loop(
        g_main_loop_new(nullptr, false), g_main_loop_unref);
    bool success = true;
    ThumbnailExtractor extractor;
    for (int i = 1; i < argc; i++)
    {
        string uri;
        try
        {
            uri = command_line_arg_to_uri(argv[i]);
        }
        catch (exception const& e)
        {
            fprintf(stderr, "Error parsing \"%s\": %s\n", argv[i], e.what());
            return 1;
        }
        printf("Extracting from %s\n", uri.c_str());
        try
        {
            extractor.set_uri(uri);
            if (extractor.has_video())
            {
                success = extractor.extract_video_frame();
            }
            else
            {
                success = extractor.extract_audio_cover_art();
            }
        }
        catch (exception const& e)
        {
            fprintf(stderr, "Error extracting content \"%s\": %s\n", argv[1], e.what());
            return 1;
        }
        if (!success) break;
    }

    return success ? 0 : 1;
}
