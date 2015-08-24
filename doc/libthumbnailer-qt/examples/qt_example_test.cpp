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

//! [AsyncThumbnailProvider_definition]
#include <unity/thumbnailer/qt/thumbnailer-qt.h>

class AsyncThumbnailProvider : public QObject
{
    Q_OBJECT
public:
    AsyncThumbnailProvider(unity::thumbnailer::qt::Thumbnailer& thumbnailer)
        : thumbnailer_(thumbnailer)
    {
    }

    void getThumbnail(QString const& path, QSize const& size);

    QImage image() const
    {
        if (request_ && request_->isValid())
        {
            return request_->image();
        }
        return QImage();
    }

Q_SIGNALS:
    void imageReady();

public Q_SLOTS:
    void requestFinished();

private:
    unity::thumbnailer::qt::Thumbnailer& thumbnailer_;
    QSharedPointer<unity::thumbnailer::qt::Request> request_;
};
//! [AsyncThumbnailProvider_definition]

//! [AsyncThumbnailProvider_async_implementation]
void AsyncThumbnailProvider::getThumbnail(QString const& path, QSize const& size)
{
    request_ = thumbnailer_.getThumbnail(path, size);
    connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
            this, &AsyncThumbnailProvider::requestFinished);
}

void AsyncThumbnailProvider::requestFinished()
{
    if (request_->isValid())
    {
        Q_EMIT imageReady();
    }
    else
    {
        request_.reset(nullptr);
        QString errorMessage = request_->errorMessage();
        // Do whatever you need to do to report the error.
    }
}
//! [AsyncThumbnailProvider_async_implementation]

//! [SyncThumbnailProvider_definition]
class SyncThumbnailProvider : public QObject
{
    Q_OBJECT
public:
    SyncThumbnailProvider(unity::thumbnailer::qt::Thumbnailer& thumbnailer)
        : thumbnailer_(thumbnailer)
    {
    }

    QImage getThumbnail(QString const& path, QSize const& size);

private:
    unity::thumbnailer::qt::Thumbnailer& thumbnailer_;
};
//! [SyncThumbnailProvider_definition]

//! [SyncThumbnailProvider_implementation]
QImage SyncThumbnailProvider::getThumbnail(QString const& path, QSize const& size)
{
    auto request = thumbnailer_.getThumbnail(path, size);

    request->waitForFinished();  // Blocks until the response is ready.

    if (request->isValid())
    {
        return request->image();
    }

    QString errorMessage = request->errorMessage();
    // Do whatever you need to do to report the error.
    return QImage();
}
//! [SyncThumbnailProvider_implementation]

#include "qt_example_test.moc"

#include <testsetup.h>
#include <utils/dbusserver.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <memory>

using namespace std;

// The thumbnailer uses g_get_user_cache_dir() to get the cache dir, and
// glib remembers that value, so changing XDG_CACHE_HOME later has no effect.

static auto set_tempdir = []()
{
    auto dir = new QTemporaryDir(TESTBINDIR "/test-dir.XXXXXX");
    setenv("XDG_CACHE_HOME", dir->path().toUtf8().data(), true);
    return dir;
};
static unique_ptr<QTemporaryDir> tempdir(set_tempdir());

class QtTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        // start dbus service
        tempdir.reset(new QTemporaryDir(TESTBINDIR "/dbus-test.XXXXXX"));
        setenv("XDG_CACHE_HOME", (tempdir->path() + "/cache").toUtf8().data(), true);

        // set 3 seconds as max idle time
        setenv("THUMBNAILER_MAX_IDLE", "1000", true);

        dbus_.reset(new DBusServer());
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

TEST_F(QtTest, basic)
{
    unity::thumbnailer::qt::Thumbnailer thumbnailer;

    AsyncThumbnailProvider async_prov(thumbnailer);
    QSignalSpy spy(&async_prov, &AsyncThumbnailProvider::imageReady);
    async_prov.getThumbnail(TESTSRCDIR "/media/testimage.jpg", QSize(80, 80));
    ASSERT_TRUE(spy.wait());
    auto image = async_prov.image();
    EXPECT_EQ(80, image.width());
    EXPECT_EQ(50, image.height());

    SyncThumbnailProvider sync_prov(thumbnailer);
    image = sync_prov.getThumbnail(TESTSRCDIR "/media/testimage.jpg", QSize(40, 40));
    EXPECT_EQ(40, image.width());
    EXPECT_EQ(25, image.height());
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
