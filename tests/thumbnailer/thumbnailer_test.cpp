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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <internal/thumbnailer.h>

#include <internal/env_vars.h>
#include <internal/file_io.h>
#include <internal/image.h>
#include <internal/raii.h>
#include <internal/trace.h>
#include <testsetup.h>
#include "utils/artserver.h"
#include "utils/env_var_guard.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gio/gio.h>
#pragma GCC diagnostic pop
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <unity/UnityExceptions.h>

#include <sys/types.h>
#include <fcntl.h>

#define TEST_IMAGE TESTDATADIR "/orientation-1.jpg"
#define BAD_IMAGE TESTDATADIR "/bad_image.jpg"
#define RGB_IMAGE TESTDATADIR "/RGB.png"
#define BIG_IMAGE TESTDATADIR "/big.jpg"
#define SMALL_GIF TESTDATADIR "/small.gif"
#define LARGE_GIF TESTDATADIR "/large.gif"
#define EMPTY_IMAGE TESTDATADIR "/empty"

#define TEST_VIDEO TESTDATADIR "/testvideo.ogg"
#define TEST_SONG TESTDATADIR "/testsong.ogg"

using namespace std;
using namespace unity::thumbnailer::internal;

// The thumbnailer uses g_get_user_cache_dir() to get the cache dir, and
// glib remembers that value, so changing XDG_CACHE_HOME later has no effect.

static auto set_tempdir = []()
{
    auto dir = new QTemporaryDir(TESTBINDIR "/test-dir.XXXXXX");
    setenv("XDG_CACHE_HOME", dir->path().toUtf8().data(), true);
    return dir;
};
static unique_ptr<QTemporaryDir> tempdir(set_tempdir());

class ThumbnailerTest : public ::testing::Test
{
public:
    static string tempdir_path()
    {
        return tempdir->path().toStdString();
    }

protected:
    virtual void SetUp() override
    {
        mkdir(tempdir_path().c_str(), 0700);
    }

    virtual void TearDown() override
    {
        boost::filesystem::remove_all(tempdir_path());
    }
};

TEST_F(ThumbnailerTest, basic)
{
    Thumbnailer tn;
    std::unique_ptr<ThumbnailRequest> request;
    QByteArray thumb;
    Image img;

    auto old_stats = tn.stats();
    request = tn.get_thumbnail(EMPTY_IMAGE, QSize(10, 10));
    thumb = request->thumbnail();
    EXPECT_EQ("", thumb);
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());

    // Again, this time we get the answer from the failure cache.
    old_stats = tn.stats();
    request = tn.get_thumbnail(EMPTY_IMAGE, QSize(10, 10));
    thumb = request->thumbnail();
    EXPECT_EQ("", thumb);
    new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.hits() + 1, new_stats.failure_stats.hits());

    request = tn.get_thumbnail(TEST_IMAGE, QSize(640, 640));
    EXPECT_TRUE(boost::starts_with(request->key(), TEST_IMAGE)) << request->key();
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    // Again, for coverage. This time the thumbnail comes from the cache.
    old_stats = tn.stats();
    request = tn.get_thumbnail(TEST_IMAGE, QSize(640, 640));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());
    new_stats = tn.stats();
    EXPECT_EQ(old_stats.thumbnail_stats.hits() + 1, new_stats.thumbnail_stats.hits());

    request = tn.get_thumbnail(TEST_IMAGE, QSize(160, 160));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(160, img.width());
    EXPECT_EQ(120, img.height());

    request = tn.get_thumbnail(TEST_IMAGE, QSize(1000, 1000));  // Will not up-scale
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    request = tn.get_thumbnail(TEST_IMAGE, QSize(100, 100));  // From EXIF data
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(100, img.width());
    EXPECT_EQ(75, img.height());

    request = tn.get_thumbnail(RGB_IMAGE, QSize(48, 48));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(48, img.width());
    EXPECT_EQ(48, img.height());

    request = tn.get_thumbnail(BIG_IMAGE, QSize(5000, 5000));  // > 1920, so will be trimmed down
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(1920, img.width());
    EXPECT_EQ(1439, img.height());

    request = tn.get_thumbnail(BIG_IMAGE, QSize(0, 0));  // Will be trimmed down
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(1920, img.width());
    EXPECT_EQ(1439, img.height());

    request = tn.get_thumbnail(SMALL_GIF, QSize(0, 0));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    request = tn.get_thumbnail(LARGE_GIF, QSize(0, 0));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(1536, img.width());
    EXPECT_EQ(1152, img.height());
}

