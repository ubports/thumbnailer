Fixture {
    name: "EmbeddedArt"

    function test_embedded_art() {
        loadThumbnail("testsong.ogg");
        compare(size.width, 128); // was 200
        compare(size.height, 128); // was 200
        comparePixel(0, 0, "#FFFFFF");
    }
}
