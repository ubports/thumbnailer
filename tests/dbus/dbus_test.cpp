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
 *              Michi Henning <michi.henning@canonical.com>
 */

#include <internal/image.h>
#include <internal/raii.h>
#include "utils/artserver.h"
#include "utils/dbusserver.h"

#include <boost/algorithm/string/predicate.hpp>
#include <gtest/gtest.h>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <testsetup.h>

#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

template <typename T>
void assert_no_error(const QDBusReply<T>& reply)
{
    QString message;
    if (!reply.isValid())
    {
        auto error = reply.error();
        message = error.name() + ": " + error.message();
    }
    ASSERT_TRUE(reply.isValid()) << message.toUtf8().constData();
}

class DBusTest : public ::testing::Test
{
protected:
    DBusTest()
    {
    }
    virtual ~DBusTest()
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
        setenv("THUMBNAILER_MAX_IDLE", "1000", true);

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

TEST_F(DBusTest, get_album_art)
{
    QDBusReply<QByteArray> reply =
        dbus_->thumbnailer_->GetAlbumArt("metallica", "load", QSize(24, 24));
    assert_no_error(reply);
    Image image(reply.value());
    EXPECT_EQ(24, image.width());
    EXPECT_EQ(24, image.width());
}

TEST_F(DBusTest, get_artist_art)
{
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetArtistArt("metallica", "load", QSize(24, 24));
        assert_no_error(reply);
        Image image(reply.value());
        EXPECT_EQ(24, image.width());
        EXPECT_EQ(24, image.width());
    }
}

TEST_F(DBusTest, thumbnail_image)
{
    const char* filename = TESTDATADIR "/testimage.jpg";
    QDBusReply<QByteArray> reply =
        dbus_->thumbnailer_->GetThumbnail(filename, QSize(256, 256));
    assert_no_error(reply);

    Image image(reply.value());
    EXPECT_EQ(256, image.width());
    EXPECT_EQ(160, image.height());
}

TEST_F(DBusTest, song_image)
{
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testsong.ogg";
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetThumbnail(filename, QSize(256, 256));
        assert_no_error(reply);

        Image image(reply.value());
        EXPECT_EQ(200, image.width());
        EXPECT_EQ(200, image.height());
    }
}

TEST_F(DBusTest, video_image)
{
    // We do this twice, so we get a cache hit on the second try.
    for (int i = 0; i < 2; ++i)
    {
        const char* filename = TESTDATADIR "/testvideo.ogg";
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetThumbnail(filename, QSize(256, 256));
        assert_no_error(reply);

        Image image(reply.value());
        EXPECT_EQ(256, image.width());
        EXPECT_EQ(144, image.height());
    }
}

TEST_F(DBusTest, thumbnail_no_such_file)
{
    const char* no_such_file = TESTDATADIR "/no-such-file.jpg";
    QDBusReply<QByteArray> reply =
        dbus_->thumbnailer_->GetThumbnail(no_such_file, QSize(256, 256));
    EXPECT_FALSE(reply.isValid());
    auto message = reply.error().message().toStdString();
    EXPECT_TRUE(boost::contains(message, " No such file or directory: ")) << message;
}

TEST_F(DBusTest, server_error)
{
    {
        QDBusReply<QDBusUnixFileDescriptor> reply =
            dbus_->thumbnailer_->GetArtistArt("error", "500", QSize(256, 256));
        EXPECT_FALSE(reply.isValid());
        auto message = reply.error().message().toStdString();
        EXPECT_EQ("Handler::createFinished(): could not get thumbnail for artist: error/500 (256,256): TEMPORARY ERROR",
                  message) << message;
    }

    // Again, so we cover the network retry limit case.
    {
        QDBusReply<QDBusUnixFileDescriptor> reply =
            dbus_->thumbnailer_->GetArtistArt("error", "500", QSize(256, 256));
        EXPECT_FALSE(reply.isValid());
        auto message = reply.error().message().toStdString();
        EXPECT_EQ("Handler::checkFinished(): no artwork for artist: error/500 (256,256): TEMPORARY ERROR",
                  message) << message;
    }
}

