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

#include <internal/file_io.h>
#include <internal/image.h>
#include <testsetup.h>
#include <unity/thumbnailer/qt/thumbnailer-qt.h>
#include <utils/artserver.h>
#include <utils/dbusserver.h>
#include <utils/supports_decoder.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#ifdef __clang__
#pragma clang diagnostic ignored "-Wparentheses-equality"
#endif
#include <gst/gst.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace std;
using namespace unity::thumbnailer::internal;

// Simple counter class that we use to count all the signals
// from the async provider instances. This allows us to use
// a single QSignalSpy to wait for the completion of an
// arbitrary number of requests.

class Counter : public QObject
{
    Q_OBJECT
public:
    Counter(int limit)
        : limit_(limit)
        , count_(0)
    {
    }

Q_SIGNALS:
    void counterDone();

public Q_SLOTS:
    void thumbnailComplete()
    {
        if (++count_ == limit_)
        {
            Q_EMIT counterDone();
        }
    }

private:
    int limit_;
    int count_;
};

// Class to run a single request and check that it completed OK.
// Notifies the counter on request completion.

class AsyncThumbnailProvider : public QObject
{
    Q_OBJECT
public:
    AsyncThumbnailProvider(unity::thumbnailer::qt::Thumbnailer* tn, Counter& counter)
        : thumbnailer_(tn)
        , counter_(counter)
    {
        assert(tn);
    }

    void getThumbnail(QString const& path, QSize const& size)
    {
        request_ = thumbnailer_->getThumbnail(path, size);
        connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
                this, &AsyncThumbnailProvider::requestFinished);
    }

    void getAlbumArt(QString const& artist, QString const& album, QSize const& size)
    {
        request_ = thumbnailer_->getAlbumArt(artist, album, size);
        connect(request_.data(), &unity::thumbnailer::qt::Request::finished,
                this, &AsyncThumbnailProvider::requestFinished);
    }

    void cancel()
    {
        request_->cancel();
    }

public Q_SLOTS:
    void requestFinished()
    {
        ASSERT_TRUE(request_->isValid() || request_->errorMessage() == "Request cancelled")
            << request_->errorMessage().toStdString();
        counter_.thumbnailComplete();
    }

private:
    unity::thumbnailer::qt::Thumbnailer* thumbnailer_;
    Counter& counter_;
    QSharedPointer<unity::thumbnailer::qt::Request> request_;
};

class StressTest : public ::testing::Test
{
protected:
    StressTest()
    {
    }

    virtual ~StressTest()
    {
        timer_.stop();
    }

    static void SetUpTestCase()
    {
        // start fake server
        art_server_.reset(new ArtServer());

        // start dbus service
        tempdir.reset(new QTemporaryDir(TESTBINDIR "/stress-test.XXXXXX"));
        setenv("XDG_CACHE_HOME", (tempdir->path() + "/cache").toUtf8().data(), true);

        // set 30 seconds as max idle time
        setenv("THUMBNAILER_MAX_IDLE", "30000", true);

        dbus_.reset(new DBusServer());
        thumbnailer_.reset(new unity::thumbnailer::qt::Thumbnailer(dbus_->connection()));

        // Set up media directories.
        ASSERT_EQ(0, mkdir((temp_dir() + "/Videos").c_str(), 0700));
        ASSERT_EQ(0, mkdir((temp_dir() + "/Music").c_str(), 0700));
        ASSERT_EQ(0, mkdir((temp_dir() + "/Pictures").c_str(), 0700));
    }

    static string temp_dir()
    {
        return tempdir->path().toStdString();
    }

    void run_requests(int num, string const& target_dir, string const& source)
    {
        vector<unique_ptr<AsyncThumbnailProvider>> providers;

        Counter counter(num);
        QSignalSpy spy(&counter, &Counter::counterDone);

        for (int i = 0; i < num; i++)
        {
            QString path = QString::fromStdString(target_dir + "/" + to_string(i) + source);
            unique_ptr<AsyncThumbnailProvider> provider(new AsyncThumbnailProvider(thumbnailer_.get(), counter));
            provider->getThumbnail(path, QSize(512, 512));
            providers.emplace_back(move(provider));
        }
        ASSERT_TRUE(spy.wait(120000));
        ASSERT_EQ(1, spy.count());
    }