TEST_F(ThumbnailerTest, changed_size)
{
    {
        Thumbnailer tn;
        EXPECT_EQ(100 * 1024 * 1024, tn.stats().thumbnail_stats.max_size_in_bytes());
    }

    {
        gobj_ptr<GSettings> gsettings(g_settings_new("com.canonical.Unity.Thumbnailer"));
        g_settings_set_int(gsettings.get(), "thumbnail-cache-size", 1);
        Thumbnailer tn;
        EXPECT_EQ(1024 * 1024, tn.stats().thumbnail_stats.max_size_in_bytes());
    }
}

TEST_F(ThumbnailerTest, compact)
{
    Thumbnailer tn;

    // For coverage.
    tn.compact(Thumbnailer::CacheSelector::all);
}

TEST_F(ThumbnailerTest, clear)
{
    Thumbnailer tn;

    auto fill_cache = [&tn]
    {
        {
            // Load a song so we have something in the thumbnail cache.
            auto request = tn.get_thumbnail(TEST_SONG, QSize(200, 200));
            ASSERT_NE(nullptr, request.get());
            Image img(request->thumbnail());
            EXPECT_EQ(200, img.width());
            EXPECT_EQ(200, img.height());
        }

        {
            // Load same song again at different size.
            auto request = tn.get_thumbnail(TEST_SONG, QSize(20, 20));
            ASSERT_NE(nullptr, request.get());
            ASSERT_NE("", request->thumbnail());
        }

        {
            // Load same song again at same size, so we get a hit on thumbnail cache.
            auto request = tn.get_thumbnail(TEST_SONG, QSize(20, 20));
            ASSERT_NE(nullptr, request.get());
            ASSERT_NE("", request->thumbnail());
        }

        {
            // Load an empty image, so we have something in the failure cache.
            auto request = tn.get_thumbnail(EMPTY_IMAGE, QSize(10, 10));
            EXPECT_EQ("", request->thumbnail());
        }

        {
            // Load empty image again, so we get a hit on failure cache.
            auto request = tn.get_thumbnail(EMPTY_IMAGE, QSize(10, 10));
            EXPECT_EQ("", request->thumbnail());
        }

        // Thumbnail a video so we get something into the full-size cache.
        {
            auto request = tn.get_thumbnail(TEST_VIDEO, QSize(1920, 1920));
            ASSERT_NE(nullptr, request.get());
            // Video thumbnails cannot be produced immediately
            ASSERT_EQ("", request->thumbnail());

            {
                QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
                request->download(chrono::milliseconds(15000));
                ASSERT_TRUE(spy.wait(20000));
            }

            request = tn.get_thumbnail(TEST_VIDEO, QSize(100, 100));
            ASSERT_NE(nullptr, request.get());

            {
                QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
                request->download(chrono::milliseconds(15000));
                ASSERT_TRUE(spy.wait(20000));
            }

            auto thumb = request->thumbnail();
            Image img(thumb);
            EXPECT_EQ(100, img.width());
            EXPECT_EQ(56, img.height());
        }
    };

    fill_cache();

    // Just to show that fill_cache() does put things into the cache and the stats are as expected.
    auto stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(3, stats.thumbnail_stats.size());
    EXPECT_EQ(1, stats.failure_stats.size());
    EXPECT_EQ(0, stats.full_size_stats.hits());
    EXPECT_EQ(1, stats.thumbnail_stats.hits());
    EXPECT_EQ(1, stats.failure_stats.hits());

    // Clear all caches and check that they are empty.
    tn.clear(Thumbnailer::CacheSelector::all);
    stats = tn.stats();
    EXPECT_EQ(0, stats.full_size_stats.size());
    EXPECT_EQ(0, stats.thumbnail_stats.size());
    EXPECT_EQ(0, stats.failure_stats.size());

    // Clear full-size cache only.
    fill_cache();
    tn.clear(Thumbnailer::CacheSelector::full_size_cache);
    stats = tn.stats();
    EXPECT_EQ(0, stats.full_size_stats.size());
    EXPECT_EQ(3, stats.thumbnail_stats.size());
    EXPECT_EQ(1, stats.failure_stats.size());

    // Clear thumbnail cache only.
    tn.clear(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear(Thumbnailer::CacheSelector::thumbnail_cache);
    stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(0, stats.thumbnail_stats.size());
    EXPECT_EQ(1, stats.failure_stats.size());

    // Clear failure cache only.
    tn.clear(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear(Thumbnailer::CacheSelector::failure_cache);
    stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(3, stats.thumbnail_stats.size());
    EXPECT_EQ(0, stats.failure_stats.size());

    // Clear all stats.
    tn.clear_stats(Thumbnailer::CacheSelector::all);
    stats = tn.stats();
    EXPECT_EQ(0, stats.full_size_stats.hits());
    EXPECT_EQ(0, stats.thumbnail_stats.hits());
    EXPECT_EQ(0, stats.failure_stats.hits());

    // Re-fill the cache and clear full-size stats only.
    tn.clear(Thumbnailer::CacheSelector::all);
    tn.clear_stats(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear_stats(Thumbnailer::CacheSelector::full_size_cache);
    stats = tn.stats();
    EXPECT_EQ(0, stats.full_size_stats.hits());
    EXPECT_EQ(1, stats.thumbnail_stats.hits());
    EXPECT_EQ(1, stats.failure_stats.hits());

    // Re-fill the cache and clear thumbnail stats only.
    tn.clear(Thumbnailer::CacheSelector::all);
    tn.clear_stats(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear_stats(Thumbnailer::CacheSelector::thumbnail_cache);
    stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(0, stats.thumbnail_stats.hits());
    EXPECT_EQ(1, stats.failure_stats.hits());

    // Re-fill the cache and clear failure stats only.
    tn.clear(Thumbnailer::CacheSelector::all);
    tn.clear_stats(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear_stats(Thumbnailer::CacheSelector::failure_cache);
    stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(1, stats.thumbnail_stats.hits());
    EXPECT_EQ(0, stats.failure_stats.hits());
}

TEST_F(ThumbnailerTest, thumbnail_video)
{
    Thumbnailer tn;
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(1920, 1920));
    ASSERT_NE(nullptr, request.get());
    // Video thumbnails cannot be produced immediately
    ASSERT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download(chrono::milliseconds(15000));
    ASSERT_TRUE(spy.wait(20000));
    {
        auto old_stats = tn.stats();
        QByteArray thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(1920, img.width());
        EXPECT_EQ(1080, img.height());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.full_size_stats.size() + 1, new_stats.full_size_stats.size());
    }

    {
        // Fetch the thumbnail again with the same size.
        // That causes it to come from the thumbnail cache.
        auto old_stats = tn.stats();
        auto request = tn.get_thumbnail(TEST_VIDEO, QSize(1920, 1920));
        QByteArray thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(1920, img.width());
        EXPECT_EQ(1080, img.height());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.thumbnail_stats.hits() + 1, new_stats.thumbnail_stats.hits());
    }

    {
        // Fetch the thumbnail again with a different size.
        // That causes it to be scaled from the full-size cache.
        auto old_stats = tn.stats();
        auto request = tn.get_thumbnail(TEST_VIDEO, QSize(500, 500));
        QByteArray thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(500, img.width());
        EXPECT_EQ(281, img.height());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.full_size_stats.hits() + 1, new_stats.full_size_stats.hits());
    }
}

TEST_F(ThumbnailerTest, thumbnail_song)
{
    Thumbnailer tn;
    auto request = tn.get_thumbnail(TEST_SONG, QSize(400, 400));
    ASSERT_NE(nullptr, request.get());
    QByteArray thumb = request->thumbnail();
    ASSERT_NE("", thumb);
    Image img(thumb);
    EXPECT_EQ(200, img.width());
    EXPECT_EQ(200, img.height());
}

TEST_F(ThumbnailerTest, exceptions)
{
    string const cache_dir = tempdir_path();
    ASSERT_EQ(0, chmod(cache_dir.c_str(), 0000));
    try
    {
        Thumbnailer tn;
        FAIL();
    }
    catch (runtime_error const& e)
    {
        ASSERT_EQ(0, chmod(cache_dir.c_str(), 0700));
        string msg = e.what();
        string exp = "Thumbnailer(): Cannot instantiate cache: PersistentStringCache: cannot open or create cache: ";
        EXPECT_EQ(0, msg.compare(0, exp.length(), exp)) << msg;
    }
    ASSERT_EQ(0, chmod(cache_dir.c_str(), 0700));

    try
    {
        Thumbnailer tn;
        tn.get_thumbnail("", QSize(0, 0));
        FAIL();
    }
    catch (unity::InvalidArgumentException const& e)
    {
        EXPECT_STREQ("unity::InvalidArgumentException: Thumbnailer::get_thumbnail(): filename is empty", e.what());
    }
}

TEST_F(ThumbnailerTest, vs_thumb_exec_failure)
{
    // Cause vs-thumb exec failure.
    EnvVarGuard ev_guard(UTIL_DIR, "no_such_directory");

    Thumbnailer tn;

    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));

    auto old_stats = tn.stats();
    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, vs_thumb_exit_1)
{
    // Run fake vs-thumb that exits with status 1
    EnvVarGuard ev_guard(UTIL_DIR, TESTSRCDIR "/thumbnailer/vs-thumb-exit-1");

    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, vs_thumb_exit_2)
{
    // Run fake vs-thumb that exits with status 2
    EnvVarGuard ev_guard(UTIL_DIR, TESTSRCDIR "/thumbnailer/vs-thumb-exit-2");

    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, vs_thumb_exit_99)
{
    // Run fake vs-thumb that exits with status 99
    EnvVarGuard ev_guard(UTIL_DIR, TESTSRCDIR "/thumbnailer/vs-thumb-exit-99");

    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, vs_thumb_crash)
{
    // Run fake vs-thumb that kills itself with SIGTERM
    EnvVarGuard ev_guard(UTIL_DIR, TESTSRCDIR "/thumbnailer/vs-thumb-crash");

    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, not_regular_file)
{
    Thumbnailer tn;

    try
    {
        tn.get_thumbnail("/dev/null", QSize(10, 10));
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::contains(e.what(), "LocalThumbnailRequest(): '/dev/null' is not a regular file")) << e.what();
    }
}

