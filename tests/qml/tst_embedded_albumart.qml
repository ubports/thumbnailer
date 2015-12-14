Fixture {
    name: "EmbeddedArt"

    function test_embedded_art() {
        loadThumbnail("testsong.ogg");
        compare(size.width, 200);
        compare(size.height, 200);
        comparePixel(0, 0, "#FFFFFF");
    }
}
