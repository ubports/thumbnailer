/*
 * Copyright (C) 2014 Canonical Ltd.
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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <internal/ubuntuserverdownloader.h>
#include <internal/artreply.h>
#include "utils/artserver.h"
#include <testsetup.h>

#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

#include <chrono>
#include <thread>
#include <vector>

using namespace unity::thumbnailer::internal;

class TestDownloaderServer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        fake_art_server_.reset(new ArtServer());
        apiroot_ = QString::fromStdString(fake_art_server_->apiroot());
    }

    void TearDown() override
    {
        fake_art_server_.reset();
    }

    std::unique_ptr<ArtServer> fake_art_server_;
    QString apiroot_;
};

// Time to wait for an expected signal to arrive. The wait()
// calls on the spy should always report success before this.
int const SIGNAL_WAIT_TIME = 5000;

auto const DOWNLOAD_TIMEOUT = std::chrono::milliseconds(10000);

TEST_F(TestDownloaderServer, test_download_album_url)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_album("sia", "fear", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QUrl check_url(reply->url_string());
    QUrlQuery url_query(check_url.query());
    EXPECT_EQ(url_query.queryItemValue("artist"), "sia");
    EXPECT_EQ(url_query.queryItemValue("album"), "fear");
    EXPECT_EQ(url_query.queryItemValue("size"), "");
    EXPECT_EQ(check_url.path(), "/musicproxy/v1/album-art");
    qDebug() << check_url.toString();
    EXPECT_TRUE(check_url.toString().startsWith(apiroot_));
}

TEST_F(TestDownloaderServer, test_download_artist_url)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_artist("sia", "fear", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QUrl check_url(reply->url_string());
    QUrlQuery url_query(check_url.query());
    EXPECT_EQ(url_query.queryItemValue("artist"), "sia");
    EXPECT_EQ(url_query.queryItemValue("album"), "fear");
    EXPECT_EQ(url_query.queryItemValue("size"), "");
    EXPECT_EQ(check_url.path(), "/musicproxy/v1/artist-art");
    EXPECT_TRUE(check_url.toString().startsWith(apiroot_));
}

TEST_F(TestDownloaderServer, test_ok_album)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_album("sia", "fear", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QSignalSpy spy(reply.get(), &ArtReply::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_EQ(reply->succeeded(), true);
    EXPECT_EQ(reply->not_found_error(), false);
    EXPECT_EQ(reply->is_running(), false);
    EXPECT_EQ(reply->network_down(), false);
    // Finally check the content of the file downloaded
    EXPECT_EQ(QString(reply->data()), QString("SIA_FEAR_TEST_STRING_IMAGE_ALBUM"));
}

TEST_F(TestDownloaderServer, test_ok_artist)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_artist("sia", "fear", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QSignalSpy spy(reply.get(), &ArtReply::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_EQ(reply->succeeded(), true);
    EXPECT_EQ(reply->not_found_error(), false);
    EXPECT_EQ(reply->is_running(), false);
    EXPECT_EQ(reply->network_down(), false);
    EXPECT_EQ(QString(reply->data()), QString("SIA_FEAR_TEST_STRING_IMAGE"));
}

TEST_F(TestDownloaderServer, test_timeout)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_artist("sleep", "4", std::chrono::milliseconds(1000));
    ASSERT_NE(reply, nullptr);

    QSignalSpy spy(reply.get(), &ArtReply::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_EQ(reply->succeeded(), false);
    EXPECT_EQ(reply->not_found_error(), false);
    EXPECT_EQ(reply->is_running(), false);
    EXPECT_EQ(reply->network_down(), true);
}

TEST_F(TestDownloaderServer, test_not_found)
{
    UbuntuServerDownloader downloader;

    auto reply = downloader.download_album("test", "test", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QSignalSpy spy(reply.get(), &ArtReply::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_EQ(reply->succeeded(), false);
    EXPECT_EQ(reply->not_found_error(), true);
    EXPECT_EQ(reply->is_running(), false);
    EXPECT_EQ(reply->network_down(), false);
    EXPECT_TRUE(reply->error_string().endsWith("server replied: Not Found"));
}

TEST_F(TestDownloaderServer, test_multiple_downloads)
{
    UbuntuServerDownloader downloader;

    std::vector<std::pair<std::shared_ptr<ArtReply>, std::shared_ptr<QSignalSpy>>> replies;

    int NUM_DOWNLOADS = 100;
    for (auto i = 0; i < NUM_DOWNLOADS; ++i)
    {
        QString download_id = QString("TEST_%1").arg(i);
        auto reply = downloader.download_album("test_threads", download_id, DOWNLOAD_TIMEOUT);
        ASSERT_NE(reply, nullptr);
        std::shared_ptr<QSignalSpy> spy(new QSignalSpy(reply.get(), &ArtReply::finished));
        replies.push_back(std::make_pair(reply, spy));
    }

    for (auto i = 0; i < NUM_DOWNLOADS; ++i)
    {
        if (!replies[i].second->count())
        {
            // if it was not called yet, wait for it
            ASSERT_TRUE(replies[i].second->wait());
        }
        ASSERT_EQ(replies[i].second->count(), 1);
        EXPECT_EQ(replies[i].first->succeeded(), true);
        EXPECT_EQ(replies[i].first->not_found_error(), false);
        EXPECT_EQ(replies[i].first->is_running(), false);
        EXPECT_EQ(replies[i].first->network_down(), false);
        // Finally check the content of the file downloaded
        EXPECT_EQ(QString(replies[i].first->data()), QString("TEST_THREADS_TEST_TEST_%1").arg(i));
    }
}

TEST_F(TestDownloaderServer, test_connection_error)
{
    UbuntuServerDownloader downloader;

    auto network_manager = downloader.network_manager();

    // disable connection before executing any query
    network_manager->setNetworkAccessible(QNetworkAccessManager::NotAccessible);

    auto reply = downloader.download_artist("sia", "fear", DOWNLOAD_TIMEOUT);
    ASSERT_NE(reply, nullptr);

    QSignalSpy spy(reply.get(), &ArtReply::finished);
    ASSERT_TRUE(spy.wait(SIGNAL_WAIT_TIME));
    // check that we've got exactly one signal
    ASSERT_EQ(spy.count(), 1);

    EXPECT_EQ(reply->succeeded(), false);
    EXPECT_EQ(reply->not_found_error(), false);
    EXPECT_EQ(reply->is_running(), false);
    EXPECT_EQ(reply->network_down(), true);
}

int main(int argc, char** argv)
{
    QCoreApplication qt_app(argc, argv);
    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
