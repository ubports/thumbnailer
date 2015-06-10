/*
 * Copyright (C) 2013-2015 Canonical Ltd.
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
 * Authored by: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 *              James Henstridge <james.henstridge@canonical.com>
 *              Michi Henning <michi.henning@canonical.com>
 */

#include <internal/imageextractor.h>

#include <internal/config.h>
#include <internal/file_io.h>
#include <internal/raii.h>
#include <internal/safe_strerror.h>

#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace unity::thumbnailer::internal;

ImageExtractor::ImageExtractor(int fd, chrono::milliseconds timeout)
    : fd_(dup(fd), do_close)
    , timeout_ms_(timeout.count())
{
    if (fd_.get() < 0)
    {
        throw runtime_error("ImageExtractor(): could not duplicate fd: " + safe_strerror(errno));
    }

    // We don't allow thumbnailing from anything but a regular file,
    // so the caller can't sneak us a pipe or socket.
    struct stat st;
    if (fstat(fd_.get(), &st) == -1)
    {
        throw runtime_error("VideoScreenshotter(): fstat(): " + safe_strerror(errno));
    }
    if (!(st.st_mode & S_IFREG))
    {
        throw runtime_error("VideoScreenshotter(): fd does not refer to regular file");
    }
    // TODO: Work-around for bug in gstreamer:
    //       http://cgit.freedesktop.org/gstreamer/gstreamer/commit/?id=91f537edf2946cbb5085782693b6c5111333db5f
    //       Pipeline hangs for empty input file. Remove this once the gstreamer fix makes into the archives.
    if (st.st_size == 0)
    {
        throw runtime_error("VideoScreenshotter(): fd refers to empty file");
    }

    process_.setStandardInputFile(QProcess::nullDevice());
    process_.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&process_, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
            &ImageExtractor::processFinished);
    connect(&process_, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this,
            &ImageExtractor::error);
    connect(&timer_, &QTimer::timeout, this, &ImageExtractor::timeout);
}

ImageExtractor::~ImageExtractor()
{
    auto error = process_.error();
    if (error != QProcess::FailedToStart)
    {
        process_.waitForFinished(timeout_ms_);
    }
}

void ImageExtractor::extract()
{
    // Gstreamer video pipelines are unstable so we need to run an
    // external helper library.
    char* utildir = getenv("TN_UTILDIR");
    exe_path_ = utildir ? utildir : SHARE_PRIV_ABS;
    exe_path_ += "/vs-thumb";

    if (!tmpfile_.open())
    {
        throw runtime_error("ImageExtractor::extract(): cannot open " + tmpfile_.fileName().toStdString());
    }
    /* Our duplicated file descriptor does not have the FD_CLOEXEC flag set */
    process_.start(exe_path_, {QString("fd://%1").arg(fd_.get()), tmpfile_.fileName()});
    // Set a watchdog timer in case vs-thumb doesn't finish in time.
    timer_.start(timeout_ms_);
}

string ImageExtractor::data()
{
    if (!error_.empty())
    {
        throw runtime_error(string("ImageExtractor::data(): ") + error_);
    }
    return read_file(tmpfile_.fileName().toStdString());
}

void ImageExtractor::processFinished()
{
    timer_.stop();
    switch (process_.exitStatus())
    {
        case QProcess::NormalExit:
            switch (process_.exitCode())
            {
                case 0:
                    error_ = "";
                    break;
                case 1:
                    error_ = "could not extract screenshot";
                    break;
                case 2:
                    error_ = "extractor pipeline failed";
                    break;
                default:
                    error_ = string("unknown exit status ") + to_string(process_.exitCode()) +
                             " from " + exe_path_.toStdString();
                    break;
            }
            break;
        case QProcess::CrashExit:
            error_ = exe_path_.toStdString() + " crashed";
            break;
        default:
            abort();  // LCOV_EXCL_LINE  // Impossible
    }
    Q_EMIT finished();
}

void ImageExtractor::timeout()
{
    if (process_.state() != QProcess::NotRunning)
    {
        process_.kill();
    }
    error_ = exe_path_.toStdString() + " did not return after " + to_string(timeout_ms_) + " milliseconds";
    Q_EMIT finished();
}

void ImageExtractor::error()
{
    if (process_.error() == QProcess::ProcessError::FailedToStart)
    {
        timer_.stop();
        error_ = string("failed to start ") + exe_path_.toStdString();
        Q_EMIT finished();
    }
}