TEST_F(ThumbnailerTest, check_client_access)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TEST_IMAGE, QSize(10, 10));
    ASSERT_NE(nullptr, request.get());
    // Check succeeds for correct user ID and valid label
    request->check_client_credentials(geteuid(), "unconfined");
    try
    {
        request->check_client_credentials(geteuid() + 1, "unconfined");
        FAIL();
    }
    catch (std::exception const& e)
    {
        EXPECT_TRUE(boost::contains(e.what(), "Request comes from a different user ID")) << e.what();
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
    }
}

TEST_F(ThumbnailerTest, invalid_size)
{
    Thumbnailer tn;

    auto request = tn.get_thumbnail(TEST_IMAGE, QSize());
    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), "invalid size: (-1,-1)")) << e.what();
    }
}

TEST_F(ThumbnailerTest, bad_image)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(BAD_IMAGE, QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(ThumbnailerTest, empty_file)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_thumbnail(TESTSRCDIR "/thumbnailer/empty.mp3", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    bool thumbnail_failed = false;
    QByteArray thumbnail;
    try
    {
        thumbnail = request->thumbnail();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("extractor pipeline failed")) << msg;
        EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
        thumbnail_failed = true;
    }
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());

    // Change in glib 2.22: previously, g_file_query_info(..., G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE, ...)
    // for "empty.mp3" returned "audio/mpeg". As of 2.22, it returns "text/plain". This causes
    // an exception on Vivid, but returns an empty thumbnail on Wily. Either behavior is acceptable,
    // seeing that extracting a thumbnail from an empty file is not ever going to produce a thumbnail anyway.
    if (!thumbnail_failed)
    {
        EXPECT_EQ("", thumbnail);
    }
}

