import QtQuick 2.0
import QtTest 1.0
import Ubuntu.Thumbnailer 0.1
import testconfig 1.0

TestCase {
    id: root
    when: windowShown

    width: canvas.width > 0 ? canvas.width : 100
    height: canvas.height > 0 ? canvas.height : 100

    property size size: Qt.size(image.implicitWidth, image.implicitHeight)
    property size requestedSize: Qt.size(-1, -1)

    Image {
        id: image
        width: parent.width
        height: parent.height
        sourceSize: root.requestedSize

        SignalSpy {
            id: spy
            target: image
            signalName: "statusChanged"
        }
    }

    Canvas {
        id: canvas
        width: image.implicitWidth
        height: image.implicitHeight
        contextType: "2d"
        renderStrategy: Canvas.Immediate
        renderTarget: Canvas.Image

        property bool paintFinished: false
        onPaint: {
            context.drawImage(image, 0, 0);
            paintFinished = true
        }
    }

    function loadThumbnail(filename) {
        var url = Qt.resolvedUrl(Config.mediaDir + "/" + filename);
        load("image://thumbnailer/" + url);
    }

    function loadAlbumArt(artist, album) {
        load("image://albumart/artist=" +
                   encodeURIComponent(artist) +
                   "&album=" +
                   encodeURIComponent(album));
    }

    function loadArtistArt(artist, album) {
        load("image://artistart/artist=" +
                   encodeURIComponent(artist) +
                   "&album=" +
                   encodeURIComponent(album));
    }

    function loadBadAlbumUrl(artist) {
        load("image://albumart/artist=" +
                   encodeURIComponent(artist));
    }

    function loadBadArtistUrl(album) {
        load("image://artistart/album=" +
                   encodeURIComponent(album));
    }

    function load(url) {
        image.source = url;
        while (image.status === Image.Loading) {
            spy.wait();
        }
        compare(image.status, Image.Ready);

        canvas.paintFinished = false;
        canvas.requestPaint();
        // We should be able to do this by waiting on the "painted"
        // signal, but I couldn't get that to work reliably.
        while (!canvas.paintFinished) {
            wait(50);
        }
    }

    function pixel(x, y) {
        var data = canvas.context.getImageData(x,y,1,1).data;
        return Qt.rgba(data[0] / 255, data[1] / 255, data[2] / 255, data[3] / 255);
    }

    function comparePixel(x, y, expected) {
        var actual = pixel(x, y);
        if (!Qt.colorEqual(actual, expected)) {
            fail("Unexpected pixel value at (" + x + ", " + y + ")\n" +
                 "actual:   " + actual + "\nexpected: " + expected);
        }
    }

    function cleanup() {
        image.source = "";
        root.requestedSize = Qt.size(-1, -1);
        while (image.status !== Image.Null) {
            spy.wait();
        }
    }
}
