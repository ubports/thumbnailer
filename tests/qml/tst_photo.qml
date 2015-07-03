Fixture {
    name: "Photo"

    function test_photo() {
        loadThumbnail("orientation-1.jpg");
        compare(size.width, 256); // was 640
        compare(size.height, 192); // was 480
        comparePixel(0, 0, "#FE0000");
        comparePixel(255, 0, "#FFFF00");
        comparePixel(0, 191, "#0000FE");
        comparePixel(255, 191, "#00FF01");
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
        compare(size.width, 256); // was 640
        compare(size.height, 192); // was 480
        comparePixel(0, 0, "#FE0000");
        comparePixel(255, 0, "#FFFF00");
        comparePixel(0, 191, "#0000FE");
        comparePixel(255, 191, "#00FF01");
    }

    function test_rotation_scaled() {
        requestedSize = Qt.size(320, 240);
        loadThumbnail("orientation-8.jpg");
        compare(size.width, 320);
        compare(size.height, 240);
        /*
        comparePixel(0, 0, "#FE0000");
        comparePixel(319, 0, "#FFFF00");
        comparePixel(0, 239, "#0000FE");
        comparePixel(319, 239, "#00FF01");
        */
    }

    function test_no_such_photo() {
        loadThumbnail("no_such_photo.jpg");
    }
}
