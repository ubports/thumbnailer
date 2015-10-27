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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <unity/thumbnailer/qt/thumbnailer-qt.h>

#include <internal/file_io.h>
#include <utils/artserver.h>
#include <utils/dbusserver.h>
#include <utils/supports_decoder.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wparentheses-equality"
#endif
#include <gst/gst.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QColor>
#include <QImage>

#include <testsetup.h>

using namespace std;
using namespace unity::thumbnailer::qt;
using namespace unity::thumbnailer::internal;

namespace
{
    // Time to wait for an expected signal to arrive. The wait()
    // calls on the spy should always report success before this.
    int const SIGNAL_WAIT_TIME = 5000;
}

class LibThumbnailerTest : public ::testing::Test
{
protected:
    LibThumbnailerTest()
    {
    }
    virtual ~LibThumbnailerTest()
    {
    }

    virtual void SetUp() override
    {
        // start fake server
        art_server_.reset(new ArtServer());

        // start dbus service
        tempdir.reset(new QTemporaryDir(TESTBINDIR "/libthumbnailer-qt.XXXXXX"));
        setenv("XDG_CACHE_HOME", (tempdir->path() + "/cache").toUtf8().data(), true);

        // set 10 seconds as max idle time
        setenv("THUMBNAILER_MAX_IDLE", "10000", true);

        dbus_.reset(new DBusServer());
    }

    string temp_dir()
    {
        return tempdir->path().toStdString();
    }

    virtual void TearDown() override
    {
        dbus_.reset();
        art_server_.reset();

        unsetenv("THUMBNAILER_MAX_IDLE");
        unsetenv("XDG_CACHE_HOME");
        tempdir.reset();
    }

    unique_ptr<QTemporaryDir> tempdir;
    unique_ptr<DBusServer> dbus_;
    unique_ptr<ArtServer> art_server_;
};

TEST_F(LibThumbnailerTest, get_album_art)
{
    Thumbnailer thumbnailer(dbus_->connection());

    auto reply = thumbnailer.getAlbumArt("metallica", "load", QSize(48, 48));

    QSignalSpy spy(reply.data(), &Request::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), QString());

    QImage image = reply->image();
    EXPECT_EQ(48, image.width());
    EXPECT_EQ(48, image.height());

    EXPECT_EQ(image.pixel(0, 0), QColor("#C80000").rgb());
    EXPECT_EQ(image.pixel(47, 0), QColor("#00D200").rgb());
    EXPECT_EQ(image.pixel(0, 47), QColor("#0000DC").rgb());
    EXPECT_EQ(image.pixel(47, 47), QColor("#646E78").rgb());
}

TEST_F(LibThumbnailerTest, get_album_art_sync)
{
    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getAlbumArt("metallica", "load", QSize(48, 48));

    reply->waitForFinished();

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), QString());

    QImage image = reply->image();
    EXPECT_EQ(48, image.width());
    EXPECT_EQ(48, image.height());

    EXPECT_EQ(image.pixel(0, 0), QColor("#C80000").rgb());
    EXPECT_EQ(image.pixel(47, 0), QColor("#00D200").rgb());
    EXPECT_EQ(image.pixel(0, 47), QColor("#0000DC").rgb());
    EXPECT_EQ(image.pixel(47, 47), QColor("#646E78").rgb());
}


TEST_F(LibThumbnailerTest, get_artist_art)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        auto reply = thumbnailer.getArtistArt("metallica", "load", QSize(48, 48));
        QSignalSpy spy(reply.data(), &Request::finished);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
        // check that we've got exactly one signal
        ASSERT_EQ(spy.count(), 1);

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();
        EXPECT_EQ(48, image.width());
        EXPECT_EQ(48, image.height());

        EXPECT_EQ(image.pixel(0, 0), QColor("#C80000").rgb());
        EXPECT_EQ(image.pixel(47, 0), QColor("#00D200").rgb());
        EXPECT_EQ(image.pixel(0, 47), QColor("#0000DC").rgb());
        EXPECT_EQ(image.pixel(47, 47), QColor("#646E78").rgb());
    }
}

