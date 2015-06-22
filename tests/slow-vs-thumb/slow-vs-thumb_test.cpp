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

#include <testsetup.h>

#include <gtest/gtest.h>
#include <QSignalSpy>
#include <unity/UnityExceptions.h>

using namespace std;
using namespace unity::thumbnailer::internal;

#define TEST_SONG TESTDATADIR "/testsong.ogg"

TEST(ThumbnailerTest, slow_vs_thumb)
{
    Thumbnailer tn;

    auto request = tn.get_thumbnail(TEST_SONG, QSize());
    EXPECT_EQ("", request->thumbnail());

    QSignalSpy spy(request.get(), &ThumbnailRequest::downloadFinished);
    request->download();
    ASSERT_TRUE(spy.wait(15000));  // Slow vs-thumb will get killed after 10 seconds.

    try
    {
        request->thumbnail();
        FAIL();
    }
    catch (unity::ResourceException const& e)
    {
        string msg = e.what();
        EXPECT_NE(string::npos, msg.find("did not return after 10000 milliseconds")) << msg;
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Run fake vs-thumb that does nothing for 20 seconds.
    setenv("TN_UTILDIR", TESTSRCDIR "/slow-vs-thumb/slow", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
