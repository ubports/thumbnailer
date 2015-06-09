/*
 * Copyright (C) 2015 Canonical Ltd
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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <core/persistent_string_cache.h>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

using namespace std;
using namespace core;

// Removes the contents of db_dir, but not db_dir itself.

void unlink_db(string const& db_dir)
{
    namespace fs = boost::filesystem;
    try
    {
        for (fs::directory_iterator end, it(db_dir); it != end; ++it)
        {
            remove_all(it->path());
        }
    }
    catch (...)
    {
    }
}

string const test_db = TEST_DIR "/perf";

char random_char()
{
    static auto seed = random_device()();
    static mt19937 engine(seed);
    static uniform_int_distribution<int> uniform_dist(0, 255);
    return uniform_dist(engine);
}

int random_int(int min, int max)
{
    static auto seed = random_device()();
    static mt19937 engine(seed);
    uniform_int_distribution<int> uniform_dist(min, max);
    return uniform_dist(engine);
}

int random_size(double mean, double stddev, int64_t min, int64_t max)
{
    static auto seed = random_device()();
    static mt19937 engine(seed);
    normal_distribution<double> normal_dist(mean, stddev);

    double val = round(normal_dist(engine));
    return val < min ? min : (val > max ? max : val);
}

string const& random_string(int size)
{
    assert(size >= 0);

    static string s;
    s.resize(size);
    for (auto i = 0; i < size; ++i)
    {
        s[i] = random_char();
    }
    return s;
}

// Returns random key padded with zeros to keylen.

string make_key(int max_key, int keylen)
{
    static ostringstream s;
    s << setfill('0') << setw(keylen) << random_int(0, max_key);
    string key = s.str();
    s.clear();
    s.str("");
    return key;
}

TEST(PersistentStringCache, basic)
{
    double const kB = 1024.0;
    double const MB = kB * 1024;

    // Adjustable parameters

    int64_t const max_cache_size = 100 * MB;
    int const record_size = 20 * kB;
    double const hit_rate = 0.8;
    int const iterations = 10000;
    int keylen = 60;
    double const stddev = record_size / 3.0;
    auto const cost_of_miss = chrono::microseconds(0);

    // End adjustable parameters

    int64_t const num_records = max_cache_size / record_size;
    int const max_key = ((1 - hit_rate) + 1) * num_records - 1;

    unlink_db(test_db);
    auto c = PersistentStringCache::open(test_db, max_cache_size, CacheDiscardPolicy::lru_only);

    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(3);

    cout << "Cache size:     " << max_cache_size / MB << " MB" << endl;
    cout << "Records:        " << num_records << endl;
    cout << "Record size:    " << record_size / kB << " kB" << endl;
    cout << "Std. deviation: " << stddev << endl;
    cout << "Key length:     " << keylen << endl;
    cout << "Hit rate:       " << hit_rate << endl;
    cout << "Cost of miss:   "
         << (cost_of_miss.count()  == 0
             ? 0.0
             : chrono::duration_cast<chrono::microseconds>(cost_of_miss).count() / 1000.0)
         << " ms" << endl;
    cout << "Iterations:     " << iterations << endl;

    static Optional<string> val;

    auto start = chrono::system_clock::now();
    for (int i = 0; i < num_records; ++i)
    {
        static ostringstream s;
        s << setfill('0') << setw(keylen) << i;
        string key = s.str();
        s.clear();
        s.str("");
        string const& val = random_string(random_size(record_size, stddev, 0, max_cache_size));
        c->put(key, val);
    }
    auto now = chrono::system_clock::now();
    double secs = chrono::duration_cast<chrono::milliseconds>(now - start).count() / 1000.0;
    cout << "Cache full, inserted " << (num_records * record_size) / MB << " MB in " << secs << " seconds ("
         << c->size_in_bytes() / MB / secs << " MB/sec)" << endl;

    int64_t bytes_read = 0;
    int64_t bytes_written = 0;
    c->clear_stats();
    start = chrono::system_clock::now();
    for (auto i = 0; i < iterations; ++i)
    {
        string key = make_key(max_key, keylen);
        val = c->get(key);
        if (!val)
        {
            string const& new_val = random_string(random_size(record_size, stddev, 0, max_cache_size));
            this_thread::sleep_for(cost_of_miss);
            c->put(key, new_val);
            bytes_written += new_val.size();
        }
        else
        {
            bytes_read += val->size();
        }
    }

    now = chrono::system_clock::now();
    secs = chrono::duration_cast<chrono::milliseconds>(now - start).count() / 1000.0;
    cout << endl;
    cout << "Performed " << iterations << " lookups with " << hit_rate * 100 << "% hit rate in " << secs << " seconds."
         << endl;
    cout << "Read:           " << bytes_read / MB << " MB (" << bytes_read / MB / secs << " MB/sec)" << endl;
    cout << "Wrote:          " << bytes_written / MB << " MB (" << bytes_written / MB / secs << " MB/sec)" << endl;
    auto sum = bytes_read + bytes_written;
    cout << "Total:          " << sum / MB << " MB (" << sum / MB / secs << " MB/sec)" << endl;
    cout << "Records/sec:    " << iterations / secs << endl;
    auto s = c->stats();
    cout << "Hits:           " << s.hits() << endl;
    cout << "Misses:         " << s.misses() << endl;
    cout << "Evictions:      " << s.lru_evictions() << endl;
    cout << "Disk size:      " << c->disk_size_in_bytes() / MB << " MB" << endl;

    cout << endl
         << "Compacting cache... " << flush;
    start = chrono::system_clock::now();
    c.compact();
    now = chrono::system_clock::now();
    secs = chrono::duration_cast<chrono::milliseconds>(now - start).count() / 1000;
    cout << "done" << endl;
    cout << "Time:          " << secs << " sec" << endl;

    {
        auto c = PersistentStringCache::open(test_db);
        cout << "New size:      " << c->disk_size_in_bytes() / MB << " MB" << endl;
    }

    unlink_db(test_db);  // Reclaim disk space
}