TEST_F(LibThumbnailerTest, get_artist_art_sync)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        auto reply = thumbnailer.getArtistArt("metallica", "load", QSize(48, 48));
        reply->waitForFinished();

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();
        EXPECT_EQ(48, image.width());
        EXPECT_EQ(48, image.height());

        EXPECT_EQ(image.pixel(0, 0), QColor("#C80000").rgb());
        EXPECT_EQ(image.pixel(47, 0), QColor("#00D200").rgb());
        EXPECT_EQ(image.pixel(0, 47), QColor("#0000DC").rgb());
        EXPECT_EQ(image.pixel(47, 47), QColor("#646E78").rgb());
    }
}

TEST_F(LibThumbnailerTest, thumbnail_image)
{
    const char* filename = TESTDATADIR "/orientation-1.jpg";

    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getThumbnail(filename, QSize(128, 96));
    QSignalSpy spy(reply.data(), &Request::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), QString());

    QImage image = reply->image();

    EXPECT_EQ(128, image.width());
    EXPECT_EQ(96, image.height());


    EXPECT_EQ(image.pixel(0, 0), QColor("#FE8081").rgb());
    EXPECT_EQ(image.pixel(127, 0), QColor("#FFFF80").rgb());
    EXPECT_EQ(image.pixel(0, 95), QColor("#807FFE").rgb());
    EXPECT_EQ(image.pixel(127, 95), QColor("#81FF81").rgb());
}

TEST_F(LibThumbnailerTest, chinese_filename)
{
    const char* filename = TESTDATADIR "/图片.JPG";

    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getThumbnail(filename, QSize(128, 96));
    QSignalSpy spy(reply.data(), &Request::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), QString());

    QImage image = reply->image();

    EXPECT_EQ(128, image.width());
    EXPECT_EQ(96, image.height());


    EXPECT_EQ(image.pixel(0, 0), QColor("#FE8081").rgb());
    EXPECT_EQ(image.pixel(127, 0), QColor("#FFFF80").rgb());
    EXPECT_EQ(image.pixel(0, 95), QColor("#807FFE").rgb());
    EXPECT_EQ(image.pixel(127, 95), QColor("#81FF81").rgb());
}

TEST_F(LibThumbnailerTest, thumbnail_image_sync)
{
    const char* filename = TESTDATADIR "/orientation-1.jpg";

    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getThumbnail(filename, QSize(128, 96));
    reply->waitForFinished();

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), QString());

    QImage image = reply->image();

    EXPECT_EQ(128, image.width());
    EXPECT_EQ(96, image.height());


    EXPECT_EQ(image.pixel(0, 0), QColor("#FE8081").rgb());
    EXPECT_EQ(image.pixel(127, 0), QColor("#FFFF80").rgb());
    EXPECT_EQ(image.pixel(0, 95), QColor("#807FFE").rgb());
    EXPECT_EQ(image.pixel(127, 95), QColor("#81FF81").rgb());
}

TEST_F(LibThumbnailerTest, song_image)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testsong.ogg";
        auto reply = thumbnailer.getThumbnail(filename, QSize(256, 256));
        QSignalSpy spy(reply.data(), &Request::finished);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        ASSERT_EQ(spy.count(), 1);

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();

        EXPECT_EQ(200, image.width());
        EXPECT_EQ(200, image.height());

        EXPECT_EQ(image.pixel(0, 0), QColor("#FFFFFF").rgb());
        EXPECT_EQ(image.pixel(199, 199), QColor("#FFFFFF").rgb());

    }
}

