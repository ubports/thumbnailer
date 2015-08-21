#include <unity/thumbnailer/qt/thumbnailer-qt.h>

class MyClass : public QObject
{
    Q_OBJECT
public:
    void getThumbnailAsync(QString const& path, QSize const& size);
    QImage getThumbnail(QString const& path, QSize const& size);

    QImage image() const
    {
        return image_;
    }

Q_SIGNALS:
    void imageReady();

public Q_SLOTS:
    void requestFinished();

private:
    unity::thumbnailer::qt::Thumbnailer thumbnailer_;
    QSharedPointer<unity::thumbnailer::qt::Request> request_;
    QImage image_;
};

void MyClass::getThumbnailAsync(QString const& path, QSize const& size)
{
    request_ = thumbnailer_.getThumbnail(path, size);
    connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
            this, &MyClass::requestFinished);
}

void MyClass::requestFinished()
{
    if (request_->isValid())
    {
        image_ = request_->image();  // Get the image from the request.
        Q_EMIT imageReady();
    }
    else
    {
        QString errorMessage = request_->errorMessage();
        // Do whatever you need to do to report the error.
        image_ = QImage();
    }
}

QImage MyClass::getThumbnail(QString const& path, QSize const& size)
{
    auto request = thumbnailer_.getThumbnail(path, size);

    request->waitForFinished();  // Blocks until the response is ready.

    if (request->isValid())
    {
        return image_ = request->image();
    }

    QString errorMessage = request->errorMessage();
    // Do whatever you need to do to report the error.
    return image_ = QImage();
}

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
    MyClass c;

    QSignalSpy spy(&c, &MyClass::imageReady);
    c.getThumbnailAsync(TESTSRCDIR "/media/testimage.jpg", QSize(80, 80));
    ASSERT_TRUE(spy.wait());
    auto image = c.image();
    EXPECT_EQ(80, image.width());
    EXPECT_EQ(50, image.height());

    image = c.getThumbnail(TESTSRCDIR "/media/testimage.jpg", QSize(40, 40));
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
