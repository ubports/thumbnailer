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

#include <utils/artserver.h>
#include <utils/dbusserver.h>

#include <boost/algorithm/string/predicate.hpp>
#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QColor>
#include <QImage>

#include <testsetup.h>

using namespace std;
using namespace unity::thumbnailer::qt;

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
        tempdir.reset(new QTemporaryDir(TESTBINDIR "/dbus-test.XXXXXX"));
        setenv("XDG_CACHE_HOME", (tempdir->path() + "/cache").toUtf8().data(), true);

        // set 3 seconds as max idle time
        setenv("THUMBNAILER_MAX_IDLE", "3000", true);

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

TEST_F(LibThumbnailerTest, cancel_request)
{
    Thumbnailer thumbnailer(dbus_->connection());

    auto reply = thumbnailer.getAlbumArt("metallica", "load", QSize(48, 48));

    QSignalSpy spy(reply.data(), &Request::finished);
    reply->cancel();

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_TRUE(reply->isFinished());
    EXPECT_FALSE(reply->isValid());
    EXPECT_EQ(reply->errorMessage(), "Request cancelled");
}

Q_DECLARE_METATYPE(QProcess::ExitStatus)  // Avoid noise from signal spy.

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");  // Avoid noise from signal spy.
    qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