TEST_F(LibThumbnailerTest, song_image_sync)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testsong.ogg";
        auto reply = thumbnailer.getThumbnail(filename, QSize(256, 256));
        reply->waitForFinished();

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();

        EXPECT_EQ(200, image.width());
        EXPECT_EQ(200, image.height());

        EXPECT_EQ(image.pixel(0, 0), QColor("#FFFFFF").rgb());
        EXPECT_EQ(image.pixel(199, 199), QColor("#FFFFFF").rgb());

    }
}

TEST_F(LibThumbnailerTest, video_image)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testvideo.ogg";

        auto reply = thumbnailer.getThumbnail(filename, QSize(256, 256));
        QSignalSpy spy(reply.data(), &Request::finished);
        ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

        ASSERT_EQ(spy.count(), 1);

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();
        EXPECT_EQ(256, image.width());
        EXPECT_EQ(144, image.height());
    }
}

TEST_F(LibThumbnailerTest, video_image_sync)
{
    Thumbnailer thumbnailer(dbus_->connection());
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testvideo.ogg";

        auto reply = thumbnailer.getThumbnail(filename, QSize(256, 256));
        reply->waitForFinished();

        EXPECT_TRUE(reply->isFinished());
        EXPECT_TRUE(reply->isValid());
        EXPECT_EQ(reply->errorMessage(), QString());

        QImage image = reply->image();
        EXPECT_EQ(256, image.width());
        EXPECT_EQ(144, image.height());
    }
}

TEST_F(LibThumbnailerTest, thumbnail_no_such_file)
{
    const char* no_such_file = TESTDATADIR "/no-such-file.jpg";

    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getThumbnail(no_such_file, QSize(256, 256));

    QSignalSpy spy(reply.data(), &Request::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(boost::contains(reply->errorMessage(), " No such file or directory: "));
    EXPECT_FALSE(reply->isValid());
}


TEST_F(LibThumbnailerTest, thumbnail_no_such_file_sync)
{
    const char* no_such_file = TESTDATADIR "/no-such-file.jpg";

    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getThumbnail(no_such_file, QSize(256, 256));

    reply->waitForFinished();

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(boost::contains(reply->errorMessage(), " No such file or directory: "));
    EXPECT_FALSE(reply->isValid());
}

TEST_F(LibThumbnailerTest, server_error)
{
    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getArtistArt("error", "500", QSize(256, 256));

    QSignalSpy spy(reply.data(), &Request::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));

    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(boost::contains(reply->errorMessage(), "fetch() failed"));
    EXPECT_FALSE(reply->isValid());
}

TEST_F(LibThumbnailerTest, server_error_sync)
{
    Thumbnailer thumbnailer(dbus_->connection());
    auto reply = thumbnailer.getArtistArt("error", "500", QSize(256, 256));

    reply->waitForFinished();

    EXPECT_TRUE(reply->isFinished());
    EXPECT_TRUE(boost::contains(reply->errorMessage(), "fetch() failed"));
    EXPECT_FALSE(reply->isValid());
}

void make_links(string const& source_path, string const& target_dir, int num_copies)
{
    using namespace boost::filesystem;

    assert(num_copies > 0);
    assert(num_copies <= 2000);  // Probably not good to put more files than this into one directory

    string filename = path(source_path).filename().native();
    string new_name = "0" + filename;

    // Make initial copy
    string copied_file = target_dir + "/" + new_name;
    string contents = read_file(source_path);
    write_file(copied_file, contents);

    // Make num_copies - 1 links to the copy.
    for (int i = 1; i < num_copies; ++i)
    {
        string link_name = target_dir + "/" + to_string(i) + filename;
        EXPECT_TRUE(link(copied_file.c_str(), link_name.c_str()) == 0 || errno == EEXIST) << "errno = " << errno;
    }
}

// Simple counter class that we use to count all the signals
// from the async provider instances. This allows us to use
// a single QSignalSpy to wait for the completion of an
// arbitrary number of requests.

