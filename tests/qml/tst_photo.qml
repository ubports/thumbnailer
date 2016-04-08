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

Fixture {
    name: "Photo"

    function test_photo() {
        loadThumbnail("orientation-1.jpg");
        compare(size.width, 640);
        compare(size.height, 480);
        comparePixel(0, 0, "#FE0000");
        comparePixel(639, 0, "#FFFF00");
        comparePixel(0, 479, "#0000FE");
        comparePixel(639, 479, "#00FF01");
    }

    function test_scaled() {
        requestedSize = Qt.size(256, 256);
        loadThumbnail("orientation-1.jpg");
        compare(size.width, 256);
        compare(size.height, 192);
    }

    function test_scale_horizontal() {
        requestedSize = Qt.size(320, 0);
        loadThumbnail("orientation-1.jpg");
        compare(size.width, 320);
        compare(size.height, 240);
    }

    function test_scale_vertical() {
        requestedSize = Qt.size(0, 240);
        loadThumbnail("orientation-1.jpg");
        compare(size.width, 320);
        compare(size.height, 240);
    }

    function test_rotation() {
        loadThumbnail("orientation-8.jpg");
        compare(size.width, 640);
        compare(size.height, 480);
        comparePixel(0, 0, "#FE0000");
        comparePixel(639, 0, "#FFFF00");
        comparePixel(0, 479, "#0000FE");
        comparePixel(639, 479, "#00FF01");
    }

    function test_rotation_scaled() {
        requestedSize = Qt.size(320, 240);
        loadThumbnail("orientation-8.jpg");
        compare(size.width, 320);
        compare(size.height, 240);
        comparePixel(0, 0, "#FE0000");
        comparePixel(319, 0, "#FFFF00");
        comparePixel(0, 239, "#0000FE");
        comparePixel(319, 239, "#00FF01");
    }

    function test_no_such_photo() {
        expectLoadError("image://thumbnailer/file:///no_such_photo.jpg");
    }
}
