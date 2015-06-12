/*
 * Copyright (C) 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include "utils/artserver.h"
#include "utils/dbusserver.h"

#include <internal/file_io.h>
#include <internal/image.h>
#include <testsetup.h>

#include <boost/algorithm/string/predicate.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace boost;
using namespace unity::thumbnailer::internal;

class AdminTest : public ::testing::Test
{
protected:
    AdminTest()
    {
    }
    virtual ~AdminTest()
    {
    }

    virtual void SetUp() override
    {
        // start dbus service
        tempdir.reset(new QTemporaryDir(TESTBINDIR "/dbus-test.XXXXXX"));
        ASSERT_NE(-1, chdir(temp_dir().c_str()));
        setenv("XDG_CACHE_HOME", qPrintable(tempdir->path() + "/cache"), true);

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

        unsetenv("THUMBNAILER_MAX_IDLE");
        unsetenv("XDG_CACHE_HOME");
        tempdir.reset();
    }

    unique_ptr<QTemporaryDir> tempdir;
    unique_ptr<DBusServer> dbus_;
};

class AdminRunner
{
public:
    AdminRunner()
    {
    }
    int run(QStringList const& args)
    {
        process_.reset(new QProcess);
        process_->setStandardInputFile(QProcess::nullDevice());
        process_->setProcessChannelMode(QProcess::SeparateChannels);
        process_->start(THUMBNAILER_ADMIN, args);
        process_->waitForFinished();
        EXPECT_EQ(QProcess::NormalExit, process_->exitStatus());
        stdout_ = process_->readAllStandardOutput().toStdString();
        stderr_ = process_->readAllStandardError().toStdString();
        return process_->exitCode();
    }

    string stdout() const
    {
        return stdout_;
    }

    string stderr() const
    {
        return stderr_;
    }

private:
    unique_ptr<QProcess> process_;
    string stdout_;
    string stderr_;
};

TEST_F(AdminTest, no_args)
{
    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"stats"}));
    auto output = ar.stdout();
    EXPECT_TRUE(output.find("Image cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Thumbnail cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Failure cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Histogram:") != string::npos) << output;
}

TEST_F(AdminTest, image_stats)
{
    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    auto output = ar.stdout();
    EXPECT_TRUE(output.find("Image cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Thumbnail cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Failure cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Histogram:") != string::npos) << output;
}

TEST_F(AdminTest, thumbnail_stats)
{
    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    auto output = ar.stdout();
    EXPECT_FALSE(output.find("Image cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Thumbnail cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Failure cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Histogram:") != string::npos) << output;
}

TEST_F(AdminTest, failure_stats)
{
    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    auto output = ar.stdout();
    EXPECT_FALSE(output.find("Image cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Thumbnail cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Failure cache:") != string::npos) << output;
    EXPECT_FALSE(output.find("Histogram:") != string::npos) << output;
}

TEST_F(AdminTest, histogram)
{
    AdminRunner ar;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "-v"}));
    auto output = ar.stdout();
    EXPECT_TRUE(output.find("Image cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Thumbnail cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Failure cache:") != string::npos) << output;
    EXPECT_TRUE(output.find("Histogram:") != string::npos) << output;

    // Add a file to the cache
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/orientation-1.jpg"}));
    EXPECT_EQ(0, ar.run(QStringList{"stats", "-v", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;
    EXPECT_TRUE(output.find("8000-8999: 1") != string::npos) << output;

    // Add a small file to the cache
    EXPECT_EQ(0, ar.run(QStringList{"get", "--size=32", TESTDATADIR "/orientation-1.jpg"}));
    EXPECT_EQ(0, ar.run(QStringList{"stats", "-v", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  2") != string::npos) << output;
    EXPECT_TRUE(output.find("900-999: 1") != string::npos) << output;
    EXPECT_TRUE(output.find("5000-5999: 0") != string::npos) << output;
    EXPECT_TRUE(output.find("8000-8999: 1") != string::npos) << output;
}

TEST_F(AdminTest, cmd_parsing)
{
    AdminRunner ar;

    // Too few args
    EXPECT_EQ(1, ar.run(QStringList{}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    // Bad command
    EXPECT_EQ(1, ar.run(QStringList{"no_such_command"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: no_such_command: invalid command")) << ar.stderr();
}

TEST_F(AdminTest, stats_parsing)
{
    AdminRunner ar;

    // Too many args
    EXPECT_EQ(1, ar.run(QStringList{"stats", "i", "t"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: too many arguments")) << ar.stderr();

    // Second arg wrong
    EXPECT_EQ(1, ar.run(QStringList{"stats", "foo"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: invalid cache_id: foo")) << ar.stderr();

    // Bad option
    EXPECT_EQ(1, ar.run(QStringList{"stats", "foo", "-x"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Unknown option 'x'.")) << ar.stderr();

    // Help option
    EXPECT_EQ(1, ar.run(QStringList{"stats", "-h"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();
}

TEST_F(AdminTest, clear_stats_parsing)
{
    AdminRunner ar;

    // Too many args
    EXPECT_EQ(1, ar.run(QStringList{"clear-stats", "i", "t"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: too many arguments")) << ar.stderr();

    // Second arg wrong
    EXPECT_EQ(1, ar.run(QStringList{"clear-stats", "foo"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: invalid cache_id: foo")) << ar.stderr();

    // Bad option
    EXPECT_EQ(1, ar.run(QStringList{"clear-stats", "foo", "-x"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Unknown option 'x'.")) << ar.stderr();

    // Help option
    EXPECT_EQ(1, ar.run(QStringList{"clear-stats", "-h"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();
}

TEST_F(AdminTest, clear_parsing)
{
    AdminRunner ar;

    // Too many args
    EXPECT_EQ(1, ar.run(QStringList{"clear", "i", "t"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: too many arguments")) << ar.stderr();

    // Second arg wrong
    EXPECT_EQ(1, ar.run(QStringList{"clear", "foo"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: invalid cache_id: foo")) << ar.stderr();

    // Bad option
    EXPECT_EQ(1, ar.run(QStringList{"clear", "foo", "-x"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Unknown option 'x'.")) << ar.stderr();

    // Help option
    EXPECT_EQ(1, ar.run(QStringList{"clear", "-h"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();
}

TEST_F(AdminTest, clear_and_clear_stats)
{
    AdminRunner ar;

    // Put something in the cache.
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/testsong.ogg"}));
    // Again, so we get a hit on thumbnail cache.
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/testsong.ogg"}));
    // Again, with different size, so we get a hit on full-size cache.
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/testsong.ogg", "--size=20x20"}));
    // Put something in the failure cache.
    EXPECT_EQ(1, ar.run(QStringList{"get", TESTDATADIR "/empty"}));
    // Again, so we get a hit on the failure cache.
    EXPECT_EQ(1, ar.run(QStringList{"get", TESTDATADIR "/empty"}));

    // Check that each of the three caches is non-empty.

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    auto output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  2") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;

    // Check that the stats of the three caches show hits.

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  1") != string::npos) << output;

    // Clear thumbnail stats only and check that only thumbnail stats were cleared.

    EXPECT_EQ(0, ar.run(QStringList{"clear-stats", "t"}));

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  0") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  1") != string::npos) << output;

    // Clear all stats and check that all stats were cleared.

    EXPECT_EQ(0, ar.run(QStringList{"clear-stats"}));

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  0") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  0") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Hits:                  0") != string::npos) << output;

    // Check that each of the three caches is still non-empty. (We've only cleared
    // the stats so far, not the actual caches.)

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  2") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;

    // Clear the failure cache only and check that it was cleared.

    EXPECT_EQ(0, ar.run(QStringList{"clear", "f"}));

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  1") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  2") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  0") != string::npos) << output;

    // Clear all caches and check that they were cleared.

    EXPECT_EQ(0, ar.run(QStringList{"clear"}));

    EXPECT_EQ(0, ar.run(QStringList{"stats", "i"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  0") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "t"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  0") != string::npos) << output;

    EXPECT_EQ(0, ar.run(QStringList{"stats", "f"}));
    output = ar.stdout();
    EXPECT_TRUE(output.find("Size:                  0") != string::npos) << output;
}

TEST_F(AdminTest, get_fullsize)
{
    auto filename = temp_dir() + "/orientation-1_0x0.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/orientation-1.jpg"}));

    // Image must have been created with the right name and contents.
    string data = read_file(filename);
    Image img(data);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());
    EXPECT_EQ(0xFE0000, img.pixel(0, 0));
    EXPECT_EQ(0xFFFF00, img.pixel(639, 0));
    EXPECT_EQ(0x00FF01, img.pixel(639, 479));
    EXPECT_EQ(0x0000FE, img.pixel(0, 479));
}

TEST_F(AdminTest, get_large_thumbnail)
{
    auto filename = temp_dir() + "/orientation-1_320x240.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", "-s=320x240", TESTDATADIR "/orientation-1.jpg"}));

    // Image must have been created with the right name and contents.
    string data = read_file(filename);
    Image img(data);
    EXPECT_EQ(320, img.width());
    EXPECT_EQ(240, img.height());
    EXPECT_EQ(0xFE0000, img.pixel(0, 0));
    EXPECT_EQ(0xFFFF00, img.pixel(319, 0));
    EXPECT_EQ(0x00FF01, img.pixel(319, 239));
    EXPECT_EQ(0x0000FE, img.pixel(0, 239));
}

TEST_F(AdminTest, get_small_thumbnail_square)
{
    auto filename = temp_dir() + "/orientation-1_48x48.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", "--size=48", TESTDATADIR "/orientation-1.jpg"}));

    // Image must have been created with the right name and contents.
    string data = read_file(filename);
    Image img(data);
    EXPECT_EQ(48, img.width());
    EXPECT_EQ(36, img.height());
    EXPECT_EQ(0xFE8081, img.pixel(0, 0));
    EXPECT_EQ(0xFFFF80, img.pixel(47, 0));
    EXPECT_EQ(0x81FF81, img.pixel(47, 35));
    EXPECT_EQ(0x807FFE, img.pixel(0, 35));
}

TEST_F(AdminTest, get_with_dir)
{
    auto filename = temp_dir() + "/orientation-2_0x0.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTDATADIR "/orientation-2.jpg", QString::fromStdString(temp_dir())}));

    // Image must have been created with the right name and contents.
    string data = read_file(filename);
    Image img(data);
    EXPECT_EQ(640, img.width());
    EXPECT_EQ(480, img.height());
    EXPECT_EQ(0xFE0000, img.pixel(0, 0));
    EXPECT_EQ(0xFFFF00, img.pixel(639, 0));
    EXPECT_EQ(0x00FF01, img.pixel(639, 479));
    EXPECT_EQ(0x0000FE, img.pixel(0, 479));
}

TEST_F(AdminTest, get_parsing)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"get"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get", "--invalid"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Unknown option 'invalid'.")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get", "--help"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get", "--size=abc", TESTDATADIR "/orientation-1/jpg"}));
    EXPECT_EQ("thumbnailer-admin: GetLocalThumbnail(): invalid size: abc\n", ar.stderr()) << ar.stderr();
}

TEST_F(AdminTest, bad_files)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"get", "no_such_file"}));
    EXPECT_EQ("thumbnailer-admin: GetLocalThumbnail::run(): cannot open no_such_file: No such file or directory\n",
              ar.stderr())
        << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get", TESTDATADIR "/orientation-2.jpg", "no_such_directory"}));
    EXPECT_EQ(
        "thumbnailer-admin: GetLocalThumbnail::run(): write_file(): "
        "cannot open no_such_directory/orientation-2_0x0.jpg: No such file or directory\n",
        ar.stderr())
        << ar.stderr();
}

class RemoteServer : public AdminTest
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

TEST_F(RemoteServer, get_artist_album_parsing)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"get_artist"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get_artist", "artist", "album", "dir", "something else"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get_artist", "--invalid"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Unknown option 'invalid'.")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get_artist", "--help"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get_artist", "--size=abc", "artist", "album"}));
    EXPECT_EQ("thumbnailer-admin: GetRemoteThumbnail(): invalid size: abc\n", ar.stderr()) << ar.stderr();
}

unique_ptr<ArtServer> RemoteServer::art_server_;

TEST_F(RemoteServer, get_artist)
{
    auto filename = temp_dir() + "/metallica_load_artist_0x0.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get_artist", "metallica", "load"})) << ar.stderr();

    string cmd = "/usr/bin/test -s " + filename + " || exit 1";
    int rc = system(cmd.c_str());
    EXPECT_EQ(0, rc);
}

TEST_F(RemoteServer, get_album)
{
    auto filename = temp_dir() + "/metallica_load_album_48x48.jpg";

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get_album", "metallica", "load", "--size=48"})) << ar.stderr();

    string cmd = "/usr/bin/test -s " + filename + " || exit 1";
    int rc = system(cmd.c_str());
    EXPECT_EQ(0, rc);
}

TEST_F(RemoteServer, get_error)
{
    AdminRunner ar;
    EXPECT_EQ(1, ar.run(QStringList{"get_album", "foo", "bar", "--size=48"}));
    EXPECT_EQ("thumbnailer-admin: Handler::create_finished(): could not get thumbnail for album: foo/bar (48,48): "
              "NO ARTWORK\n", ar.stderr()) << ar.stderr();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    setenv("LC_ALL", "C", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
