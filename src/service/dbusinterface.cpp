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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include "dbusinterface.h"

#include <internal/file_io.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <thread>

using namespace std;
using namespace unity::thumbnailer::internal;

namespace
{
char const ART_ERROR[] = "com.canonical.Thumbnailer.Error.Failed";
}

namespace unity
{

namespace thumbnailer
{

namespace service
{

namespace
{

// Return a string identifying hardware for which we need to
// set max-extractions to some special value.
// Be careful when making modifications here. We need
// to find a string in cpuinfo that is unique to the specific
// hardwares we care about. For example, the output from
// /proc/cpuinfo is *not* guaranteed to contain a "Hardware :" entry.

#if defined(__arm__) || defined(__PPC__)

string hardware()
{
    string hw;

    string const pattern = R"del([Hh]ardware[ \t]*:(.*))del";
    boost::regex r(pattern);

    string cpuinfo;
    try
    {
        cpuinfo = read_file("/proc/cpuinfo");
    }
    // LCOV_EXCL_START
    catch (runtime_error const& e)
    {
        qWarning() << "DBusInterface(): cannot read /proc/cpuinfo:" << e.what();
        return "";
    }
    // LCOV_EXCL_STOP

    vector<string> lines;
    boost::split(lines, cpuinfo, boost::is_any_of("\n"));
    for (auto const& line : lines)
    {
        boost::smatch hw_match;
        if (boost::regex_match(line, hw_match, r))
        {
            hw = hw_match[1];
            boost::trim(hw);
            break;
        }
    }
    return hw;
}

// TODO: Hack to work around gstreamer problems.
//       See https://bugs.launchpad.net/thumbnailer/+bug/1466273

int adjusted_limit(int limit)
{
    int new_limit = limit;

    // Only adjust if max-extractions is at its default of 0.
    // That allows us to still set it to something else for testing.
    if (limit == 0)
    {
        string hw = hardware();
#if 0
        // TODO: Disabled for now until we can figure out in more
        //       detail how to deal with the gstreamer problems.
        // On BQ (MT6582), we can handle only one vs-thumb at a time.
        new_limit = hw == "MT6582" ? 1 : 2;
#else
        new_limit = 1;
#endif
        qDebug() << "DBusInterface(): adjusted max-extractions to" << new_limit << "for" << QString::fromStdString(hw);
    }
    return new_limit;
}

#else

// Not on Arm or PPC, leave as is.

int adjusted_limit(int limit)
{
    return limit;
}

#endif

}

DBusInterface::DBusInterface(shared_ptr<Thumbnailer> const& thumbnailer,
                             InactivityHandler& inactivity_handler,
                             QObject* parent)
    : QObject(parent)
    , thumbnailer_(thumbnailer)
    , inactivity_handler_(inactivity_handler)
    , check_thread_pool_(make_shared<QThreadPool>())
    , create_thread_pool_(make_shared<QThreadPool>())
    , download_limiter_(settings_.max_downloads())
{
    auto limit = settings_.max_extractions();

    // TODO: Hack to deal with gstreamer problems on (at least) Mako and BQ.
    //       See https://bugs.launchpad.net/thumbnailer/+bug/1466273
    limit = adjusted_limit(limit);

    if (limit == 0)
    {
        limit = std::thread::hardware_concurrency();
        if (limit == 0)
        {
            // If the platform can't tell us how many
            // cores we have, we assume 1.
            limit = 1;  // LCOV_EXCL_LINE
        }
    }

    extraction_limiter_.reset(new RateLimiter(limit));
}

DBusInterface::~DBusInterface()
{
}

CredentialsCache& DBusInterface::credentials()
{
    if (!credentials_)
    {
        credentials_.reset(new CredentialsCache(connection()));
    }
    return *credentials_.get();
}

QDBusUnixFileDescriptor DBusInterface::GetAlbumArt(QString const& artist,
                                                   QString const& album,
                                                   QSize const& requestedSize)
{
    try
    {
        QString details;
        QTextStream s(&details);
        s << "album: " << artist << "/" << album << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
        auto request = thumbnailer_->get_album_art(artist.toStdString(), album.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 download_limiter_, credentials(), inactivity_handler_,
                                 std::move(request), details));
    }
    // LCOV_EXCL_START
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetArtistArt(): " + artist + "/" + album + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, e.what());
    }
    // LCOV_EXCL_STOP
    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor DBusInterface::GetArtistArt(QString const& artist,
                                                    QString const& album,
                                                    QSize const& requestedSize)
{
    try
    {
        QString details;
        QTextStream s(&details);
        s << "artist: " << artist << "/" << album << " (" << requestedSize.width() << "," << requestedSize.height() << ")";
        auto request = thumbnailer_->get_artist_art(artist.toStdString(), album.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 download_limiter_, credentials(), inactivity_handler_,
                                 std::move(request), details));
    }
    // LCOV_EXCL_START
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetArtistArt(): " + artist + "/" + album + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, msg);
    }
    // LCOV_EXCL_STOP
    return QDBusUnixFileDescriptor();
}

