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

#include <internal/file_io.h>
#include <internal/image.h>
#include <internal/raii.h>
#include <internal/trace.h>
#include <testsetup.h>
#include "utils/artserver.h"


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
    string thumb;
    Image img;

    request = tn.get_thumbnail(EMPTY_IMAGE, QSize());
    thumb = request->thumbnail();
    EXPECT_EQ("", thumb);

    // Again, this time we get the answer from the failure cache.
    request = tn.get_thumbnail(EMPTY_IMAGE, QSize());
    thumb = request->thumbnail();
    EXPECT_EQ("", thumb);

    request = tn.get_thumbnail(TEST_IMAGE, QSize());
    EXPECT_TRUE(boost::starts_with(request->key(), TEST_IMAGE)) << request->key();
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

    // Again, for coverage. This time the thumbnail comes from the cache.
    request = tn.get_thumbnail(TEST_IMAGE, QSize());
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());

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

    try
    {
        request = tn.get_thumbnail(BAD_IMAGE, QSize());
        request->thumbnail();
        FAIL();
    }
    catch (std::exception const& e)
    {
        string msg = e.what();
        EXPECT_TRUE(boost::starts_with(msg, "unity::ResourceException: RequestBase::thumbnail(): key = ")) << msg;
    }

    request = tn.get_thumbnail(RGB_IMAGE, QSize(48, 48));
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(48, img.width());
    EXPECT_EQ(48, img.height());

    request = tn.get_thumbnail(BIG_IMAGE, QSize());  // > 1920, so will be trimmed down
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(1920, img.width());
    EXPECT_EQ(1439, img.height());

    request = tn.get_thumbnail(BIG_IMAGE, QSize(0, 0));  // unconstrained, so will not be trimmed down
    thumb = request->thumbnail();
    img = Image(thumb);
    EXPECT_EQ(2731, img.width());
    EXPECT_EQ(2048, img.height());
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
            // Load a song so we have something in the full-size and thumbnail caches.
            auto request = tn.get_thumbnail(TEST_SONG, QSize());
            ASSERT_NE(nullptr, request.get());
            // Audio thumbnails cannot be produced immediately
            ASSERT_EQ("", request->thumbnail());

            QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
            request->download(chrono::milliseconds(15000));
            ASSERT_TRUE(spy.wait(20000));
            string thumb = request->thumbnail();
            ASSERT_NE("", thumb);
            Image img(thumb);
            EXPECT_EQ(200, img.width());
            EXPECT_EQ(200, img.height());
        }

        {
            // Load same song again at different size, so we get a hit on full-size cache.
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
            auto request = tn.get_thumbnail(EMPTY_IMAGE, QSize());
            EXPECT_EQ("", request->thumbnail());
        }

        {
            // Load empty image again, so we get a hit on failure cache.
            auto request = tn.get_thumbnail(EMPTY_IMAGE, QSize());
            EXPECT_EQ("", request->thumbnail());
        }
    };

    fill_cache();

    // Just to show that fill_cache() does put things into the cache and the stats are as expected.
    auto stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.size());
    EXPECT_EQ(2, stats.thumbnail_stats.size());
    EXPECT_EQ(1, stats.failure_stats.size());
    EXPECT_EQ(1, stats.full_size_stats.hits());
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
    EXPECT_EQ(2, stats.thumbnail_stats.size());
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
    EXPECT_EQ(2, stats.thumbnail_stats.size());
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
    EXPECT_EQ(1, stats.full_size_stats.hits());
    EXPECT_EQ(0, stats.thumbnail_stats.hits());
    EXPECT_EQ(1, stats.failure_stats.hits());

    // Re-fill the cache and clear failure stats only.
    tn.clear(Thumbnailer::CacheSelector::all);
    tn.clear_stats(Thumbnailer::CacheSelector::all);
    fill_cache();
    tn.clear_stats(Thumbnailer::CacheSelector::failure_cache);
    stats = tn.stats();
    EXPECT_EQ(1, stats.full_size_stats.hits());
    EXPECT_EQ(1, stats.thumbnail_stats.hits());
    EXPECT_EQ(0, stats.failure_stats.hits());
}

TEST_F(ThumbnailerTest, DISABLED_replace_photo)
{
    string testfile = tempdir_path() + "/foo.jpg";
    ASSERT_EQ(0, link(TEST_IMAGE, testfile.c_str()));

    Thumbnailer tn;
    auto request = tn.get_thumbnail(testfile, QSize());

    // Replace test image with a different file with different
    // dimensions so we can tell which one is thumbnailed.
    ASSERT_EQ(0, unlink(testfile.c_str()));
    ASSERT_EQ(0, link(BIG_IMAGE, testfile.c_str()));

    string data = request->thumbnail();
    Image img(data);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());
}

