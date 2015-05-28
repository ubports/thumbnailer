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
        setenv("XDG_CACHE_HOME", qPrintable(tempdir->path() + "/cache"), true);

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
    AdminRunner() {}
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

// TODO: How to do this better?
#if 0
TEST(ServiceTest, stats_service_not_running)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"stats"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: No such interface")) << ar.stderr();
}

TEST(ServiceTest, get_service_not_running)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"get", TESTSRCDIR "/media/orientation-2.jpg"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: No such interface")) << ar.stderr();
}
#endif

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
}

TEST_F(AdminTest, stats_parsing)
{
    AdminRunner ar;

    // Too few args
    EXPECT_EQ(1, ar.run(QStringList{}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: Usage: ")) << ar.stderr();

    // Too many args
    EXPECT_EQ(1, ar.run(QStringList{"stats", "i", "t"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: too many arguments")) << ar.stderr();

    // Second arg wrong
    EXPECT_EQ(1, ar.run(QStringList{"stats", "foo"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: invalid cache_id: foo")) << ar.stderr();

    // Bad command
    EXPECT_EQ(1, ar.run(QStringList{"no_such_command"}));
    EXPECT_TRUE(starts_with(ar.stderr(), "thumbnailer-admin: no_such_command: invalid command")) << ar.stderr();
}

TEST_F(AdminTest, get_fullsize)
{
    auto filename = string(TESTBINDIR "/tools/orientation-1_0x0.jpg");
    unlink(filename.c_str());

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTSRCDIR "/media/orientation-1.jpg"}));

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
    auto filename = string(TESTBINDIR "/tools/orientation-1_320x240.jpg");
    unlink(filename.c_str());

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", "-s=320x240", TESTSRCDIR "/media/orientation-1.jpg"}));

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
    auto filename = string(TESTBINDIR "/tools/orientation-1_48x48.jpg");
    unlink(filename.c_str());

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", "--size=48", TESTSRCDIR "/media/orientation-1.jpg"}));

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
    auto filename = string(TESTBINDIR "/orientation-2_0x0.jpg");
    unlink(filename.c_str());

    AdminRunner ar;
    EXPECT_EQ(0, ar.run(QStringList{"get", TESTSRCDIR "/media/orientation-2.jpg", TESTBINDIR}));

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

    EXPECT_EQ(1, ar.run(QStringList{"get", "--size=abc", TESTSRCDIR "/media/orientation-1/jpg"}));
    EXPECT_EQ("thumbnailer-admin: GetThumbnail(): invalid size: abc\n", ar.stderr()) << ar.stderr();
}

TEST_F(AdminTest, bad_files)
{
    AdminRunner ar;

    EXPECT_EQ(1, ar.run(QStringList{"get", "no_such_file"}));
    EXPECT_EQ("thumbnailer-admin: GetThumbnail::run(): cannot open no_such_file: No such file or directory\n",
              ar.stderr()) << ar.stderr();

    EXPECT_EQ(1, ar.run(QStringList{"get", TESTSRCDIR "/media/orientation-2.jpg", "no_such_directory"}));
    EXPECT_EQ("thumbnailer-admin: GetThumbnail::run(): cannot open no_such_directory/orientation-2_0x0.jpg: "
              "No such file or directory\n",
              ar.stderr()) << ar.stderr();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    setenv("LC_ALL", "C", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
