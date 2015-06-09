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

#include <core/persistent_cache.h>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace core;

string const test_db = TEST_DIR "/db";

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

// The Person example appears in the doc, so we have it here too. It's nice
// if the examples in the doc actually compile and run...

struct Person
{
    string name;
    int age;
};

namespace core  // Specializations must be placed into namespace core.
{

template <>
string CacheCodec<Person>::encode(Person const& p)
{
    ostringstream s;
    s << p.age << ' ' << p.name;
    return s.str();
}

template <>
Person CacheCodec<Person>::decode(string const& str)
{
    istringstream s(str);
    Person p;
    s >> p.age >> p.name;
    return p;
}

}  // namespace core

TEST(PersistentCache, person_key)
{
    unlink_db(test_db);

    using PersonCache = core::PersistentCache<Person, string>;

    auto c = PersonCache::open("my_db", 1024 * 1024 * 1024, CacheDiscardPolicy::lru_only);

    Person bjarne{"Bjarne Stroustrup", 65};
    c->put(bjarne, "C++ inventor");
    auto value = c->get(bjarne);
    if (value)
    {
        cout << bjarne.name << ": " << *value << endl;
    }
    Person person{"no such person", 0};
    value = c->get(person);
    assert(!value);
}

namespace core
{

template <>
string CacheCodec<int>::encode(int const& value)
{
    return to_string(value);
}

template <>
int CacheCodec<int>::decode(string const& s)
{
    return stoi(s);
}

template <>
string CacheCodec<double>::encode(double const& value)
{
    return to_string(value);
}

template <>
double CacheCodec<double>::decode(string const& s)
{
    return stod(s);
}

template <>
string CacheCodec<char>::encode(char const& value)
{
    return string(1, value);
}

template <>
char CacheCodec<char>::decode(string const& s)
{
    return s.empty() ? '\0' : s[0];
}

}  // namespace core