TEST_F(ThumbnailerTest, clear_if_old_cache_version)
{
    {
        Thumbnailer tn;

        // Load a song so we have something in the thumbnail cache.
        auto request = tn.get_thumbnail(TEST_SONG, QSize(200, 200));
        ASSERT_NE(nullptr, request.get());
        request->thumbnail();
        auto stats = tn.stats().thumbnail_stats;
        EXPECT_EQ(1, stats.size());
    }

    // Re-open and check that stats are still the same.
    {
        Thumbnailer tn;

        auto stats = tn.stats().thumbnail_stats;
        EXPECT_EQ(1, stats.size());
    }

    // Pretend that this cache is an old 2.3.x cache.
    string cache_version_file = tempdir_path() + "/unity-thumbnailer/thumbnailer-cache-version";
    system((string("echo 0 >") + cache_version_file).c_str());

    // Re-open and check that the cache was wiped.
    {
        Thumbnailer tn;

        auto stats = tn.stats().thumbnail_stats;
        EXPECT_EQ(0, stats.size());
    }
}

class RemoteServer : public ThumbnailerTest
{
protected:
    static void SetUpTestCase()
    {
        // start fake server
        art_server_.reset(new ArtServer());
    }

    static void TearDownTestCase()
    {
        art_server_.reset();
    }

