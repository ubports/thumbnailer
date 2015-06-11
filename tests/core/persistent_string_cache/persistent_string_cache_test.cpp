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

string const test_db = TEST_DIR "/db";

TEST(PersistentStringCache, basic)
{
    unlink_db(test_db);

    {
        // Constructor and move constructor.
        auto c = PersistentStringCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        PersistentStringCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = PersistentStringCache::open(test_db);
        auto c2 = PersistentStringCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    // Tests below are cursory, simply calling each method once.
    // Note: get_or_put() is tested by PersistentStringCacheImpl_test.cpp.
    {
        auto c = PersistentStringCache::open(test_db);

        Optional<string> val;
        Optional<string> metadata;
        Optional<PersistentStringCache::Data> data;

        val = c->get("x");
        EXPECT_FALSE(val);
        data = c->get_data("x");
        EXPECT_FALSE(data);
        metadata = c->get_metadata("x");
        EXPECT_FALSE(metadata);
        EXPECT_FALSE(c->contains_key("x"));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());
        EXPECT_TRUE(c->put("x", ""));
        EXPECT_TRUE(c->put("x", "x", 1));
        EXPECT_TRUE(c->put("x", "y", ""));
        EXPECT_TRUE(c->put("x", "y", 1, "z", 1));
        EXPECT_TRUE(c->put_metadata("x", "z"));
        EXPECT_TRUE(c->put_metadata("x", "z", 1));
        data = c->take_data("x");
        EXPECT_TRUE(data);
        EXPECT_EQ("y", data->value);
        EXPECT_EQ("z", data->metadata);
        PersistentStringCache::Data data2;
        data2 = *data;  // Assignment operator
        EXPECT_EQ("y", data2.value);
        EXPECT_EQ("z", data2.metadata);
        val = c->take("x");
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate("x"));
        EXPECT_FALSE(c->touch("x"));
        c->invalidate();
        c->compact();
        c->put("x", "");
        c->invalidate({"x"});
        EXPECT_FALSE(c->contains_key("x"));
        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto handler = [&](string const&, CacheEvent, PersistentCacheStats const&)
        {
        };

        c->set_handler(AllCacheEvents, handler);
        c->set_handler(CacheEvent::get, handler);
    }
}

