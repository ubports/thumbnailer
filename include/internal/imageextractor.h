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
 */

#pragma once

#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>
#include <unity/util/ResourcePtr.h>

#include <chrono>
#include <memory>
#include <string>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class ImageExtractor final : public QObject
{
    Q_OBJECT
public:
    ImageExtractor(int fd, std::chrono::milliseconds timeout);
    ~ImageExtractor();

    ImageExtractor(const ImageExtractor& t) = delete;
    ImageExtractor& operator=(const ImageExtractor& t) = delete;

    void extract();
    std::string data();

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void processFinished();
    void timeout();
    void error();

private:
    unity::util::ResourcePtr<int, void (*)(int)> fd_;
    int timeout_ms_;
    QString exe_path_;
    std::string error_;

    QProcess process_;
    QTimer timer_;
    QTemporaryFile tmpfile_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
