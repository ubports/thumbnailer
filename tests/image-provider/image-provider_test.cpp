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
 */

#include <albumartgenerator.h>
#include <artistartgenerator.h>
#include <thumbnailgenerator.h>

#include <unity/thumbnailer/qt/thumbnailer-qt.h>
#include <internal/env_vars.h>
#include <testsetup.h>
#include "utils/artserver.h"
#include "utils/dbusserver.h"
#include "utils/testutils.h"

#include <gtest/gtest.h>
#include <QGuiApplication>
#include <QImage>
#include <QQuickImageProvider>
#include <QSignalSpy>
#include <QSize>
#include <QTemporaryDir>

#include <cstdlib>
#include <memory>

using namespace std;
using namespace unity::thumbnailer::qml;
using unity::thumbnailer::internal::UTIL_DIR;
using unity::thumbnailer::qt::Thumbnailer;

class ProviderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        art_server_.reset(new ArtServer());
        tempdir_.reset(new QTemporaryDir(TESTBINDIR "provider-test.XXXXXX"));
        setenv("XDG_CACHE_HOME", (temp_dir() + "/cache").c_str(), true);
        dbus_.reset(new DBusServer());
        thumbnailer_.reset(new Thumbnailer(dbus_->connection()));
    }

    void TearDown() override
    {
        thumbnailer_.reset();
        dbus_.reset();
        art_server_.reset();
        unsetenv("XDG_CACHE_HOME");
        tempdir_.reset();
    }

    string temp_dir()
    {
        return tempdir_->path().toStdString();
    }

    unique_ptr<QTemporaryDir> tempdir_;
    unique_ptr<DBusServer> dbus_;
    unique_ptr<ArtServer> art_server_;
    shared_ptr<Thumbnailer> thumbnailer_;
};

namespace
{
int const SIGNAL_WAIT_TIME = 10000;

void wait(QQuickImageResponse* response) {
    // Using old signal syntax because the new one fails
    // on arm64 with Vivid.
    QSignalSpy spy(response, SIGNAL(finished()));
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    ASSERT_EQ(1, spy.count());
}
}

TEST_F(ProviderTest, thumbnail_image)
{
    const char* filename = TESTDATADIR "/orientation-1.jpg";

    ThumbnailGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(filename, QSize(128, 128)));
    wait(response.get());
    ASSERT_EQ("", response->errorString());

    unique_ptr<QQuickTextureFactory> factory(response->textureFactory());
    ASSERT_NE(nullptr, factory.get());
    QImage image = factory->image();

    EXPECT_EQ(128, image.width());
    EXPECT_EQ(96, image.height());
    EXPECT_EQ(QColor("#FE8081").rgb(), image.pixel(0, 0));
    EXPECT_EQ(QColor("#FFFF80").rgb(), image.pixel(127, 0));
    EXPECT_EQ(QColor("#807FFE").rgb(), image.pixel(0, 95));
    EXPECT_EQ(QColor("#81FF81").rgb(), image.pixel(127, 95));
}

TEST_F(ProviderTest, thumbnail_cancel)
{
    const char* filename = TESTDATADIR "/orientation-1.jpg";

    ThumbnailGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(filename, QSize(128, 128)));
    response->cancel();
    wait(response.get());
    EXPECT_EQ("Request cancelled", response->errorString());
}

TEST_F(ProviderTest, thumbnail_missing)
{
    const char* filename = TESTDATADIR "/no-such-file.jpg";

    ThumbnailGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(filename, QSize(128, 128)));
    wait(response.get());
    string error = response->errorString().toStdString();
    EXPECT_NE(string::npos, error.find("No such file or directory")) << error;
}

TEST_F(ProviderTest, albumart)
{
    const char* id = "artist=metallica&album=load";

    AlbumArtGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(id, QSize(128, 128)));
    wait(response.get());
    EXPECT_EQ("", response->errorString());

    unique_ptr<QQuickTextureFactory> factory(response->textureFactory());
    ASSERT_NE(nullptr, factory.get());
    QImage image = factory->image();

    EXPECT_EQ(48, image.width());
    EXPECT_EQ(48, image.height());
    EXPECT_EQ(QColor("#C80000").rgb(), image.pixel(0, 0));
    EXPECT_EQ(QColor("#00D200").rgb(), image.pixel(47, 0));
    EXPECT_EQ(QColor("#0000DC").rgb(), image.pixel(0, 47));
    EXPECT_EQ(QColor("#646E78").rgb(), image.pixel(47, 47));
}

TEST_F(ProviderTest, albumart_missing)
{
    const char* id = "artist=no-such-artist&album=no-such-album";

    AlbumArtGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(id, QSize(128, 128)));
    wait(response.get());
    string error = response->errorString().toStdString();
    EXPECT_NE(string::npos, error.find("could not get thumbnail for album")) << error;
}

TEST_F(ProviderTest, artistart)
{
    const char* id = "artist=beck&album=odelay";

    ArtistArtGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(id, QSize(128, 128)));
    wait(response.get());
    EXPECT_EQ("", response->errorString());

    unique_ptr<QQuickTextureFactory> factory(response->textureFactory());
    ASSERT_NE(nullptr, factory.get());
    QImage image = factory->image();

    EXPECT_EQ(128, image.width());
    EXPECT_EQ(96, image.height());
    EXPECT_EQ(QColor("#FE0000").rgb(), image.pixel(0, 0));
    EXPECT_EQ(QColor("#FFFF00").rgb(), image.pixel(127, 0));
    EXPECT_EQ(QColor("#0000FE").rgb(), image.pixel(0, 95));
    EXPECT_EQ(QColor("#00FF01").rgb(), image.pixel(127, 95));
}

TEST_F(ProviderTest, artistart_missing)
{
    const char* id = "artist=no-such-artist&album=no-such-album";

    ArtistArtGenerator provider(thumbnailer_);
    unique_ptr<QQuickImageResponse> response(
        provider.requestImageResponse(id, QSize(128, 128)));
    wait(response.get());
    string error = response->errorString().toStdString();
    EXPECT_NE(string::npos, error.find("could not get thumbnail for artist")) << error;
}

int main(int argc, char **argv)
{
#if defined(SKIP_DBUS_TESTS)
    // TODO: Re-enable this ASAP!
    cerr << "WARNING: Skipping tests on " << DISTRO << endl;
    cerr << "         See https://bugs.launchpad.net/ubuntu/+source/thumbnailer/+bug/1613561" << endl;
    cerr << "             https://bugs.launchpad.net/ubuntu/+source/qtbase-opensource-src/+bug/1625930" << endl;
    return 0;
#endif
    QGuiApplication app(argc, argv);
    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv(UTIL_DIR, TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
