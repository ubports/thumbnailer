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

#include <internal/trace.h>

#include <QDebug>

#include <chrono>
#include <mutex>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

void trace_message_handler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg)
{
    using namespace std;
    using namespace std::chrono;

    static recursive_mutex mutex;
    lock_guard<decltype(mutex)> lock(mutex);

    auto now = system_clock::now();
    auto sys_time = system_clock::to_time_t(now);
    auto local_time = localtime(&sys_time);
    int msecs = duration_cast<milliseconds>(now.time_since_epoch()).count() - sys_time * 1000;

    char buf[100];
    strftime(buf, sizeof(buf), "%T", local_time);
    fprintf(stderr, "thumbnailer-service: [%s.%03d]", buf, msecs);
    switch (type)
    {
        case QtWarningMsg:
            fputs("Warning:", stderr);
            break;
        case QtCriticalMsg:
            fputs("Critical:", stderr);
            break;
        case QtFatalMsg:
            fputs("Critical:", stderr);
        default:
            ;  // Print nothing for QtDebugMsg.
    }
    fprintf(stderr, " %s\n", msg.toLocal8Bit().constData());
    if (type == QtFatalMsg)
    {
        abort();
    }
}

static int init_count = 0;
static QtMessageHandler old_message_handler;

}  // namespace

TraceMessageHandlerInitializer::TraceMessageHandlerInitializer()
{
    if (init_count++ == 0)
    {
        old_message_handler = qInstallMessageHandler(trace_message_handler);
    }
}

TraceMessageHandlerInitializer::~TraceMessageHandlerInitializer()
{
    if (--init_count == 0)
    {
        qInstallMessageHandler(old_message_handler);
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
