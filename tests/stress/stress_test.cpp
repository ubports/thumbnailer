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

#include <internal/image.h>
#include <testsetup.h>
#include <utils/artserver.h>
#include <utils/dbusserver.h>

#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>


using namespace std;

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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
