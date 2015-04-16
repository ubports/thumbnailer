/*
 * Copyright (C) 2014 Canonical Ltd.
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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <internal/qurldownloader.h>
#include <internal/ubuntuserverdownloader.h>

#include <gtest/gtest.h>

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QVector>

#include <core/posix/exec.h>

#include <chrono>
#include <thread>

using namespace unity::thumbnailer::internal;
namespace posix = core::posix;

// Thread to download a file and check its content
// The fake server generates specific file content when the given artist is "test_threads"

// The content coming from the fake server is: TEST_THREADS_TEST_ + the give download_id.
// Example: download_id = "TEST_1" --> content is: "TEST_THREADS_TEST_TEST_1"
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(QString download_id, QObject* parent = nullptr)
        : QThread(parent)
        , download_id_(download_id)
    {
    }

private:
    void run() Q_DECL_OVERRIDE
    {
        UbuntuServerDownloader downloader;
        QString url = downloader.download_album("test_threads", download_id_);
        QSignalSpy spy(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

        // check the returned url
        QString url_to_check =
            QString(
                "/musicproxy/v1/album-art?artist=test_threads&album=%1&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc")
                .arg(download_id_);
        ASSERT_TRUE(url.endsWith(url_to_check) == true);

        // we set a timeout of 5 seconds waiting for the signal to be emitted,
        // which should never be reached
        spy.wait(5000);

        // check that we've got exactly one signal
        ASSERT_EQ(spy.count(), 1);

        QList<QVariant> arguments = spy.takeFirst();
        ASSERT_EQ(arguments.at(0).toString().endsWith(url_to_check), true);
        // Finally check the content of the file downloaded
        ASSERT_EQ(arguments.at(1).toString(), QString("TEST_THREADS_TEST_%1").arg(download_id_));
    }

private:
    QString download_id_;
};

class TestDownloaderServer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        fake_downloader_server_ = posix::exec(FAKE_DOWNLOADER_SERVER, {}, {}, posix::StandardStream::stdout);

        ASSERT_GT(fake_downloader_server_.pid(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::string port;
        fake_downloader_server_.cout() >> port;

        apiroot_ = "http://127.0.0.1:" + QString::fromStdString(port);
        setenv("THUMBNAILER_LASTFM_APIROOT", apiroot_.toStdString().c_str(), true);
        setenv("THUMBNAILER_UBUNTU_APIROOT", apiroot_.toStdString().c_str(), true);
    }

    void TearDown() override
    {
        unsetenv("THUMBNAILER_LASTFM_APIROOT");
        unsetenv("THUMBNAILER_UBUNTU_APIROOT");
    }

    posix::ChildProcess fake_downloader_server_ = posix::ChildProcess::invalid();
    QString apiroot_;
};

TEST_F(TestDownloaderServer, test_ok_album)
{
    UbuntuServerDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download_album("sia", "fear");
    ASSERT_EQ(
        url.endsWith("/musicproxy/v1/album-art?artist=sia&album=fear&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc"),
        true);

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    // check the arguments of the signal.
    // With this we check that the api_key is OK and that the url are build as
    // expected.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString().endsWith(
                  "/musicproxy/v1/album-art?artist=sia&album=fear&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc"),
              true);
    // Finally check the content of the file downloaded
    ASSERT_EQ(arguments.at(1).toString(), QString("SIA_FEAR_TEST_STRING_IMAGE"));
}

TEST_F(TestDownloaderServer, test_ok_artist)
{
    UbuntuServerDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download_artist("sia", "fear");
    QVERIFY(
        url.endsWith("/musicproxy/v1/artist-art?artist=sia&album=fear&size=300&key=0f450aa882a6125ebcbfb3d7f7aa25bc") ==
        true);

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    QVERIFY(spy.count() == 1);

    // check the arguments of the signal.
    // With this we check that the api_key is OK and that the url are build as
    // expected.
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString().endsWith(
                 "/musicproxy/v1/artist-art?artist=sia&album=fear&size=300&key=0f450aa882a6125ebcbfb3d7f7aa25bc"),
             true);
    // Finally check the content of the file downloaded
    QCOMPARE(arguments.at(1).toString(), QString("SIA_FEAR_TEST_STRING_IMAGE"));
}

TEST_F(TestDownloaderServer, test_not_found)
{
    UbuntuServerDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(download_error(QString const&, QNetworkReply::NetworkError, QString const&)));
    QSignalSpy spy_ok(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download_album("test", "test");
    ASSERT_EQ(
        url.endsWith("/musicproxy/v1/album-art?artist=test&album=test&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc"),
        true);

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);
    // and that the signal for file downloaded successfully is not emitted
    ASSERT_EQ(spy_ok.count(), 0);

    // check the arguments of the signal.
    // With this we check that the api_key is OK and that the url are build as
    // expected.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString().endsWith(
                  "/musicproxy/v1/album-art?artist=test&album=test&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc"),
              true);
    ASSERT_EQ(arguments.at(1).toInt(), static_cast<int>(QNetworkReply::InternalServerError));
    ASSERT_EQ(arguments.at(2).toString().endsWith(
                  "/musicproxy/v1/album-art?artist=test&album=test&size=350&key=0f450aa882a6125ebcbfb3d7f7aa25bc - "
                  "server replied: Internal Server Error"),
              true);
}

TEST_F(TestDownloaderServer, test_threads)
{
    QVector<WorkerThread*> threads;

    int NUM_THREADS = 100;
    for (auto i = 0; i < NUM_THREADS; ++i)
    {
        QString download_id = QString("TEST_%1").arg(i);
        threads.push_back(new WorkerThread(download_id));
    }

    for (auto i = 0; i < NUM_THREADS; ++i)
    {
        threads[i]->start();
    }

    for (auto i = 0; i < NUM_THREADS; ++i)
    {
        threads[i]->wait();
    }

    for (auto th : threads)
    {
        delete th;
    }
}

TEST_F(TestDownloaderServer, test_not_found_url)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader,
                   SIGNAL(download_source_not_found(QString const&, QNetworkReply::NetworkError, QString const&)));
    QSignalSpy spy_ok(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download(QUrl(apiroot_ + "/images_not_found/sia_fear_not_found.png"));
    ASSERT_EQ(url, apiroot_ + "/images_not_found/sia_fear_not_found.png");

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);
    // and that the signal for file downloaded successfully is not emitted
    ASSERT_EQ(spy_ok.count(), 0);

    // check the arguments of the signal.
    // With this we check that the api_key is OK and that the url are build as
    // expected.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString(), apiroot_ + "/images_not_found/sia_fear_not_found.png");
    ASSERT_EQ(arguments.at(1).toInt(), static_cast<int>(QNetworkReply::ContentNotFoundError));
    ASSERT_EQ(
        arguments.at(2).toString().endsWith("images_not_found/sia_fear_not_found.png - server replied: Not Found"),
        true);
}

TEST_F(TestDownloaderServer, test_host_not_found_url)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader,
                   SIGNAL(download_source_not_found(QString const&, QNetworkReply::NetworkError, QString const&)));
    QSignalSpy spy_ok(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download(QUrl("http://www.thishostshouldnotexist.com/file.png"));
    ASSERT_EQ(url, "http://www.thishostshouldnotexist.com/file.png");

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);
    // and that the signal for file downloaded successfully is not emitted
    ASSERT_EQ(spy_ok.count(), 0);

    // check the arguments of the signal.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString(), "http://www.thishostshouldnotexist.com/file.png");
    ASSERT_EQ(arguments.at(1).toInt(), static_cast<int>(QNetworkReply::HostNotFoundError));
    ASSERT_EQ(arguments.at(2).toString(), "Host www.thishostshouldnotexist.com not found");
}

TEST_F(TestDownloaderServer, test_good_url)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download(QUrl(apiroot_ + "/images/sia_fear.png"));
    ASSERT_EQ(url.endsWith("/images/sia_fear.png"), true);

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    // check the arguments of the signal.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString().endsWith("/images/sia_fear.png"), true);
    // Finally check the content of the file downloaded
    ASSERT_EQ(arguments.at(1).toString(), QString("SIA_FEAR_TEST_STRING_IMAGE"));
}

TEST_F(TestDownloaderServer, test_url_parsing_error)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(bad_url_error(QString const&)));

    auto url = downloader.download(QUrl("http://http://www.thishostshouldnotexist.com/file.png"));

    // check that we've got exactly one signal
    // this signal is emitted in the download_url call. that's why we don't wait
    // for it.
    ASSERT_EQ(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString(),
              QString(
                  "Port field was empty; source was \"http://http://www.thishostshouldnotexist.com/file.png\"; scheme "
                  "= \"http\", host = \"http\", path = \"//www.thishostshouldnotexist.com/file.png\""));
}

TEST_F(TestDownloaderServer, test_download_specific_id)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download(QUrl(apiroot_ + "/images/sia_fear.png"), "this_is_the_id_i_want");
    ASSERT_EQ(url, "this_is_the_id_i_want");

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    // check the arguments of the signal.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString(), "this_is_the_id_i_want");
    // Finally check the content of the file downloaded
    ASSERT_EQ(arguments.at(1).toString(), QString("SIA_FEAR_TEST_STRING_IMAGE"));
}

TEST_F(TestDownloaderServer, test_host_not_found_url_specific_id)
{
    QUrlDownloader downloader;

    QSignalSpy spy(&downloader,
                   SIGNAL(download_source_not_found(QString const&, QNetworkReply::NetworkError, QString const&)));
    QSignalSpy spy_ok(&downloader, SIGNAL(file_downloaded(QString const&, QByteArray const&)));

    auto url = downloader.download(QUrl("http://www.thishostshouldnotexist.com/file.png"), "this_is_the_id_i_want");
    ASSERT_EQ(url, "this_is_the_id_i_want");

    // we set a timeout of 5 seconds waiting for the signal to be emitted,
    // which should never be reached
    spy.wait(5000);

    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);
    // and that the signal for file downloaded successfully is not emitted
    ASSERT_EQ(spy_ok.count(), 0);

    // check the arguments of the signal.
    QList<QVariant> arguments = spy.takeFirst();
    ASSERT_EQ(arguments.at(0).toString(), "this_is_the_id_i_want");
    ASSERT_EQ(arguments.at(1).toInt(), static_cast<int>(QNetworkReply::HostNotFoundError));
    ASSERT_EQ(arguments.at(2).toString(), "Host www.thishostshouldnotexist.com not found");
}

int main(int argc, char** argv)
{
    QCoreApplication qt_app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// QTEST_MAIN(TestDownloader)
#include "download.moc"