TEST_F(ThumbnailerTest, thumbnail_video)
{
    Thumbnailer tn;
    auto request = tn.get_thumbnail(TEST_VIDEO, QSize());
    ASSERT_NE(nullptr, request.get());
    // Video thumbnails cannot be produced immediately
    ASSERT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download(chrono::milliseconds(15000));
    ASSERT_TRUE(spy.wait(20000));
    {
        string thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(1920, img.width());
        EXPECT_EQ(1080, img.height());
    }

    {
        // Fetch the thumbnail again with the same size.
        // That causes it to come from the thumbnail cache.
        auto request = tn.get_thumbnail(TEST_VIDEO, QSize());
        string thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(1920, img.width());
        EXPECT_EQ(1080, img.height());
    }

    {
        // Fetch the thumbnail again with a different size.
        // That causes it to be scaled from the thumbnail cache.
        auto request = tn.get_thumbnail(TEST_VIDEO, QSize(500, 500));
        string thumb = request->thumbnail();
        ASSERT_NE("", thumb);
        Image img(thumb);
        EXPECT_EQ(500, img.width());
        EXPECT_EQ(281, img.height());
    }
}

TEST_F(ThumbnailerTest, DISABLED_replace_video)
{
    string testfile = tempdir_path() + "/foo.ogv";
    ASSERT_EQ(0, link(TEST_VIDEO, testfile.c_str())) << strerror(errno);

    Thumbnailer tn;
    auto request = tn.get_thumbnail(testfile, QSize());
    ASSERT_EQ("", request->thumbnail());

    // Replace test image with a different file with different
    // dimensions so we can tell which one is thumbnailed.
    ASSERT_EQ(0, unlink(testfile.c_str()));
    ASSERT_EQ(0, link(BIG_IMAGE, testfile.c_str()));

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download(chrono::milliseconds(15000));
    ASSERT_TRUE(spy.wait(20000));

    string data = request->thumbnail();
    Image img(data);
    EXPECT_EQ(1920, img.width());
    EXPECT_EQ(1080, img.height());
}

TEST_F(ThumbnailerTest, thumbnail_song)
{
    Thumbnailer tn;
    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    ASSERT_NE(nullptr, request.get());
    // Audio thumbnails cannot be produced immediately
    ASSERT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download(chrono::milliseconds(15000));
    ASSERT_TRUE(spy.wait(20000));
    string thumb = request->thumbnail();
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
}

TEST_F(ThumbnailerTest, vs_thumb_exec_failure)
{
    Thumbnailer tn;
    {
        // Cause vs-thumb exec failure.
        char const* tn_util = getenv("TN_UTILDIR");
        ASSERT_TRUE(tn_util && *tn_util != '\0');
        string old_env = tn_util;

        setenv("TN_UTILDIR", "no_such_directory", true);

        auto request = tn.get_thumbnail(TEST_SONG, QSize());
        EXPECT_EQ("", request->thumbnail());

        QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
        request->download();
        ASSERT_TRUE(spy.wait(15000));

        try
        {
            request->thumbnail();
            FAIL();
        }
        catch (unity::ResourceException const& e)
        {
            string msg = e.to_string();
            string exp = "ImageExtractor::data(): failed to start no_such_directory/vs-thumb";
            EXPECT_TRUE(msg.find(exp) != string::npos) << msg;
        }
        setenv("TN_UTILDIR", old_env.c_str(), true);
    }
}

TEST_F(ThumbnailerTest, vs_thumb_exit_1)
{
    Thumbnailer tn;

    // Run fake vs-thumb that exits with status 1
    char const* tn_util = getenv("TN_UTILDIR");
    ASSERT_TRUE(tn_util && *tn_util != '\0');
    string old_env = tn_util;

    setenv("TN_UTILDIR", TESTSRCDIR "/thumbnailer/vs-thumb-exit-1", true);

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("could not extract screenshot")) << msg;
    }

    setenv("TN_UTILDIR", old_env.c_str(), true);
}

TEST_F(ThumbnailerTest, vs_thumb_exit_2)
{
    Thumbnailer tn;

    // Run fake vs-thumb that exits with status 2
    char const* tn_util = getenv("TN_UTILDIR");
    ASSERT_TRUE(tn_util && *tn_util != '\0');
    string old_env = tn_util;

    setenv("TN_UTILDIR", TESTSRCDIR "/thumbnailer/vs-thumb-exit-2", true);

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("extractor pipeline failed")) << msg;
    }

    setenv("TN_UTILDIR", old_env.c_str(), true);
}

TEST_F(ThumbnailerTest, vs_thumb_exit_99)
{
    Thumbnailer tn;

    // Run fake vs-thumb that exits with status 99
    char const* tn_util = getenv("TN_UTILDIR");
    ASSERT_TRUE(tn_util && *tn_util != '\0');
    string old_env = tn_util;

    setenv("TN_UTILDIR", TESTSRCDIR "/thumbnailer/vs-thumb-exit-99", true);

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("unknown exit status 99 from ")) << msg;
    }

    setenv("TN_UTILDIR", old_env.c_str(), true);
}