TEST_F(DBusTest, duplicate_requests)
{
    int const N_REQUESTS = 10;
    std::unique_ptr<QDBusPendingCallWatcher> watchers[N_REQUESTS];
    vector<int> results;

    for (int i = 0; i < N_REQUESTS; i++)
    {
        watchers[i].reset(
            new QDBusPendingCallWatcher(dbus_->thumbnailer_->GetAlbumArt(
                "metallica", "load", QSize(i*10, i*10))));
        QObject::connect(watchers[i].get(), &QDBusPendingCallWatcher::finished,
                         [i, &results]{ results.push_back(i); });
    }

    // The results should all be returned in order
    QSignalSpy spy(watchers[N_REQUESTS-1].get(), &QDBusPendingCallWatcher::finished);
    ASSERT_TRUE(spy.wait());

    for (int i = 0; i < N_REQUESTS; i++)
    {
        EXPECT_TRUE(watchers[i]->isFinished());
    }
    EXPECT_EQ(vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}), results);
}

TEST_F(DBusTest, rate_limit_requests)
{
    // This can't actually check that the requests are being properly
    // rate limited, but it does exercise the code paths as shown by
    // the coverage report.
    int const N_REQUESTS = 10;
    QDBusPendingReply<QByteArray> replies[N_REQUESTS];

    for (int i = 0; i < N_REQUESTS; i++)
    {
        replies[i] = dbus_->thumbnailer_->GetAlbumArt(
            "no such artist", QString::number(i), QSize(64, 64));
    }

    // Wait for all requests to complete.
    for (int i = 0; i < N_REQUESTS; i++)
    {
        replies[i].waitForFinished();
        EXPECT_FALSE(replies[i].isValid());
        string message = replies[i].error().message().toStdString();
        EXPECT_TRUE(boost::contains(message, "Handler::createFinished(): could not get thumbnail for ")) << message;
    }
}

TEST_F(DBusTest, test_inactivity_exit)
{
    // basic setup to the query
    const char* filename = TESTDATADIR "/testimage.jpg";

    QSignalSpy spy_exit(&dbus_->service_process(),
                        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished));

    // start a query
    QDBusReply<QByteArray> reply =
        dbus_->thumbnailer_->GetThumbnail(filename, QSize(256, 256));
    assert_no_error(reply);

    // wait for 5 seconds... (default)
    // Maximum inactivity should be less than that.
    spy_exit.wait();
    ASSERT_EQ(spy_exit.count(), 1);

    QList<QVariant> arguments = spy_exit.takeFirst();
    EXPECT_EQ(arguments.at(0).toInt(), 0);
}

TEST_F(DBusTest, service_exits_if_run_twice)
{
    // Try to start a second copy of the thumbnailer service
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(THUMBNAILER_SERVICE, QStringList());
    process.waitForFinished();
    EXPECT_EQ(QProcess::NormalExit, process.exitStatus());
    EXPECT_EQ(1, process.exitCode());
}

TEST_F(DBusTest, service_exits_if_name_taken)
{
    // Try to start a second copy of the thumbnailer service
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);

    // Force a different cache dir so we don't trigger the cache
    // locking exit.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_CACHE_HOME", tempdir->path() + "/cache2");
    process.setProcessEnvironment(env);

    process.start(THUMBNAILER_SERVICE, QStringList());
    process.waitForFinished();
    EXPECT_EQ(QProcess::NormalExit, process.exitStatus());
    EXPECT_EQ(1, process.exitCode());
}