    virtual void SetUp() override
    {
        ThumbnailerTest::SetUp();
        art_server_->unblock_access();
    }

    static unique_ptr<ArtServer> art_server_;
};

unique_ptr<ArtServer> RemoteServer::art_server_;

TEST_F(RemoteServer, basic)
{
    Thumbnailer tn;

    {
        auto request = tn.get_album_art("metallica", "load", QSize(0, 0));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        auto thumb = request->thumbnail();
        Image img(thumb);
        EXPECT_EQ(48, img.width());
        EXPECT_EQ(48, img.height());
    }

    {
        auto request = tn.get_artist_art("metallica", "load", QSize(0, 0));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        auto thumb = request->thumbnail();
        Image img(thumb);
        EXPECT_EQ(48, img.width());
        EXPECT_EQ(48, img.height());
    }

    {
        // For coverage, big images are down-sized for the full-size cache.
        auto request = tn.get_artist_art("big", "image", QSize(5000, 5000));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        auto thumb = request->thumbnail();
        Image img(thumb);
        EXPECT_EQ(1920, img.width());
        EXPECT_EQ(1439, img.height());
    }
}

TEST_F(RemoteServer, no_such_album)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_album_art("no_such_artist", "no_such_album", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));
    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::not_found, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(RemoteServer, decode_fails)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_album_art("empty", "empty", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());
    request->download();

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
}

TEST_F(RemoteServer, no_such_local_image)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    try
    {
        auto request = tn.get_thumbnail("/no_such_file", QSize(10, 10));
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.to_string();
        EXPECT_TRUE(boost::starts_with(msg,
                                       "unity::ResourceException: Thumbnailer::get_thumbnail():\n"
                                       "    boost::filesystem::canonical: No such file or directory: ")) << msg;
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
    }
}

TEST_F(RemoteServer, relative_path)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    try
    {
        auto request = tn.get_thumbnail("xxx", QSize(10, 10));
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.to_string();
        EXPECT_TRUE(boost::starts_with(msg,
                                       "unity::ResourceException: Thumbnailer::get_thumbnail():\n"
                                       "    LocalThumbnailRequest(): xxx: file name must be an absolute path")) << msg;
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
    }
}

TEST_F(RemoteServer, bad_request)
{
    Thumbnailer tn;

    // We do this twice because 400 is not a retryable error. This
    // verifies that a 400 response does add an entry to the failure cache.
    {
        auto old_stats = tn.stats();
        auto request = tn.get_artist_art("error", "400", QSize(10, 10));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));
        EXPECT_EQ("", request->thumbnail());
        EXPECT_EQ(ThumbnailRequest::FetchStatus::hard_error, request->status());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size() + 1, new_stats.failure_stats.size());
    }

    {
        auto old_stats = tn.stats();
        auto request = tn.get_artist_art("error", "400", QSize(10, 10));
        EXPECT_EQ("", request->thumbnail());

        EXPECT_EQ(ThumbnailRequest::FetchStatus::cached_failure, request->status());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.hits() + 1, new_stats.failure_stats.hits());
    }
}

