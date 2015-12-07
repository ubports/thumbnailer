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

#include <internal/env_vars.h>
#include <internal/thumbnailer.h>

#include <testsetup.h>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <unity/UnityExceptions.h>

using namespace std;
using namespace unity::thumbnailer::internal;

#define TEST_SONG TESTDATADIR "/testsong.ogg"

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

TEST_F(ThumbnailerTest, slow_vs_thumb)
{
    Thumbnailer tn;

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));  // Slow vs-thumb will get killed after 10 seconds.

    EXPECT_EQ("", request->thumbnail());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Run fake vs-thumb that does nothing for 20 seconds.
    setenv(env_vars.at("util_dir"), TESTSRCDIR "/slow-vs-thumb/slow", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