TEST(DBusTestBadIdle, env_variable_bad_value)
{
    QTemporaryDir tempdir(TESTBINDIR "/dbus-test.XXXXXX");
    setenv("XDG_CACHE_HOME", (tempdir.path() + "/cache").toUtf8().data(), true);

    setenv("THUMBNAILER_MAX_IDLE", "bad_value", true);
    try
    {
        unique_ptr<DBusServer> dbus(new DBusServer());
        FAIL();
    }
    catch (std::logic_error const& e)
    {
        string err = e.what();
        EXPECT_TRUE(err.find("failed to appear on bus"));
    }
    unsetenv("THUMBNAILER_MAX_IDLE");
}

TEST(DBusTestBadIdle, env_variable_out_of_range)
{
    QTemporaryDir tempdir(TESTBINDIR "/dbus-test.XXXXXX");
    setenv("XDG_CACHE_HOME", (tempdir.path() + "/cache").toUtf8().data(), true);

    setenv("THUMBNAILER_MAX_IDLE", "999", true);
    try
    {
        unique_ptr<DBusServer> dbus(new DBusServer());
        FAIL();
    }
    catch (std::logic_error const& e)
    {
        string err = e.what();
        EXPECT_TRUE(err.find("failed to appear on bus"));
    }
    unsetenv("THUMBNAILER_MAX_IDLE");
}

TEST(DBusTestBadIdle, default_timeout)
{
    QTemporaryDir tempdir(TESTBINDIR "/dbus-test.XXXXXX");
    setenv("XDG_CACHE_HOME", (tempdir.path() + "/cache").toUtf8().data(), true);

    unsetenv("THUMBNAILER_MAX_IDLE");
    unique_ptr<DBusServer> dbus(new DBusServer());  // For coverage with default timeout.
}

bool near_current_time(chrono::system_clock::time_point& t)
{
    using namespace std::chrono;

    auto now_msecs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    auto t_msecs = duration_cast<milliseconds>(t.time_since_epoch()).count();
    if (abs(now_msecs - t_msecs) > 10000)
    {
        cerr << "Current time more than 10 seconds away from test time t" << endl;
        cerr << "Current time: " << now_msecs << endl;
        cerr << "Test time   : " << t_msecs << endl;
        return false;
    }
    return true;
}

TEST_F(DBusTest, bad_clear_or_compact_params)
{
    QDBusReply<void> reply = dbus_->admin_->ClearStats(-1);
    ASSERT_FALSE(reply.isValid());
    string msg = reply.error().message().toStdString();
    EXPECT_EQ("ClearStats(): invalid cache selector: -1", msg) << msg;

    reply = dbus_->admin_->ClearStats(4);
    ASSERT_FALSE(reply.isValid());
    msg = reply.error().message().toStdString();
    EXPECT_EQ("ClearStats(): invalid cache selector: 4", msg) << msg;

    reply = dbus_->admin_->Clear(-1);
    ASSERT_FALSE(reply.isValid());
    msg = reply.error().message().toStdString();
    EXPECT_EQ("Clear(): invalid cache selector: -1", msg) << msg;

    reply = dbus_->admin_->Clear(4);
    ASSERT_FALSE(reply.isValid());
    msg = reply.error().message().toStdString();
    EXPECT_EQ("Clear(): invalid cache selector: 4", msg) << msg;

    reply = dbus_->admin_->Compact(-1);
    ASSERT_FALSE(reply.isValid());
    msg = reply.error().message().toStdString();
    EXPECT_EQ("Compact(): invalid cache selector: -1", msg) << msg;

    reply = dbus_->admin_->Compact(4);
    ASSERT_FALSE(reply.isValid());
    msg = reply.error().message().toStdString();
    EXPECT_EQ("Compact(): invalid cache selector: 4", msg) << msg;
}