TEST(PersistentCache, IDCCache)
{
    unlink_db(test_db);

    using IDCCache = PersistentCache<int, double, char>;

    {
        // Constructor and move constructor.
        auto c = IDCCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        IDCCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = IDCCache::open(test_db);
        auto c2 = IDCCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = IDCCache::open(test_db);

        auto val = c->get(1);
        EXPECT_FALSE(val);

        auto data = c->get_data(1);
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key(1));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take(42);
        EXPECT_FALSE(val);

        data = c->take_data(42);
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put(1, 2.0));
        val = c->get(1);
        EXPECT_EQ(2.0, *val);

        data = c->get_data(1);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ('\0', data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate(1));

        EXPECT_TRUE(c->put(1, 2.0));
        data = c->take_data(1);
        EXPECT_TRUE(data);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ('\0', data->metadata);

        EXPECT_TRUE(c->put(2, 3, '4'));
        data = c->take_data(2);
        EXPECT_TRUE(data);
        EXPECT_EQ(3, data->value);
        EXPECT_EQ('4', data->metadata);

        EXPECT_TRUE(c->put(1, 2, '3'));
        EXPECT_TRUE(c->put_metadata(1, '3'));
        data = c->take_data(1);
        EXPECT_EQ(2, data->value);
        EXPECT_EQ('3', data->metadata);

        val = c->take(42);
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate(42));
        EXPECT_FALSE(c->touch(42));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put(1, 0));
        EXPECT_TRUE(c->put(2, 0));
        c->invalidate({1, 2});
        EXPECT_FALSE(c->contains_key(1));
        EXPECT_FALSE(c->contains_key(2));

        EXPECT_TRUE(c->put(3, 0));
        EXPECT_TRUE(c->put(4, 0));
        vector<int> keys = {3, 4};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key(3));
        EXPECT_FALSE(c->contains_key(4));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        // Handlers

        bool handler_called;
        auto handler = [&](int const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(1, 1));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(2, 2));
        EXPECT_TRUE(handler_called);

        // Event operators

        typedef std::underlying_type<CacheEvent>::type EventValue;

        EXPECT_EQ(0x7f, EventValue(AllCacheEvents));
        EXPECT_EQ(0x7e, EventValue(~CacheEvent::get));
        EXPECT_EQ(0x3, EventValue(CacheEvent::get | CacheEvent::put));
        EXPECT_EQ(0x2, EventValue(AllCacheEvents & CacheEvent::put));
        CacheEvent v = CacheEvent::get | CacheEvent::put;
        v |= CacheEvent::invalidate;
        EXPECT_EQ(0x7, EventValue(v));
        v &= ~CacheEvent::get;
        EXPECT_EQ(0x6, EventValue(v));

        // Loader methods

        bool loader_called;
        auto loader = [&](int const& key, IDCCache& c)
        {
            loader_called = true;
            if (key != 99)
            {
                EXPECT_TRUE(c.put(key, 99));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put(3, loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data(4, loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data(99, loader));
    }
}

// Test below go through the seven specializations for the different
// compbinations of custom type and string.

// K = string

TEST(PersistentCache, SDCCache)
{
    unlink_db(test_db);

    using SDCCache = PersistentCache<string, double, char>;

    {
        // Constructor and move constructor.
        auto c = SDCCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        SDCCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = SDCCache::open(test_db);
        auto c2 = SDCCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = SDCCache::open(test_db);

        auto val = c->get("1");
        EXPECT_FALSE(val);

        auto data = c->get_data("1");
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take("42");
        EXPECT_FALSE(val);

        data = c->take_data("42");
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put("1", 2.0));
        val = c->get("1");
        EXPECT_EQ(2.0, *val);

        data = c->get_data("1");
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ('\0', data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate("1"));

        EXPECT_TRUE(c->put("1", 2.0));
        data = c->take_data("1");
        EXPECT_TRUE(data);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ('\0', data->metadata);

        EXPECT_TRUE(c->put("2", 3, '4'));
        data = c->take_data("2");
        EXPECT_TRUE(data);
        EXPECT_EQ(3, data->value);
        EXPECT_EQ('4', data->metadata);

        EXPECT_TRUE(c->put("1", 2, '3'));
        EXPECT_TRUE(c->put_metadata("1", '3'));
        data = c->take_data("1");
        EXPECT_EQ(2, data->value);
        EXPECT_EQ('3', data->metadata);

        val = c->take("42");
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate("42"));
        EXPECT_FALSE(c->touch("42"));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put("1", 0));
        EXPECT_TRUE(c->put("2", 0));
        c->invalidate({"1", "2"});
        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_FALSE(c->contains_key("2"));

        EXPECT_TRUE(c->put("3", 0));
        EXPECT_TRUE(c->put("3", 0));
        vector<string> keys = {{"3"}, {"4"}};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key("3"));
        EXPECT_FALSE(c->contains_key("3"));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](string const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("1", 1));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("2", 2));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](string const& key, SDCCache& c)
        {
            loader_called = true;
            if (key != "99")
            {
                EXPECT_TRUE(c.put(key, 99));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put("3", loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data("4", loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data("99", loader));
    }
}

// V = string

TEST(PersistentCache, ISCCache)
{
    unlink_db(test_db);

    using ISCCache = PersistentCache<int, string, char>;

    {
        // Constructor and move constructor.
        auto c = ISCCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        ISCCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = ISCCache::open(test_db);
        auto c2 = ISCCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = ISCCache::open(test_db);

        auto val = c->get(1);
        EXPECT_FALSE(val);

        auto data = c->get_data(1);
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key(1));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take(42);
        EXPECT_FALSE(val);

        data = c->take_data(42);
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put(1, "2.0"));
        val = c->get(1);
        EXPECT_EQ("2.0", *val);

        data = c->get_data(1);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ('\0', data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate(1));

        EXPECT_TRUE(c->put(1, "2.0"));
        data = c->take_data(1);
        EXPECT_TRUE(data);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ('\0', data->metadata);

        EXPECT_TRUE(c->put(2, string("3"), '4'));
        data = c->take_data(2);
        EXPECT_TRUE(data);
        EXPECT_EQ("3", data->value);
        EXPECT_EQ('4', data->metadata);

        EXPECT_TRUE(c->put(1, string("2"), '3'));
        EXPECT_TRUE(c->put_metadata(1, '3'));
        data = c->take_data(1);
        EXPECT_EQ("2", data->value);
        EXPECT_EQ('3', data->metadata);

        val = c->take(42);
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate(42));
        EXPECT_FALSE(c->touch(42));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put(1, string("0")));
        EXPECT_TRUE(c->put(2, string("0")));
        c->invalidate({1, 2});
        EXPECT_FALSE(c->contains_key(1));
        EXPECT_FALSE(c->contains_key(2));

        EXPECT_TRUE(c->put(3, string("0")));
        EXPECT_TRUE(c->put(4, string("0")));
        vector<int> keys = {3, 4};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key(3));
        EXPECT_FALSE(c->contains_key(4));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](int const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(1, "1"));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(2, "2"));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](int const& key, ISCCache& c)
        {
            loader_called = true;
            if (key != 99)
            {
                EXPECT_TRUE(c.put(key, "99"));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put(3, loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data(4, loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data(99, loader));

        // Extra put() overloads
        c->invalidate();
        string vbuf(20, 'v');

        EXPECT_TRUE(c->put(1, vbuf.data(), vbuf.size()));
        val = c->get(1);
        EXPECT_EQ(vbuf, *val);

        c->invalidate();
        EXPECT_TRUE(c->put(1, vbuf.data(), vbuf.size(), 'm'));
        data = c->get_data(1);
        EXPECT_EQ(vbuf, data->value);
        EXPECT_EQ('m', data->metadata);
    }
}

// M = string

TEST(PersistentCache, IDSCache)
{
    unlink_db(test_db);

    using IDSCache = PersistentCache<int, double, string>;

    {
        // Constructor and move constructor.
        auto c = IDSCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        IDSCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = IDSCache::open(test_db);
        auto c2 = IDSCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = IDSCache::open(test_db);

        auto val = c->get(1);
        EXPECT_FALSE(val);

        auto data = c->get_data(1);
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key(1));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take(42);
        EXPECT_FALSE(val);

        data = c->take_data(42);
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put(1, 2.0));
        val = c->get(1);
        EXPECT_EQ(2.0, *val);

        data = c->get_data(1);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ("\0", data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate(1));

        EXPECT_TRUE(c->put(1, 2.0));
        data = c->take_data(1);
        EXPECT_TRUE(data);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ("\0", data->metadata);

        EXPECT_TRUE(c->put(2, 3, "4"));
        data = c->take_data(2);
        EXPECT_TRUE(data);
        EXPECT_EQ(3, data->value);
        EXPECT_EQ("4", data->metadata);

        EXPECT_TRUE(c->put(1, 2, "3"));
        EXPECT_TRUE(c->put_metadata(1, "3"));
        data = c->take_data(1);
        EXPECT_EQ(2, data->value);
        EXPECT_EQ("3", data->metadata);

        val = c->take(42);
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate(42));
        EXPECT_FALSE(c->touch(42));
        c->invalidate();

        EXPECT_TRUE(c->put(1, 0));
        EXPECT_TRUE(c->put(2, 0));
        c->invalidate({1, 2});
        EXPECT_FALSE(c->contains_key(1));
        EXPECT_FALSE(c->contains_key(2));

        EXPECT_TRUE(c->put(3, 0));
        EXPECT_TRUE(c->put(4, 0));
        vector<int> keys = {3, 4};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key(3));
        EXPECT_FALSE(c->contains_key(4));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](int const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(1, 1));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(2, 2));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](int const& key, IDSCache& c)
        {
            loader_called = true;
            if (key != 99)
            {
                EXPECT_TRUE(c.put(key, 99));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put(3, loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data(4, loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data(99, loader));

        // Extra put() overload
        c->invalidate();
        c->compact();
        string mbuf(20, 'm');

        EXPECT_TRUE(c->put(1, 2.0, mbuf.data(), mbuf.size()));
        data = c->get_data(1);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ(mbuf, data->metadata);

        mbuf = string(10, 'x');
        EXPECT_TRUE(c->put_metadata(1, mbuf.data(), mbuf.size()));
        data = c->get_data(1);
        EXPECT_EQ(mbuf, data->metadata);
    }
}

// K and V = string

TEST(PersistentCache, SSCCache)
{
    unlink_db(test_db);

    using SSCCache = PersistentCache<string, string, char>;

    {
        // Constructor and move constructor.
        auto c = SSCCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        SSCCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = SSCCache::open(test_db);
        auto c2 = SSCCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = SSCCache::open(test_db);

        auto val = c->get("1");
        EXPECT_FALSE(val);

        auto data = c->get_data("1");
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take("42");
        EXPECT_FALSE(val);

        data = c->take_data("42");
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put("1", "2.0"));
        val = c->get("1");
        EXPECT_EQ("2.0", *val);

        data = c->get_data("1");
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ('\0', data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate("1"));

        EXPECT_TRUE(c->put("1", "2.0"));
        data = c->take_data("1");
        EXPECT_TRUE(data);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ('\0', data->metadata);

        EXPECT_TRUE(c->put("2", string("3"), '4'));
        data = c->take_data("2");
        EXPECT_TRUE(data);
        EXPECT_EQ("3", data->value);
        EXPECT_EQ('4', data->metadata);

        EXPECT_TRUE(c->put("1", string("2"), '3'));
        EXPECT_TRUE(c->put_metadata("1", '3'));
        data = c->take_data("1");
        EXPECT_EQ("2", data->value);
        EXPECT_EQ('3', data->metadata);

        val = c->take("42");
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate("42"));
        EXPECT_FALSE(c->touch("42"));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put("1", "0"));
        EXPECT_TRUE(c->put("2", "0"));
        c->invalidate({"1", "2"});
        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_FALSE(c->contains_key("2"));

        EXPECT_TRUE(c->put("3", "0"));
        EXPECT_TRUE(c->put("3", "0"));
        vector<string> keys = {{"3"}, {"4"}};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key("3"));
        EXPECT_FALSE(c->contains_key("3"));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](string const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("1", string("1")));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("2", string("2")));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](string const& key, SSCCache& c)
        {
            loader_called = true;
            if (key != "99")
            {
                EXPECT_TRUE(c.put(key, "99"));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put("3", loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data("4", loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data("99", loader));

        // Extra put() overloads
        c->invalidate();
        string vbuf(20, 'v');

        EXPECT_TRUE(c->put("1", vbuf.data(), vbuf.size()));
        val = c->get("1");
        EXPECT_EQ(vbuf, *val);

        c->invalidate();
        EXPECT_TRUE(c->put("1", vbuf.data(), vbuf.size(), 'm'));
        data = c->get_data("1");
        EXPECT_EQ(vbuf, data->value);
        EXPECT_EQ('m', data->metadata);
    }
}

// K and M = string

TEST(PersistentCache, SDSCache)
{
    unlink_db(test_db);

    using SDSCache = PersistentCache<string, double, string>;

    {
        // Constructor and move constructor.
        auto c = SDSCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        SDSCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = SDSCache::open(test_db);
        auto c2 = SDSCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = SDSCache::open(test_db);

        auto val = c->get("1");
        EXPECT_FALSE(val);

        auto data = c->get_data("1");
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take("42");
        EXPECT_FALSE(val);

        data = c->take_data("42");
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put("1", 2.0));
        val = c->get("1");
        EXPECT_EQ(2.0, *val);

        data = c->get_data("1");
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ("", data->metadata);

        metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate("1"));

        EXPECT_TRUE(c->put("1", 2.0));
        data = c->take_data("1");
        EXPECT_TRUE(data);
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ("", data->metadata);

        EXPECT_TRUE(c->put("2", 3, "4"));
        data = c->take_data("2");
        EXPECT_TRUE(data);
        EXPECT_EQ(3, data->value);
        EXPECT_EQ("4", data->metadata);

        EXPECT_TRUE(c->put("1", 2, "3"));
        EXPECT_TRUE(c->put_metadata("1", "3"));
        data = c->take_data("1");
        EXPECT_EQ(2, data->value);
        EXPECT_EQ("3", data->metadata);

        val = c->take("42");
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate("42"));
        EXPECT_FALSE(c->touch("42"));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put("1", 0));
        EXPECT_TRUE(c->put("2", 0));
        c->invalidate({"1", "2"});
        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_FALSE(c->contains_key("2"));

        EXPECT_TRUE(c->put("3", 0));
        EXPECT_TRUE(c->put("3", 0));
        vector<string> keys = {{"3"}, {"4"}};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key("3"));
        EXPECT_FALSE(c->contains_key("3"));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](string const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("1", 1));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("2", 2));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](string const& key, SDSCache& c)
        {
            loader_called = true;
            if (key != "99")
            {
                EXPECT_TRUE(c.put(key, 99));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put("3", loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data("4", loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data("99", loader));

        // Extra put() overload
        c->invalidate();
        string mbuf(20, 'm');

        EXPECT_TRUE(c->put("1", 2.0, mbuf.data(), mbuf.size()));
        data = c->get_data("1");
        EXPECT_EQ(2.0, data->value);
        EXPECT_EQ(mbuf, data->metadata);

        mbuf = string(10, 'x');
        EXPECT_TRUE(c->put_metadata("1", mbuf.data(), mbuf.size()));
        data = c->get_data("1");
        EXPECT_EQ(mbuf, data->metadata);
    }
}

// V and M = string

TEST(PersistentCache, ISSCache)
{
    unlink_db(test_db);

    using ISSCache = PersistentCache<int, string, string>;

    {
        // Constructor and move constructor.
        auto c = ISSCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        ISSCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = ISSCache::open(test_db);
        auto c2 = ISSCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = ISSCache::open(test_db);

        auto val = c->get(1);
        EXPECT_FALSE(val);

        auto data = c->get_data(1);
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key(1));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take(42);
        EXPECT_FALSE(val);

        data = c->take_data(42);
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put(1, "2.0"));
        val = c->get(1);
        EXPECT_EQ("2.0", *val);

        data = c->get_data(1);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ("\0", data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata(1);
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate(1));

        EXPECT_TRUE(c->put(1, "2.0"));
        data = c->take_data(1);
        EXPECT_TRUE(data);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ("\0", data->metadata);

        EXPECT_TRUE(c->put(2, string("3"), "4"));
        data = c->take_data(2);
        EXPECT_TRUE(data);
        EXPECT_EQ("3", data->value);
        EXPECT_EQ("4", data->metadata);

        EXPECT_TRUE(c->put(1, string("2"), "3"));
        EXPECT_TRUE(c->put_metadata(1, "3"));
        data = c->take_data(1);
        EXPECT_EQ("2", data->value);
        EXPECT_EQ("3", data->metadata);

        val = c->take(42);
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate(42));
        EXPECT_FALSE(c->touch(42));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put(1, string("0")));
        EXPECT_TRUE(c->put(2, string("0")));
        c->invalidate({1, 2});
        EXPECT_FALSE(c->contains_key(1));
        EXPECT_FALSE(c->contains_key(2));

        EXPECT_TRUE(c->put(3, string("0")));
        EXPECT_TRUE(c->put(4, string("0")));
        vector<int> keys = {3, 4};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key(3));
        EXPECT_FALSE(c->contains_key(4));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](int const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(1, "1"));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put(2, "2"));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](int const& key, ISSCache& c)
        {
            loader_called = true;
            if (key != 99)
            {
                EXPECT_TRUE(c.put(key, "99"));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put(3, loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data(4, loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data(99, loader));

        // Extra put() overloads
        c->invalidate();
        string vbuf(20, 'v');
        string mbuf(20, 'm');

        EXPECT_TRUE(c->put(1, vbuf.data(), vbuf.size()));
        val = c->get(1);
        EXPECT_EQ(vbuf, *val);

        c->invalidate();
        EXPECT_TRUE(c->put(1, vbuf.data(), vbuf.size(), mbuf.data(), mbuf.size()));
        data = c->get_data(1);
        EXPECT_EQ(vbuf, data->value);
        EXPECT_EQ(mbuf, data->metadata);

        // Extra put_metadata() overload
        mbuf = string(10, 'x');
        EXPECT_TRUE(c->put_metadata(1, mbuf.data(), mbuf.size()));
        data = c->get_data(1);
        EXPECT_EQ(mbuf, data->metadata);
    }
}

// K, V and M = string

using SSSCache = PersistentCache<string, string, string>;

TEST(PersistentCache, SSSCache)
{
    unlink_db(test_db);

    {
        // Constructor and move constructor.
        auto c = SSSCache::open(test_db, 1024, CacheDiscardPolicy::lru_only);
        SSSCache c2(move(*c));
        EXPECT_EQ(1024, c2.max_size_in_bytes());
    }

    {
        // Constructor and move assignment.
        auto c = SSSCache::open(test_db);
        auto c2 = SSSCache::open(test_db + "2", 2048, CacheDiscardPolicy::lru_ttl);
        *c = move(*c2);
        EXPECT_EQ(2048, c->max_size_in_bytes());
    }

    {
        auto c = SSSCache::open(test_db);

        auto val = c->get("1");
        EXPECT_FALSE(val);

        auto data = c->get_data("1");
        EXPECT_FALSE(data);

        auto metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);

        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_EQ(0, c->size());
        EXPECT_EQ(0, c->size_in_bytes());
        EXPECT_EQ(1024, c->max_size_in_bytes());
        EXPECT_NE(0, c->disk_size_in_bytes());
        EXPECT_EQ(CacheDiscardPolicy::lru_only, c->discard_policy());

        val = c->take("42");
        EXPECT_FALSE(val);

        data = c->take_data("42");
        EXPECT_FALSE(data);

        EXPECT_TRUE(c->put("1", "2.0"));
        val = c->get("1");
        EXPECT_EQ("2.0", *val);

        data = c->get_data("1");
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ("", data->metadata);  // decode() generates '\0' for empty metadata

        metadata = c->get_metadata("1");
        EXPECT_FALSE(metadata);  // No metadata exists, so false

        EXPECT_TRUE(c->invalidate("1"));

        EXPECT_TRUE(c->put("1", "2.0"));
        data = c->take_data("1");
        EXPECT_TRUE(data);
        EXPECT_EQ("2.0", data->value);
        EXPECT_EQ("", data->metadata);

        EXPECT_TRUE(c->put("2", string("3"), "4"));
        data = c->take_data("2");
        EXPECT_TRUE(data);
        EXPECT_EQ("3", data->value);
        EXPECT_EQ("4", data->metadata);

        EXPECT_TRUE(c->put("1", string("2"), "3"));
        EXPECT_TRUE(c->put_metadata("1", "3"));
        data = c->take_data("1");
        EXPECT_EQ("2", data->value);
        EXPECT_EQ("3", data->metadata);

        val = c->take("42");
        EXPECT_FALSE(val);
        EXPECT_FALSE(c->invalidate("42"));
        EXPECT_FALSE(c->touch("42"));
        c->invalidate();
        c->compact();

        EXPECT_TRUE(c->put("1", "0"));
        EXPECT_TRUE(c->put("2", "0"));
        c->invalidate({"1", "2"});
        EXPECT_FALSE(c->contains_key("1"));
        EXPECT_FALSE(c->contains_key("2"));

        EXPECT_TRUE(c->put("3", "0"));
        EXPECT_TRUE(c->put("3", "0"));
        vector<string> keys = {{"3"}, {"4"}};
        c->invalidate(keys);
        EXPECT_FALSE(c->contains_key("3"));
        EXPECT_FALSE(c->contains_key("3"));

        c->clear_stats();
        c->resize(2048);
        c->trim_to(0);

        auto stats = c->stats();
        EXPECT_EQ(0, stats.size());
        EXPECT_EQ(0, stats.size_in_bytes());
        EXPECT_EQ(2048, stats.max_size_in_bytes());

        bool handler_called;
        auto handler = [&](string const&, CacheEvent, PersistentCacheStats const&)
        {
            handler_called = true;
        };

        c->set_handler(AllCacheEvents, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("1", string("1")));
        EXPECT_TRUE(handler_called);

        c->set_handler(CacheEvent::put, handler);
        handler_called = false;
        EXPECT_TRUE(c->put("2", string("2")));
        EXPECT_TRUE(handler_called);

        bool loader_called;
        auto loader = [&](string const& key, SSSCache& c)
        {
            loader_called = true;
            if (key != "99")
            {
                EXPECT_TRUE(c.put(key, "99"));
            }
        };

        loader_called = false;
        EXPECT_TRUE(c->get_or_put("3", loader));

        loader_called = false;
        EXPECT_TRUE(c->get_or_put_data("4", loader));

        loader_called = false;
        EXPECT_FALSE(c->get_or_put_data("99", loader));

        // Extra put() overloads
        c->invalidate();
        string vbuf(20, 'v');
        string mbuf(20, 'm');

        EXPECT_TRUE(c->put("1", vbuf.data(), vbuf.size()));
        val = c->get("1");
        EXPECT_EQ(vbuf, *val);

        c->invalidate();
        EXPECT_TRUE(c->put("1", vbuf.data(), vbuf.size(), mbuf.data(), mbuf.size()));
        data = c->get_data("1");
        EXPECT_EQ(vbuf, data->value);
        EXPECT_EQ(mbuf, data->metadata);

        // Extra put_metadata() overload
        mbuf = string(10, 'x');
        EXPECT_TRUE(c->put_metadata("1", mbuf.data(), mbuf.size()));
        data = c->get_data("1");
        EXPECT_EQ(mbuf, data->metadata);
    }
}