class Counter : public QObject
{
    Q_OBJECT
public:
    Counter(int limit)
        : limit_(limit)
        , count_(0)
        , cancellations_(0)
    {
    }

    int cancellations()
    {
        return cancellations_;
    }

Q_SIGNALS:
    void counterDone();

public Q_SLOTS:
    void thumbnailComplete(bool cancelled)
    {
        if (cancelled)
        {
            ++cancellations_;
        }
        if (++count_ == limit_)
        {
            Q_EMIT counterDone();
        }
    }

private:
    int limit_;
    int count_;
    int cancellations_;
};

// Class to run a single request and check that it completed OK.
// Notifies the counter on request completion.

class AsyncThumbnailProvider : public QObject
{
    Q_OBJECT
public:
    AsyncThumbnailProvider(unity::thumbnailer::qt::Thumbnailer* tn, Counter& counter)
        : thumbnailer_(tn)
        , counter_(counter)
    {
        assert(tn);
    }

    void getThumbnail(QString const& path, QSize const& size)
    {
        request_ = thumbnailer_->getThumbnail(path, size);
        connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
                this, &AsyncThumbnailProvider::requestFinished);
    }

    void getAlbumArt(QString const& artist, QString const& album, QSize const& size)
    {
        request_ = thumbnailer_->getAlbumArt(artist, album, size);
        connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
                this, &AsyncThumbnailProvider::requestFinished);
    }

    void cancel()
    {
        request_->cancel();
    }

    void waitForFinished()
    {
        request_->waitForFinished();
    }

public Q_SLOTS:
    void requestFinished()
    {
    cerr << "request finished" << endl;
        EXPECT_TRUE(request_->isFinished());
        if (!request_->isValid())
        {
            EXPECT_TRUE(request_->isCancelled());
            EXPECT_TRUE(request_->image().isNull());
            EXPECT_TRUE(request_->errorMessage() == QString("Request cancelled"))
                << request_->errorMessage().toStdString();
        }
        counter_.thumbnailComplete(request_->isCancelled());
    }

private:
    unity::thumbnailer::qt::Thumbnailer* thumbnailer_;
    Counter& counter_;
    QSharedPointer<unity::thumbnailer::qt::Request> request_;
};

void pump(int millisecs)
{
    QTimer timer;
    QSignalSpy timer_spy(&timer, &QTimer::timeout);
    timer.start(millisecs);
    EXPECT_TRUE(timer_spy.wait(millisecs + 1000));
}

TEST_F(LibThumbnailerTest, cancel)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for MP3 decoder\n");
        return;
    }

    Thumbnailer thumbnailer(dbus_->connection());

    string source = "short-track.mp3";
    string target_dir = temp_dir();
    make_links(string(TESTDATADIR) + "/" + source, target_dir, 4);

    vector<unique_ptr<AsyncThumbnailProvider>> providers;
    QString path;

    Counter counter(2);
    QSignalSpy spy(&counter, &Counter::counterDone);

    unique_ptr<AsyncThumbnailProvider> provider(new AsyncThumbnailProvider(&thumbnailer, counter));
    path = QString::fromStdString(target_dir + "/0" + source);
    provider->getThumbnail(path, QSize(512, 512));
    providers.emplace_back(move(provider));

    pump(1000);

    providers[0]->cancel();

    pump(500);

    provider.reset(new AsyncThumbnailProvider(&thumbnailer, counter));
    provider->getThumbnail(path, QSize(512, 512));

    EXPECT_TRUE(spy.wait(1000));
    EXPECT_EQ(1, spy.count());
}

Q_DECLARE_METATYPE(QProcess::ExitStatus)  // Avoid noise from signal spy.

int main(int argc, char** argv)
{
    gst_init(&argc, &argv);

    QCoreApplication app(argc, argv);
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");  // Avoid noise from signal spy.
    qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include "libthumbnailer-qt_test.moc"