TEST_F(RemoteServer, temporary_error)
{
    Thumbnailer tn;

    // 402 (Payment Required) is a retryable error. This
    // verifies that a 402 response does not add an entry to the failure cache.
    auto old_stats = tn.stats();
    auto request = tn.get_artist_art("error", "402", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));
    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::temporary_error, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
}

TEST_F(RemoteServer, get_artist_empty_strings)
{
    Thumbnailer tn;

    try
    {
        tn.get_artist_art("", "some album", QSize(10, 10));
        FAIL();
    }
    catch (unity::InvalidArgumentException const& e)
    {
        EXPECT_STREQ("unity::InvalidArgumentException: Thumbnailer::get_artist_art(): artist is empty",
                     e.what()) << e.what();
    }
}

TEST_F(RemoteServer, get_album_empty_strings)
{
    Thumbnailer tn;

    try
    {
        tn.get_album_art("some artist", "", QSize(10, 10));
        FAIL();
    }
    catch (unity::InvalidArgumentException const& e)
    {
        EXPECT_STREQ("unity::InvalidArgumentException: Thumbnailer::get_album_art(): album is empty",
                     e.what()) << e.what();
    }
}

TEST_F(RemoteServer, timeout)
{
    Thumbnailer tn;

    auto old_stats = tn.stats();
    auto request = tn.get_album_art("sleep", "3", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());
    request->download(chrono::seconds(1));

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::timeout, request->status());
    auto new_stats = tn.stats();
    EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
}

TEST_F(RemoteServer, server_error)
{
    Thumbnailer tn;

    // We do this twice, so we get coverage on the transient network error handling.
    for (int i = 0; i < 2; ++i)
    {
        auto old_stats = tn.stats();
        auto request = tn.get_album_art("error", "429", QSize(10, 10));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        EXPECT_EQ("", request->thumbnail());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
    }
}

TEST_F(RemoteServer, network_error)
{
    Thumbnailer tn;

    art_server_->block_access();
    {
        auto old_stats = tn.stats();
        auto request = tn.get_album_art("metallica", "load", QSize(10, 10));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        // Still fails
        EXPECT_EQ("", request->thumbnail());
        EXPECT_EQ(ThumbnailRequest::FetchStatus::temporary_error, request->status());
        auto new_stats = tn.stats();
        EXPECT_EQ(old_stats.failure_stats.size(), new_stats.failure_stats.size());
    }

    art_server_->unblock_access();
    {
        auto request = tn.get_album_art("metallica", "load", QSize(10, 10));
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        auto thumb = request->thumbnail();
        Image img(thumb);
        EXPECT_EQ(10, img.width());
        EXPECT_EQ(10, img.height());
    }
}

TEST_F(RemoteServer, album_and_artist_have_distinct_keys)
{
    Thumbnailer tn;

    auto album_request = tn.get_album_art("metallica", "load", QSize(10, 10));
    auto artist_request = tn.get_artist_art("metallica", "load", QSize(10, 10));
    EXPECT_NE(album_request->key(), artist_request->key());
}

TEST_F(RemoteServer, dead_server)
{
    // Dead server won't reply.
    EnvVarGuard ev_guard(UBUNTU_SERVER_URL, "http://deadserver.invalid");

    Thumbnailer tn;

    auto request = tn.get_album_art("some_artist", "some_album", QSize(10, 10));
    EXPECT_EQ("", request->thumbnail());

    request->download();

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    EXPECT_EQ("", request->thumbnail());
    EXPECT_EQ(ThumbnailRequest::FetchStatus::network_down, request->status());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv(UTIL_DIR, TESTBINDIR "/../src/vs-thumb", true);
    setenv(UBUNTU_SERVER_URL, "http://127.0.0.1", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