TEST_F(DBusTest, stats)
{
    using namespace std::chrono;
    using namespace unity::thumbnailer::service;

    QDBusReply<AllStats> reply = dbus_->admin_->Stats();
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    {
        CacheStats s = reply.value().full_size_stats;
        EXPECT_EQ(temp_dir() + "/cache/unity-thumbnailer/images", s.cache_path.toStdString());
        EXPECT_EQ(1, s.policy);
        EXPECT_EQ(0, s.size);
        EXPECT_EQ(0, s.size_in_bytes);
        EXPECT_NE(0, s.max_size_in_bytes);
        EXPECT_EQ(0, s.hits);
        EXPECT_EQ(0, s.misses);
        EXPECT_EQ(0, s.hits_since_last_miss);
        EXPECT_EQ(0, s.misses_since_last_hit);
        EXPECT_EQ(0, s.longest_hit_run);
        EXPECT_EQ(0, s.longest_miss_run);
        EXPECT_EQ(0.0, s.avg_hit_run_length);
        EXPECT_EQ(0.0, s.avg_miss_run_length);
        EXPECT_EQ(0, s.ttl_evictions);
        EXPECT_EQ(0, s.lru_evictions);
        EXPECT_EQ(0, duration_cast<milliseconds>(s.most_recent_hit_time.time_since_epoch()).count());
        EXPECT_EQ(0, duration_cast<milliseconds>(s.most_recent_miss_time.time_since_epoch()).count());
        EXPECT_EQ(0, duration_cast<milliseconds>(s.longest_hit_run_time.time_since_epoch()).count());
        EXPECT_EQ(0, duration_cast<milliseconds>(s.longest_miss_run_time.time_since_epoch()).count());
        auto list = s.histogram;
        for (auto c : list)
        {
            EXPECT_EQ(0, c);
        }
    }

    {
        CacheStats s = reply.value().thumbnail_stats;
        EXPECT_EQ(temp_dir() + "/cache/unity-thumbnailer/thumbnails", s.cache_path.toStdString());
        EXPECT_EQ(1, s.policy);
        EXPECT_EQ(0, s.size);
    }

    {
        CacheStats s = reply.value().failure_stats;
        EXPECT_EQ(temp_dir() + "/cache/unity-thumbnailer/failures", s.cache_path.toStdString());
        EXPECT_EQ(0, s.policy);
        EXPECT_EQ(0, s.size);
    }

    // Get a remote image from the cache, so the stats change.
    {
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetAlbumArt("metallica", "load", QSize(24, 24));
        assert_no_error(reply);
        Image image(reply.value());
        EXPECT_EQ(24, image.width());
        EXPECT_EQ(24, image.width());
    }

    reply = dbus_->admin_->Stats();
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    {
        CacheStats s = reply.value().full_size_stats;
        EXPECT_EQ(1, s.size);
        EXPECT_NE(0, s.size_in_bytes);
        EXPECT_EQ(0, s.hits);
        EXPECT_EQ(2, s.misses);
        EXPECT_EQ(0, s.hits_since_last_miss);
        EXPECT_EQ(2, s.misses_since_last_hit);
        EXPECT_EQ(0, s.longest_hit_run);
        EXPECT_EQ(2, s.longest_miss_run);
        EXPECT_EQ(0.0, s.avg_hit_run_length);
        EXPECT_EQ(2.0, s.avg_miss_run_length);
        EXPECT_EQ(0, s.ttl_evictions);
        EXPECT_EQ(0, s.lru_evictions);
        EXPECT_EQ(0, duration_cast<milliseconds>(s.most_recent_hit_time.time_since_epoch()).count());
        EXPECT_TRUE(near_current_time(s.most_recent_miss_time));
        EXPECT_EQ(0, duration_cast<milliseconds>(s.longest_hit_run_time.time_since_epoch()).count());
        EXPECT_TRUE(near_current_time(s.longest_miss_run_time));
        auto list = s.histogram;
        // There must be exactly one bin with value 1 now.
        int count = 0;
        for (auto c : list)
        {
            if (c != 0)
            {
                ++count;
            }
        }
        EXPECT_EQ(1, count);
    }

    {
        CacheStats s = reply.value().thumbnail_stats;
        EXPECT_EQ(1, s.size);
        EXPECT_NE(0, s.size_in_bytes);
        EXPECT_EQ(0, s.hits);
        EXPECT_EQ(2, s.misses);
        EXPECT_EQ(0, s.hits_since_last_miss);
        EXPECT_EQ(2, s.misses_since_last_hit);
        EXPECT_EQ(0, s.longest_hit_run);
        EXPECT_EQ(2, s.longest_miss_run);
        EXPECT_EQ(0.0, s.avg_hit_run_length);
        EXPECT_EQ(2.0, s.avg_miss_run_length);
        EXPECT_EQ(0, s.ttl_evictions);
        EXPECT_EQ(0, s.lru_evictions);
        EXPECT_EQ(0, duration_cast<milliseconds>(s.most_recent_hit_time.time_since_epoch()).count());
        EXPECT_TRUE(near_current_time(s.most_recent_miss_time));
        EXPECT_EQ(0, duration_cast<milliseconds>(s.longest_hit_run_time.time_since_epoch()).count());
        EXPECT_TRUE(near_current_time(s.longest_miss_run_time));
    }

    // Get the same image again, so we get a hit.
    {
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetAlbumArt("metallica", "load", QSize(24, 24));
        assert_no_error(reply);
        Image image(reply.value());
        EXPECT_EQ(24, image.width());
        EXPECT_EQ(24, image.width());
    }

    reply = dbus_->admin_->Stats();
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    {
        CacheStats s = reply.value().thumbnail_stats;
        EXPECT_EQ(1, s.size);
        EXPECT_NE(0, s.size_in_bytes);
        EXPECT_EQ(1, s.hits);
        EXPECT_EQ(2, s.misses);
        EXPECT_EQ(1, s.hits_since_last_miss);
        EXPECT_EQ(0, s.misses_since_last_hit);
        EXPECT_EQ(1, s.longest_hit_run);
        EXPECT_EQ(2, s.longest_miss_run);
        EXPECT_EQ(1.0, s.avg_hit_run_length);
        EXPECT_EQ(2.0, s.avg_miss_run_length);
        EXPECT_EQ(0, s.ttl_evictions);
        EXPECT_EQ(0, s.lru_evictions);
        EXPECT_TRUE(near_current_time(s.most_recent_hit_time));
        EXPECT_TRUE(near_current_time(s.most_recent_miss_time));
        EXPECT_TRUE(near_current_time(s.longest_hit_run_time));
        EXPECT_TRUE(near_current_time(s.longest_miss_run_time));
    }

    // Get a non-existent remote image from the cache, so the failure stats change.
    {
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetAlbumArt(
                "no_such_artist", "no_such_album", QSize(24, 24));
    }

    reply = dbus_->admin_->Stats();
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    {
        CacheStats s = reply.value().failure_stats;
        EXPECT_EQ(1, s.size);
        EXPECT_EQ(0, s.hits);
        EXPECT_EQ(4, s.misses);
    }

    // Get the same non-existent remote image again, so we get a hit.
    {
        QDBusReply<QByteArray> reply =
            dbus_->thumbnailer_->GetAlbumArt(
                "no_such_artist", "no_such_album", QSize(24, 24));
    }

    reply = dbus_->admin_->Stats();
    ASSERT_TRUE(reply.isValid()) << reply.error().message().toStdString();

    {
        CacheStats s = reply.value().failure_stats;
        EXPECT_EQ(1, s.size);
        EXPECT_EQ(1, s.hits);
        EXPECT_EQ(4, s.misses);
    }

    {
        // For coverage
        QDBusReply<void> reply = dbus_->admin_->Compact(0);
        EXPECT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    }

    {
        // For coverage
        QDBusReply<void> reply = dbus_->admin_->Shutdown();
        EXPECT_TRUE(reply.isValid()) << reply.error().message().toStdString();
    }
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