    void run_and_cancel_requests(int num, string const& target_dir, string const& source, chrono::milliseconds msecs)
    {
        vector<unique_ptr<AsyncThumbnailProvider>> providers;

        Counter counter(num);
        QSignalSpy spy(&counter, &Counter::counterDone);

        QSignalSpy timer_spy(&timer_, &QTimer::timeout);
        timer_.start(msecs.count());

        for (int i = 0; i < num; i++)
        {
            QString path = QString::fromStdString(target_dir + "/" + to_string(i) + source);
            unique_ptr<AsyncThumbnailProvider> provider(new AsyncThumbnailProvider(thumbnailer_.get(), counter));
            provider->getThumbnail(path, QSize(512, 512));
            providers.emplace_back(move(provider));
        }

        cerr << "pumping event loop" << endl;
        ASSERT_TRUE(timer_spy.wait(msecs.count() + 1000));
        cerr << "stopped pumping event loop" << endl;
        for (auto& p : providers)
        {
            p->cancel();
        }

        cerr << "pumping event loop" << endl;
        ASSERT_TRUE(spy.wait(30000));
        cerr << "done pumping event loop" << endl;
        ASSERT_EQ(1, spy.count());
    }

    static void add_stats(int N_REQUESTS,
                          chrono::system_clock::time_point start_time,
                          chrono::system_clock::time_point finish_time)
    {
        assert(start_time <= finish_time);
        double secs = chrono::duration_cast<chrono::milliseconds>(finish_time - start_time).count() / 1000.0;

        stringstream s;
        s.setf(ios::fixed, ios::floatfield);
        s.precision(3);
        auto info = ::testing::UnitTest::GetInstance()->current_test_info();
        s << info->name() << ": " << N_REQUESTS << " thumbnails in " << secs << " sec ("
          << N_REQUESTS / secs << " req/sec)" << endl;
        stats_ += s.str();
    }

    static void show_stats()
    {
        cout << stats_;
    }

    static void TearDownTestCase()
    {
        thumbnailer_.reset();
        dbus_.reset();
        art_server_.reset();

        unsetenv("THUMBNAILER_MAX_IDLE");
        unsetenv("XDG_CACHE_HOME");
        tempdir.reset();

        show_stats();
    }

    static unique_ptr<QTemporaryDir> tempdir;
    static unique_ptr<DBusServer> dbus_;
    static unique_ptr<unity::thumbnailer::qt::Thumbnailer> thumbnailer_;
    static unique_ptr<ArtServer> art_server_;
    static string stats_;
    static QTimer timer_;
};

unique_ptr<QTemporaryDir> StressTest::tempdir;
unique_ptr<DBusServer> StressTest::dbus_;
unique_ptr<unity::thumbnailer::qt::Thumbnailer> StressTest::thumbnailer_;
unique_ptr<ArtServer> StressTest::art_server_;
string StressTest::stats_;
QTimer StressTest::timer_;

// Little helper function to hard-link a single file a number of times
// under different names, so we can have lots of files without consuming
// tons of disk space.

void make_links(string const& source_path, string const& target_dir, int num_copies)
{
    using namespace boost::filesystem;

    assert(num_copies > 0);
    assert(num_copies <= 2000);  // Probably not good to put more files than this into one directory

    string filename = path(source_path).filename().native();
    string new_name = "0" + filename;

    // Make initial copy
    string copied_file = target_dir + "/" + new_name;
    string contents = read_file(source_path);
    write_file(copied_file, contents);

    // Make num_copies - 1 links to the copy.
    for (int i = 1; i < num_copies; ++i)
    {
        string link_name = target_dir + "/" + to_string(i) + filename;
        ASSERT_TRUE(link(copied_file.c_str(), link_name.c_str()) == 0 || errno == EEXIST) << "errno = " << errno;
    }
}

