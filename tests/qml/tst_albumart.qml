Fixture {
    name: "OnlineArt"

    function test_albumart() {
        loadAlbumArt("metallica", "load")
        compare(size.width, 48);
        compare(size.width, 48);
        comparePixel(0, 0, "#C80000");
        comparePixel(47, 0, "#00D200");
        comparePixel(0, 47, "#0000DC");
        comparePixel(47, 47, "#646E78");
    }

    function test_artistart() {
        loadArtistArt("beck", "odelay")
        compare(size.width, 640);
        compare(size.height, 480);
        comparePixel(0, 0, "#FE0000");
        comparePixel(639, 0, "#FFFF00");
        comparePixel(0, 479, "#0000FE");
        comparePixel(639, 479, "#00FF01");
    }

    function test_artistart_not_found() {
        loadArtistArt("beck2", "odelay")
        compare(size.width, 640);
        compare(size.height, 480);
        comparePixel(0, 0, "#FE0000");
        comparePixel(639, 0, "#FFFF00");
        comparePixel(0, 479, "#0000FE");
        comparePixel(639, 479, "#00FF01");
    }

    function test_albumart_not_found() {
        loadAlbumArt("beck2", "odelay")
        compare(size.width, 640);
        compare(size.height, 480);
        comparePixel(0, 0, "#FE0000");
        comparePixel(639, 0, "#FFFF00");
        comparePixel(0, 479, "#0000FE");
        comparePixel(639, 479, "#00FF01");
    }

    function test_bad_artist_art_url() {
        loadBadArtistUrl("load")
    }

    function test_bad_album_art_url() {
        loadBadAlbumUrl("metallica")
    }
}
