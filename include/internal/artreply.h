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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#pragma once

#include <QObject>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

class ArtReply : public QObject
{
    Q_OBJECT
public:
    Q_DISABLE_COPY(ArtReply)

    virtual ~ArtReply() = default;

    virtual bool succeeded() const = 0;
    virtual bool is_running() const = 0;
    virtual QString error_string() const = 0;
    virtual bool not_found_error() const = 0;
    virtual QByteArray const& data() const = 0;
    virtual QString url_string() const = 0;
    virtual bool network_down() const = 0;

Q_SIGNALS:
    void finished();

protected:
    ArtReply(QObject* parent = nullptr)
        : QObject(parent){};
};

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