typedef vector<QSharedPointer<unity::thumbnailer::qt::Request>> RequestVec;

// Test for synchronous wait.

#if 0
TEST_F(StressTest, photo_waitForFinished)
{
    int const N_REQUESTS = 1000;

    string source = "Photo-with-exif.jpg";
    string target_dir = temp_dir() + "/Pictures";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    RequestVec requests;

    auto start = chrono::system_clock::now();
    for (int i = 0; i < N_REQUESTS; i++)
    {
        QString path = QString::fromStdString(target_dir + "/" + to_string(i) + source);
        requests.emplace_back(thumbnailer_->getThumbnail(path, QSize(512, 512)));
    }
    for (auto&& r : requests)
    {
        r->waitForFinished();
    }
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

// Asynchronous tests.

TEST_F(StressTest, photo)
{
    int const N_REQUESTS = 1000;

    string source = "Photo-with-exif.jpg";
    string target_dir = temp_dir() + "/Pictures";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    auto start = chrono::system_clock::now();
    run_requests(N_REQUESTS, target_dir, source);
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

TEST_F(StressTest, photo_no_exif)
{
    int const N_REQUESTS = 200;

    string source = "Photo-without-exif.jpg";
    string target_dir = temp_dir() + "/Pictures";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    auto start = chrono::system_clock::now();
    run_requests(N_REQUESTS, target_dir, source);
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

TEST_F(StressTest, mp3)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for MP3 decoder\n");
        return;
    }

    int const N_REQUESTS = 300;

    string source = "short-track.mp3";
    string target_dir = temp_dir() + "/Music";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    auto start = chrono::system_clock::now();
    run_requests(N_REQUESTS, target_dir, source);
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

TEST_F(StressTest, video)
{
    if (!supports_decoder("video/x-h264"))
    {
        fprintf(stderr, "No support for H.264 decoder\n");
        return;
    }

    int const N_REQUESTS = 50;

    string source = "testvideo.mp4";
    string target_dir = temp_dir() + "/Videos";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    auto start = chrono::system_clock::now();
    run_requests(N_REQUESTS, target_dir, source);
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

TEST_F(StressTest, album_art)
{
    int const N_REQUESTS = 2000;

    vector<unique_ptr<AsyncThumbnailProvider>> providers;

    Counter counter(N_REQUESTS);
    QSignalSpy spy(&counter, &Counter::counterDone);

    auto start = chrono::system_clock::now();
    for (int i = 0; i < N_REQUESTS; i++)
    {
        unique_ptr<AsyncThumbnailProvider> provider(new AsyncThumbnailProvider(thumbnailer_.get(), counter));
        provider->getAlbumArt("metallica", "load", QSize(i + 1, i + 1));
        providers.emplace_back(move(provider));
    }
    ASSERT_TRUE(spy.wait(120000));
    ASSERT_EQ(1, spy.count());
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}
#endif

TEST_F(StressTest, cancel)
{
    if (!supports_decoder("audio/mpeg"))
    {
        fprintf(stderr, "No support for MP3 decoder\n");
        return;
    }

    int const N_REQUESTS = 500;

    string source = "short-track.mp3";
    string target_dir = temp_dir() + "/Music";
    make_links(string(TESTDATADIR) + "/" + source, target_dir, N_REQUESTS);

    auto start = chrono::system_clock::now();
    run_and_cancel_requests(N_REQUESTS, target_dir, source, chrono::milliseconds(2000));
    auto finish = chrono::system_clock::now();

    add_stats(N_REQUESTS, start, finish);
}

int main(int argc, char** argv)
{
    gst_init(&argc, &argv);

    QCoreApplication app(argc, argv);

    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    setenv("TN_UTILDIR", TESTBINDIR "/../src/vs-thumb", true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include "stress_test.moc"