TEST_F(ThumbnailerTest, vs_thumb_crash)
{
    Thumbnailer tn;

    // Run fake vs-thumb that kills itself with SIGTERM
    char const* tn_util = getenv("TN_UTILDIR");
    ASSERT_TRUE(tn_util && *tn_util != '\0');
    string old_env = tn_util;

    setenv("TN_UTILDIR", TESTSRCDIR "/thumbnailer/vs-thumb-crash", true);

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("vs-thumb crashed")) << msg;
    }

    setenv("TN_UTILDIR", old_env.c_str(), true);
}

TEST_F(ThumbnailerTest, not_regular_file)
{
    Thumbnailer tn;
    try
    {
        auto request = tn.get_thumbnail("/dev/null", QSize());
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
    auto request = tn.get_thumbnail(TEST_IMAGE, QSize());
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
    }
}

TEST_F(ThumbnailerTest, empty_file)
{
    Thumbnailer tn;

    auto request = tn.get_thumbnail(TESTSRCDIR "/thumbnailer/empty.mp3", QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(5000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("could not extract screenshot")) << msg;
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

    static unique_ptr<ArtServer> art_server_;
};

unique_ptr<ArtServer> RemoteServer::art_server_;

TEST_F(RemoteServer, basic)
{
    Thumbnailer tn;

    {
        auto request = tn.get_album_art("metallica", "load", QSize());
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
        auto request = tn.get_artist_art("metallica", "load", QSize());
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
        auto request = tn.get_artist_art("big", "image", QSize());
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

    auto request = tn.get_album_art("no_such_artist", "no_such_album", QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));
    EXPECT_EQ("", request->thumbnail());
}

TEST_F(RemoteServer, decode_fails)
{
    Thumbnailer tn;

    auto request = tn.get_album_art("empty", "empty", QSize());
    EXPECT_EQ("", request->thumbnail());
    request->download();

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    try
    {
        EXPECT_EQ("", request->thumbnail());
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        EXPECT_EQ("unity::ResourceException: RequestBase::thumbnail(): key = empty\\0empty\\0album:\n"
                  "    load_image(): cannot close pixbuf loader: Unrecognized image file format",
                  e.to_string());
    }
}

TEST_F(RemoteServer, no_such_local_image)
{
    Thumbnailer tn;

    try
    {
        auto request = tn.get_thumbnail("no_such_file", QSize());
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.to_string();
        EXPECT_TRUE(boost::starts_with(msg,
                                       "unity::ResourceException: Thumbnailer::get_thumbnail():\n"
                                       "    boost::filesystem::canonical: No such file or directory: ")) << msg;
    }
}

TEST_F(RemoteServer, get_artist_empty_strings)
{
    Thumbnailer tn;

    try
    {
        tn.get_artist_art("", "", QSize());
        FAIL();
    }
    catch (unity::InvalidArgumentException const& e)
    {
        EXPECT_STREQ("unity::InvalidArgumentException: Thumbnailer::get_artist_art(): both artist and album are empty",
                     e.what()) << e.what();
    }
}

TEST_F(RemoteServer, get_album_empty_strings)
{
    Thumbnailer tn;

    try
    {
        tn.get_album_art("", "", QSize());
        FAIL();
    }
    catch (unity::InvalidArgumentException const& e)
    {
        EXPECT_STREQ("unity::InvalidArgumentException: Thumbnailer::get_album_art(): both artist and album are empty",
                     e.what()) << e.what();
    }
}

TEST_F(RemoteServer, timeout)
{
    Thumbnailer tn;

    auto request = tn.get_album_art("sleep", "3", QSize());
    EXPECT_EQ("", request->thumbnail());
    request->download(chrono::seconds(1));

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    EXPECT_EQ("", request->thumbnail());
}

TEST_F(RemoteServer, server_error)
{
    Thumbnailer tn;

    auto request = tn.get_album_art("error", "403", QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.to_string();
        EXPECT_TRUE(boost::starts_with(
                        msg,
                        "unity::ResourceException: RequestBase::thumbnail(): key = error")) << msg;
    }
}

TEST_F(RemoteServer, album_and_artist_have_distinct_keys)
{
    Thumbnailer tn;

    auto album_request = tn.get_album_art("metallica", "load", QSize());
    auto artist_request = tn.get_artist_art("metallica", "load", QSize());
    EXPECT_NE(album_request->key(), artist_request->key());
}

class DeadServer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto apiroot = QString("http://deadserver.invalid:80");
        setenv("THUMBNAILER_UBUNTU_APIROOT", apiroot.toUtf8().constData(), true);
    }

    void TearDown() override
    {
        unsetenv("THUMBNAILER_UBUNTU_APIROOT");
    }
};

TEST_F(DeadServer, errors)
{
    Thumbnailer tn;

    // DeadServer won't reply
    auto request = tn.get_album_art("some_artist", "some_album", QSize());
    EXPECT_EQ("", request->thumbnail());

    request->download();

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    ASSERT_TRUE(spy.wait(15000));

    EXPECT_EQ("", request->thumbnail());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
