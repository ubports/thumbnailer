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

#pragma once

#include <memory>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class ThumbnailExtractor final
{
    struct Private;

public:
    ThumbnailExtractor();
    ~ThumbnailExtractor();

    void reset();
    void set_uri(const std::string& uri);
    bool has_video();
    bool extract_video_frame();
    bool extract_audio_cover_art();
    void save_screenshot(const std::string& filename);

private:
    std::unique_ptr<Private> p;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
