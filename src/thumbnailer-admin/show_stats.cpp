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

#include "show_stats.h"

#include <core/persistent_cache_stats.h>

#include <cassert>
#include <ctime>
#include <iomanip>
#include <iostream>

#include <inttypes.h>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace tools
{

ShowStats::ShowStats(QCommandLineParser& parser)
    : Action(parser)
{
    parser.addPositionalArgument(QStringLiteral("stats"), QStringLiteral("Show statistics"), QStringLiteral("stats"));
    parser.addPositionalArgument(QStringLiteral("cache_id"), QStringLiteral("Select cache (i=image, t=thumbnail, f=failure)"), QStringLiteral("[cache_id]"));
    QCommandLineOption histogram_option({"v", "verbose"}, QStringLiteral("Show histogram"));
    parser.addOption(histogram_option);

    if (!parser.parse(QCoreApplication::arguments()))
    {
        throw parser.errorText() + "\n\n" + parser.helpText();
    }
    if (parser.isSet(help_option_))
    {
        throw parser.helpText();
    }

    auto args = parser.positionalArguments();
    assert(args.first() == QString("stats"));
    if (args.size() > 2)
    {
        throw QStringLiteral("too many arguments for stats command") + parser.errorText() + "\n\n" + parser.helpText();
    }
    if (parser.isSet(histogram_option))
    {
        show_histogram_ = true;
    }

    if (args.size() == 2)
    {
        QString arg = args[1];
        if (arg == QLatin1String("i"))
        {
            show_image_stats_ = true;
            show_thumbnail_stats_ = false;
            show_failure_stats_ = false;
        }
        else if (arg == QLatin1String("t"))
        {
            show_image_stats_ = false;
            show_thumbnail_stats_ = true;
            show_failure_stats_ = false;
        }
        else if (arg == QLatin1String("f"))
        {
            show_image_stats_ = false;
            show_thumbnail_stats_ = false;
            show_failure_stats_ = true;
        }
        else
        {
            throw QStringLiteral("invalid cache_id: ") + arg + "\n" + parser.helpText();
        }
    }
}

ShowStats::~ShowStats()
{
}

namespace
{

char const* to_time_string(chrono::system_clock::time_point tp)
{
    if (tp == chrono::system_clock::time_point())
    {
        return "never\n";
    }
    time_t t = chrono::system_clock::to_time_t(tp);
    return asctime(localtime(&t));
}

int display_width(quint32 val)
{
    return log10(val) + 1;
}

void print_entry(int label_width, pair<int32_t, int32_t> bounds, int value_width, quint32 value)
{
    string labels = to_string(bounds.first) + "-" + to_string(bounds.second);
    cout << "        " << setw(label_width * 2 + 1) << labels << ": " << setw(value_width) << value << endl;
}

void show_histogram(QList<quint32> const& h)
{
    int first_slot = -1;    // First non-zero index
    int last_slot = -1;     // Last non-zero index
    quint32 max_count = 0;  // Largest count

    // Find the first and last non-zero slot.
    for (int i = 0; i < h.size(); ++i)
    {
        if (h[i] != 0)
        {
            if (first_slot == -1)
            {
                first_slot = i;
            }
            last_slot = i;
            max_count = max(max_count, h[i]);
        }
    }
    printf("    Histogram:");
    if (first_slot == -1)
    {
        printf("             empty\n");
        return;
    }
    printf("\n");

    // Print the histogram from first non-zero to last non-zero entry.
    auto labels = core::PersistentCacheStats::histogram_bounds();
    int label_width = display_width(labels[last_slot].first);
    int value_width = display_width(max_count);
    for (int i = first_slot; i <= last_slot; ++i)
    {
        print_entry(label_width, labels[i], value_width, h[i]);
    }
}

}  // namespace

void ShowStats::show_stats(service::CacheStats const& st)
{
    printf("    Path:                  %s\n", qPrintable(st.cache_path));
    char const* policy = st.policy == quint32(core::CacheDiscardPolicy::lru_ttl) ? "lru_ttl" : "lru_only";
    printf("    Policy:                %s\n", policy);
    printf("    Size:                  %" PRId64 "\n", int64_t(st.size));
    printf("    Size in bytes:         %" PRId64 "\n", int64_t(st.size_in_bytes));
    printf("    Max size in bytes:     %" PRId64 "\n", int64_t(st.max_size_in_bytes));
    printf("    Hits:                  %" PRId64 "\n", int64_t(st.hits));
    printf("    Misses:                %" PRId64 "\n", int64_t(st.misses));
    printf("    Hits since last miss:  %" PRId64 "\n", int64_t(st.hits_since_last_miss));
    printf("    Misses_since_last_hit: %" PRId64 "\n", int64_t(st.misses_since_last_hit));
    printf("    Longest hit run:       %" PRId64 "\n", int64_t(st.longest_hit_run));
    printf("    Longest miss run:      %" PRId64 "\n", int64_t(st.longest_miss_run));
    printf("    Avg hit run length:    %.02f\n", st.avg_hit_run_length);
    printf("    Avg miss run length:   %.02f\n", st.avg_miss_run_length);
    printf("    TTL evictions:         %" PRId64 "\n", int64_t(st.ttl_evictions));
    printf("    LRU evictions:         %" PRId64 "\n", int64_t(st.lru_evictions));
    printf("    Most-recent hit time:  %s", to_time_string(st.most_recent_hit_time));
    printf("    Most-recent miss time: %s", to_time_string(st.most_recent_miss_time));
    printf("    Longest hit-run time:  %s", to_time_string(st.longest_hit_run_time));
    printf("    Longest miss-run time: %s", to_time_string(st.longest_miss_run_time));

    if (show_histogram_)
    {
        show_histogram(st.histogram);
    }
}

void ShowStats::run(DBusConnection& conn)
{
    qDBusRegisterMetaType<unity::thumbnailer::service::AllStats>();

    auto reply = conn.admin().Stats();
    reply.waitForFinished();
    if (!reply.isValid())
    {
        throw reply.error().message();  // LCOV_EXCL_LINE
    }
    auto st = reply.value();
    if (show_image_stats_)
    {
        printf("%s\n", "Image cache:");
        show_stats(st.full_size_stats);
    }
    if (show_thumbnail_stats_)
    {
        printf("%s\n", "Thumbnail cache:");
        show_stats(st.thumbnail_stats);
    }
    if (show_failure_stats_)
    {
        printf("%s\n", "Failure cache:");
        show_stats(st.failure_stats);
    }
}

}  // namespace tools

}  // namespace thumbnailer

}  // namespace unity
