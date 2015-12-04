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
#include <QTimer>

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
    ImageExtractor(std::string const& filename, std::chrono::milliseconds timeout);
    ~ImageExtractor();

    ImageExtractor(ImageExtractor const& t) = delete;
    ImageExtractor& operator=(ImageExtractor const& t) = delete;

    void extract();
    QByteArray read();

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void processFinished();
    void timeout();
    void error();

private:
    std::string const filename_;
    int const timeout_ms_;
    bool read_called_;
    QString exe_path_;
    std::string error_;

    QProcess process_;
    QTimer timer_;
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