QDBusUnixFileDescriptor DBusInterface::GetThumbnail(QString const& filename,
                                                    QSize const& requestedSize)
{
    std::unique_ptr<ThumbnailRequest> request;

    try
    {
        QString details;
        QTextStream s(&details);
        s << "thumbnail: " << filename << " (" << requestedSize.width() << "," << requestedSize.height() << ")";

        auto request = thumbnailer_->get_thumbnail(filename.toStdString(), requestedSize);
        queueRequest(new Handler(connection(), message(),
                                 check_thread_pool_, create_thread_pool_,
                                 *extraction_limiter_, credentials(), inactivity_handler_,
                                 std::move(request), details));
    }
    catch (exception const& e)
    {
        QString msg = "DBusInterface::GetThumbnail(): " + filename + ": " + e.what();
        qWarning() << msg;
        sendErrorReply(ART_ERROR, msg);
    }
    return QDBusUnixFileDescriptor();
}

void DBusInterface::queueRequest(Handler* handler)
{
    requests_.emplace(handler, std::unique_ptr<Handler>(handler));
    connect(handler, &Handler::finished, this, &DBusInterface::requestFinished);
    setDelayedReply(true);

    std::vector<Handler*> &requests_for_key = request_keys_[handler->key()];
    if (requests_for_key.size() == 0)
    {
        /* There are no other concurrent requests for this item, so
         * begin immediately. */
        handler->begin();
    }
    else
    {
        /* There are other requests for this item, so chain this
         * request to wait for them to complete first.  This way we
         * can take advantage of any cached downloads or failures. */
        // TODO: should record time spent in queue
        connect(requests_for_key.back(), &Handler::finished,
                handler, &Handler::begin);
    }
    requests_for_key.push_back(handler);
}

void DBusInterface::requestFinished()
{
    Handler* handler = static_cast<Handler*>(sender());
    try
    {
        auto& h = requests_.at(handler);
        h.release();
        requests_.erase(handler);
    }
    // LCOV_EXCL_START
    catch (std::out_of_range const& e)
    {
        qWarning() << "finished() called on unknown handler" << handler;
    }
    // LCOV_EXCL_STOP

    // Remove ourselves from the chain of requests
    std::vector<Handler*> &requests_for_key = request_keys_[handler->key()];
    requests_for_key.erase(
        std::remove(requests_for_key.begin(), requests_for_key.end(), handler),
        requests_for_key.end());
    if (requests_for_key.size() == 0)
    {
        request_keys_.erase(handler->key());
    }

    // Queue deletion of handler when we re-enter the event loop.
    handler->deleteLater();

    QString msg;
    QTextStream s(&msg);
    s.setRealNumberNotation(QTextStream::FixedNotation);
    s << handler->details() << ": " << double(handler->completion_time().count()) / 1000000;

    auto queued_time = double(handler->queued_time().count()) / 1000000;
    auto download_time = double(handler->download_time().count()) / 1000000;

    if (queued_time > 0 || download_time > 0)
    {
        s << " [";
    }
    if (queued_time > 0)
    {
        s << "q: " << queued_time;
        if (download_time > 0)
        {
            s << ", ";
        }
    }
    if (download_time > 0)
    {
        s << "d: " << download_time;
    }
    if (queued_time > 0 || download_time > 0)
    {
        s << "]";
    }

    s << " sec (" << handler->status() << ")";
    qDebug() << msg;
}

}  // namespace service

}  // namespace thumbnailer

}  // namespace unity
