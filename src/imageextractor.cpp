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
#include <internal/env_vars.h>
#include <internal/safe_strerror.h>

#include <QDebug>
#include <QUrl>

#include <cassert>

#include <fcntl.h>

using namespace std;
using namespace unity::thumbnailer::internal;

ImageExtractor::ImageExtractor(std::string const& filename, chrono::milliseconds timeout)
    : filename_(filename)
    , timeout_ms_(timeout.count())
    , read_called_(false)
    , pipe_read_fd_(::close)
    , pipe_write_fd_(::close)
{
    process_.setStandardInputFile(QProcess::nullDevice());
    process_.setProcessChannelMode(QProcess::ForwardedOutputChannel);
    process_.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&process_, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &ImageExtractor::processFinished);
    connect(&process_, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
            this, &ImageExtractor::error);
    connect(&process_, &QProcess::started, this, &ImageExtractor::started);
    connect(&timer_, &QTimer::timeout, this, &ImageExtractor::timeout);

    // We set up a pipe to receive the image from vs-thumb. Because some gstreamer
    // codecs chatter on stdout, we can't use stdout to transfer the image.
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        throw runtime_error(string("ImageExtractor(): cannot create pipe: ") + safe_strerror(errno));  // LCOV_EXCL_LINE
    }
    pipe_read_fd_.reset(pipe_fd[0]);
    pipe_write_fd_.reset(pipe_fd[1]);
    if (fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK) == -1)
    {
        throw runtime_error(string("ImageExtractor(): fcntl failed: ") + safe_strerror(errno));  // LCOV_EXCL_LINE
    }

    // Make notifiers to fire on ready to read and error.
    read_notifier_.reset(new QSocketNotifier(pipe_fd[0], QSocketNotifier::Read));
    connect(read_notifier_.get(), &QSocketNotifier::activated, this, &ImageExtractor::readFromPipe);
    error_notifier_.reset(new QSocketNotifier(pipe_fd[0], QSocketNotifier::Exception));
    connect(error_notifier_.get(), &QSocketNotifier::activated, this, &ImageExtractor::pipeError);
}

ImageExtractor::~ImageExtractor()
{
    if (process_.state() != QProcess::NotRunning)
    {
        // LCOV_EXCL_START
        process_.kill();
        if (!process_.waitForFinished(timeout_ms_))
        {
            qWarning().nospace() << "~ImageExtractor(): " << exe_path_ << " (pid" << process_.pid()
                                 << ") did not exit after " << timeout_ms_ << " milliseconds";
        }
        // LCOV_EXCL_STOP
    }
}

void ImageExtractor::extract()
{
    // Gstreamer video pipelines are unstable so we need to run an
    // external helper library.
    char* utildir = getenv(UTIL_DIR);
    exe_path_ = utildir ? utildir : SHARE_PRIV_ABS;
    exe_path_ += QLatin1String("/vs-thumb");
    QUrl url;
    url.setScheme("fd");
    url.setPath(to_string(pipe_write_fd_.get()).c_str());
    process_.start(exe_path_, {QString::fromStdString(filename_), url.toString()});

    // Set a watchdog timer in case vs-thumb doesn't finish in time.
    timer_.setSingleShot(true);
    timer_.start(timeout_ms_);
}

QByteArray ImageExtractor::read()
{
    if (!error_.empty())
    {
        throw runtime_error(string("ImageExtractor::read(): ") + error_);
    }
    assert(!read_called_);
    read_called_ = true;
    return move(image_data_);
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
                    // The finished signal may arrive while there is still
                    // some unread data in the pipe, so do one more read.
                    readFromPipe();
                    error_ = "";
                    break;
                case 1:
                    error_ = string("no artwork for ") + filename_;
                    break;
                case 2:
                    error_ = string("extractor pipeline failed for ") + filename_;
                    break;
                default:
                    error_ = string("unknown exit status ") + to_string(process_.exitCode()) +
                             " from " + exe_path_.toStdString() + " for " + filename_;
                    break;
            }
            break;
        case QProcess::CrashExit:
            if (error_.empty())
            {
                // Conditional because, if we get a timeout and send a kill,
                // we don't want to overwrite the message set by timeout().
                error_ = exe_path_.toStdString() + " crashed";
            }
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
    error_ = exe_path_.toStdString() + " (pid " + to_string(process_.pid()) + ") did not return after " +
             to_string(timeout_ms_) + " milliseconds";
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

void ImageExtractor::started()
{
    // We don't need the write end of the pipe.
    pipe_write_fd_.dealloc();
}

// Read data from pipe (non-blocking) and append it to image_data_.

void ImageExtractor::readFromPipe()
{
    if (!error_.empty())
    {
        return;
    }

    char buf[16 * 1024];  // No point in making this larger, pipe can hold at most 16 KB.
    int rc;

try_again:
    while ((rc = ::read(pipe_read_fd_.get(), buf, sizeof(buf))) > 0)
    {
        image_data_.append(buf, rc);
    }
    if (rc == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;  // EOF
        }
        if (errno != EINTR)
        {
            // LCOV_EXCL_START
            error_ = string("cannot read remaining bytes from pipe: ") + safe_strerror(errno);
            return;
            // LCOV_EXCL_STOP
        }
        assert(errno == EINTR);
        goto try_again;
    }
    assert(rc == 0);  // No data available for now.
}

// LCOV_EXCL_START
void ImageExtractor::pipeError()
{
    error_ = "broken pipe";
}
// LCOV_EXCL_STOP