TEST(PersistentStringCache, stats)
{
    {
        PersistentCacheStats s;
        EXPECT_EQ("", s.cache_path());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, s.policy());
        EXPECT_EQ(0, s.size());
        EXPECT_EQ(0, s.size_in_bytes());
        EXPECT_EQ(0, s.max_size_in_bytes());
        EXPECT_EQ(0, s.hits());
        EXPECT_EQ(0, s.misses());
        EXPECT_EQ(0, s.hits_since_last_miss());
        EXPECT_EQ(0, s.misses_since_last_hit());
        EXPECT_EQ(0, s.longest_hit_run());
        EXPECT_EQ(0, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_EQ(chrono::system_clock::time_point(), s.most_recent_hit_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.most_recent_miss_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.longest_hit_run_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.longest_miss_run_time());
        auto hist = s.histogram();
        for (unsigned i = 0; i < hist.size(); ++i)
        {
            EXPECT_EQ(0, hist[i]);  // Other bins must still be empty.
        }
    }

    {
        unlink_db(test_db);

        auto c = PersistentStringCache::open(test_db, 1024, CacheDiscardPolicy::lru_ttl);

        // Check that we start out with everything initialized.
        auto s = c->stats();
        EXPECT_EQ(test_db, s.cache_path());
        EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s.policy());
        EXPECT_EQ(0, s.size());
        EXPECT_EQ(0, s.size_in_bytes());
        EXPECT_EQ(1024, s.max_size_in_bytes());
        EXPECT_EQ(0, s.hits());
        EXPECT_EQ(0, s.misses());
        EXPECT_EQ(0, s.hits_since_last_miss());
        EXPECT_EQ(0, s.misses_since_last_hit());
        EXPECT_EQ(0, s.longest_hit_run());
        EXPECT_EQ(0, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_EQ(chrono::system_clock::time_point(), s.most_recent_hit_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.most_recent_miss_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.longest_hit_run_time());
        EXPECT_EQ(chrono::system_clock::time_point(), s.longest_miss_run_time());

        // Incur a miss followed by a hit.
        auto now = chrono::system_clock::now();
        c->put("x", "y");
        EXPECT_FALSE(c->get("y"));
        EXPECT_TRUE(c->get("x"));

        s = c->stats();
        EXPECT_EQ(test_db, s.cache_path());                  // Must not have changed.
        EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s.policy());  // Must not have changed.
        EXPECT_EQ(1, s.size());
        EXPECT_EQ(2, s.size_in_bytes());
        EXPECT_EQ(1024, s.max_size_in_bytes());              // Must not have changed.
        EXPECT_EQ(1, s.hits());
        EXPECT_EQ(1, s.misses());
        EXPECT_EQ(1, s.hits_since_last_miss());
        EXPECT_EQ(0, s.misses_since_last_hit());
        EXPECT_EQ(1, s.longest_hit_run());
        EXPECT_EQ(1, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_NE(chrono::system_clock::time_point(), s.most_recent_hit_time());
        EXPECT_GE(s.most_recent_hit_time(), now);
        EXPECT_NE(chrono::system_clock::time_point(), s.most_recent_miss_time());
        EXPECT_GE(s.most_recent_miss_time(), now);
        EXPECT_NE(chrono::system_clock::time_point(), s.longest_hit_run_time());
        EXPECT_GE(s.longest_hit_run_time(), now);
        EXPECT_NE(chrono::system_clock::time_point(), s.longest_miss_run_time());
        EXPECT_GE(s.longest_miss_run_time(), now);

        auto last_hit_time = s.most_recent_hit_time();
        auto last_miss_time = s.most_recent_miss_time();
        auto hit_run_time = s.longest_hit_run_time();
        auto miss_run_time = s.longest_miss_run_time();

        // Two more hits.
        now = chrono::system_clock::now();
        EXPECT_TRUE(c->get("x"));
        EXPECT_TRUE(c->get("x"));

        s = c->stats();
        EXPECT_EQ(3, s.hits());
        EXPECT_EQ(1, s.misses());
        EXPECT_EQ(3, s.hits_since_last_miss());
        EXPECT_EQ(0, s.misses_since_last_hit());
        EXPECT_EQ(3, s.longest_hit_run());
        EXPECT_EQ(1, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_LE(last_hit_time, s.most_recent_hit_time());
        EXPECT_EQ(last_miss_time, s.most_recent_miss_time());
        EXPECT_LE(hit_run_time, s.longest_hit_run_time());
        EXPECT_EQ(miss_run_time, s.longest_miss_run_time());

        last_hit_time = s.most_recent_hit_time();
        last_miss_time = s.most_recent_miss_time();
        hit_run_time = s.longest_hit_run_time();
        miss_run_time = s.longest_miss_run_time();

        // Four more misses
        now = chrono::system_clock::now();
        EXPECT_FALSE(c->get("y"));
        EXPECT_FALSE(c->get("y"));
        EXPECT_FALSE(c->get("y"));
        EXPECT_FALSE(c->get("y"));

        s = c->stats();
        EXPECT_EQ(3, s.hits());
        EXPECT_EQ(5, s.misses());
        EXPECT_EQ(0, s.hits_since_last_miss());
        EXPECT_EQ(4, s.misses_since_last_hit());
        EXPECT_EQ(3, s.longest_hit_run());
        EXPECT_EQ(4, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_EQ(last_hit_time, s.most_recent_hit_time());
        EXPECT_LE(last_miss_time, s.most_recent_miss_time());
        EXPECT_EQ(hit_run_time, s.longest_hit_run_time());
        EXPECT_LE(miss_run_time, s.longest_miss_run_time());

        last_hit_time = s.most_recent_hit_time();
        last_miss_time = s.most_recent_miss_time();
        hit_run_time = s.longest_hit_run_time();
        miss_run_time = s.longest_miss_run_time();

        // One more hit
        now = chrono::system_clock::now();
        EXPECT_TRUE(c->get("x"));

        s = c->stats();
        EXPECT_EQ(4, s.hits());
        EXPECT_EQ(5, s.misses());
        EXPECT_EQ(1, s.hits_since_last_miss());
        EXPECT_EQ(0, s.misses_since_last_hit());
        EXPECT_EQ(3, s.longest_hit_run());
        EXPECT_EQ(4, s.longest_miss_run());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(0, s.lru_evictions());
        EXPECT_LE(last_hit_time, s.most_recent_hit_time());
        EXPECT_EQ(last_miss_time, s.most_recent_miss_time());
        EXPECT_LE(hit_run_time, s.longest_hit_run_time());
        EXPECT_EQ(miss_run_time, s.longest_miss_run_time());

        last_hit_time = s.most_recent_hit_time();
        last_miss_time = s.most_recent_miss_time();
        hit_run_time = s.longest_hit_run_time();
        miss_run_time = s.longest_miss_run_time();

        {
            // Copy constructor.
            auto s2(s);
            EXPECT_EQ(test_db, s2.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s2.policy());
            EXPECT_EQ(1, s2.size());
            EXPECT_EQ(2, s2.size_in_bytes());
            EXPECT_EQ(1024, s2.max_size_in_bytes());
            EXPECT_EQ(4, s2.hits());
            EXPECT_EQ(5, s2.misses());
            EXPECT_EQ(1, s2.hits_since_last_miss());
            EXPECT_EQ(0, s2.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s2.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s2.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s2.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s2.longest_miss_run_time());
        }

        {
            // Assignment operator.
            PersistentCacheStats s2;
            s2 = s;
            EXPECT_EQ(test_db, s2.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s2.policy());
            EXPECT_EQ(1, s2.size());
            EXPECT_EQ(2, s2.size_in_bytes());
            EXPECT_EQ(1024, s2.max_size_in_bytes());
            EXPECT_EQ(4, s2.hits());
            EXPECT_EQ(5, s2.misses());
            EXPECT_EQ(1, s2.hits_since_last_miss());
            EXPECT_EQ(0, s2.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s2.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s2.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s2.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s2.longest_miss_run_time());
        }

        {
            // Move constructor.
            PersistentCacheStats s2(move(s));
            EXPECT_EQ(test_db, s2.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s2.policy());
            EXPECT_EQ(1, s2.size());
            EXPECT_EQ(2, s2.size_in_bytes());
            EXPECT_EQ(1024, s2.max_size_in_bytes());
            EXPECT_EQ(4, s2.hits());
            EXPECT_EQ(5, s2.misses());
            EXPECT_EQ(1, s2.hits_since_last_miss());
            EXPECT_EQ(0, s2.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s2.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s2.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s2.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s2.longest_miss_run_time());

            s.cache_path();  // Moved-from instance must be usable.

            // Move assignment.
            PersistentCacheStats s3;
            s3 = move(s2);
            EXPECT_EQ(test_db, s3.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s3.policy());
            EXPECT_EQ(1, s3.size());
            EXPECT_EQ(2, s3.size_in_bytes());
            EXPECT_EQ(1024, s3.max_size_in_bytes());
            EXPECT_EQ(4, s3.hits());
            EXPECT_EQ(5, s3.misses());
            EXPECT_EQ(1, s3.hits_since_last_miss());
            EXPECT_EQ(0, s3.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s3.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s3.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s3.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s3.longest_miss_run_time());

            s2.cache_path();  // Moved-from instance must be usable.
        }

        // To get coverage for copying and moving from the internal instance,
        // we need to use an event handler because the event handler is passed the
        // shared_ptr to the internal instance, whereas c->stats() returns a copy.

        auto copy_construct_handler = [&](string const&, CacheEvent, PersistentCacheStats const& s)
        {
            PersistentCacheStats s2(s);
            EXPECT_EQ(test_db, s2.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s2.policy());
            EXPECT_EQ(1, s2.size());
            EXPECT_EQ(2, s2.size_in_bytes());
            EXPECT_EQ(1024, s2.max_size_in_bytes());
            EXPECT_EQ(4, s2.hits());
            EXPECT_EQ(5, s2.misses());
            EXPECT_EQ(1, s2.hits_since_last_miss());
            EXPECT_EQ(0, s2.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s2.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s2.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s2.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s2.longest_miss_run_time());

            EXPECT_EQ(test_db, s.cache_path());  // Source must remain intact.
        };
        c->set_handler(CacheEvent::touch, copy_construct_handler);
        c->touch("x");

        auto move_assign_handler = [&](string const&, CacheEvent, PersistentCacheStats const& s)
        {
            PersistentCacheStats s2;
            s2 = move(s);
            EXPECT_EQ(test_db, s2.cache_path());
            EXPECT_EQ(CacheDiscardPolicy::lru_ttl, s2.policy());
            EXPECT_EQ(1, s2.size());
            EXPECT_EQ(2, s2.size_in_bytes());
            EXPECT_EQ(1024, s2.max_size_in_bytes());
            EXPECT_EQ(4, s2.hits());
            EXPECT_EQ(5, s2.misses());
            EXPECT_EQ(1, s2.hits_since_last_miss());
            EXPECT_EQ(0, s2.misses_since_last_hit());
            EXPECT_EQ(3, s.longest_hit_run());
            EXPECT_EQ(4, s.longest_miss_run());
            EXPECT_EQ(last_hit_time, s2.most_recent_hit_time());
            EXPECT_EQ(last_miss_time, s2.most_recent_miss_time());
            EXPECT_EQ(hit_run_time, s2.longest_hit_run_time());
            EXPECT_EQ(miss_run_time, s2.longest_miss_run_time());

            EXPECT_EQ(test_db, s.cache_path());  // Moved-from instance wasn't really moved from.
        };
        c->set_handler(CacheEvent::touch, move_assign_handler);
        c->touch("x");

        // Move construction is impossible because handlers are passed a const ref
        // to the internal instance.
    }

    {
        unlink_db(test_db);

        auto c = PersistentStringCache::open(test_db, 1024, CacheDiscardPolicy::lru_ttl);

        // Check that eviction counts are correct.
        c->put("1", string(200, 'a'));
        c->put("2", string(200, 'a'));
        c->put("3", string(200, 'a'));
        c->put("4", string(200, 'a'));
        c->put("5", string(200, 'a'));

        // Cache almost full now (1005 bytes). Adding a 401-byte record must evict two entries.
        c->put("6", string(400, 'a'));
        EXPECT_EQ(1004, c->size_in_bytes());
        auto s = c->stats();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(2, s.lru_evictions());

        // Add two records that expire in 500ms. These must evict two more entries.
        auto later = chrono::system_clock::now() + chrono::milliseconds(500);
        c->put("7", string(200, 'a'), later);
        c->put("8", string(200, 'a'), later);
        EXPECT_EQ(1004, c->size_in_bytes());
        s = c->stats();
        EXPECT_EQ(4, s.size());
        EXPECT_EQ(0, s.ttl_evictions());
        EXPECT_EQ(4, s.lru_evictions());

        // Wait until records have expired.
        while (chrono::system_clock::now() <= later)
        {
            this_thread::sleep_for(chrono::milliseconds(5));
        }

        // Add a single record. That must evict both expired entries, even though evicting one would be enough.
        c->put("9", string(300, 'a'));
        EXPECT_EQ(903, c->size_in_bytes());
        s = c->stats();
        EXPECT_EQ(3, s.size());
        EXPECT_EQ(2, s.ttl_evictions());
        EXPECT_EQ(4, s.lru_evictions());
    }
}
