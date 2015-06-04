/*
 * Copyright (C) 2013 Canonical Ltd.
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
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <internal/gobj_memory.h>
#include <glib-object.h>
#include <gtest/gtest.h>

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(Gobj_ptr, trivial)
{
    gobj_ptr<GObject> basic(G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr)));
    EXPECT_TRUE(bool(basic));
    EXPECT_TRUE(G_IS_OBJECT(basic.get()));
}

TEST(Gobj_ptr, compare)
{
    GObject* o1 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GObject* o2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    if (o1 > o2)
    {
        std::swap(o1, o2);
    }
    ASSERT_TRUE(o1 < o2);
    gobj_ptr<GObject> u1(o1);
    gobj_ptr<GObject> u2(o2);

    EXPECT_TRUE(!(u1 == nullptr));
    EXPECT_TRUE(u1 != nullptr);
    EXPECT_TRUE(u1 != u2);
    EXPECT_TRUE(!(u1 == u2));
    EXPECT_TRUE(u1 < u2);
    EXPECT_TRUE(!(u2 < u1));
    EXPECT_TRUE(!(u1 == u2));
    EXPECT_TRUE(!(u2 == u1));
    EXPECT_TRUE(u1 <= u2);
    EXPECT_TRUE(!(u2 <= u1));
}

// This is its own thing due to need to avoid double release.

TEST(Gobj_ptr, equality)
{
    GObject* o = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u1(o);
    g_object_ref(o);
    gobj_ptr<GObject> u2(o);
    EXPECT_TRUE(u1 == u2);
    EXPECT_TRUE(u2 == u1);
    EXPECT_TRUE(!(u1 != u2));
    EXPECT_TRUE(!(u2 != u1));
}

TEST(Gobj_ptr, release)
{
    GObject* o = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u(o);
    EXPECT_TRUE(u != nullptr);
    EXPECT_TRUE(u.get() != nullptr);
    EXPECT_TRUE(o == u.release());
    EXPECT_TRUE(!u);
    EXPECT_TRUE(u.get() == nullptr);
    g_object_unref(o);
}

TEST(Gobj_ptr, refcount)
{
    GObject* o = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    EXPECT_EQ(1, o->ref_count);
    g_object_ref(o);

    {
        EXPECT_EQ(2, o->ref_count);
        gobj_ptr<GObject> u(o);
        EXPECT_EQ(2, o->ref_count);
        // Now it dies and refcount is reduced.
    }

    EXPECT_EQ(1, o->ref_count);
    g_object_unref(o);
}

TEST(Gobj_ptr, copy)
{
    GObject* o = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u(o);
    EXPECT_EQ(1, u->ref_count);
    gobj_ptr<GObject> u2(u);
    EXPECT_EQ(2, u->ref_count);
    gobj_ptr<GObject> u3 = u2;
    EXPECT_EQ(3, u->ref_count);
    u3.reset();
    u2.reset();
    EXPECT_EQ(1, u->ref_count);
}

TEST(Gobj_ptr, swap)
{
    GObject* o1 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GObject* o2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u1(o1);
    gobj_ptr<GObject> u2(o2);

    u1.swap(u2);
    EXPECT_EQ(o2, u1.get());
    EXPECT_EQ(o1, u2.get());

    std::swap(u1, u2);
    EXPECT_EQ(o1, u1.get());
    EXPECT_EQ(o2, u2.get());
}

TEST(Gobj_ptr, floating)
{
    GObject* o = G_OBJECT(g_object_new(G_TYPE_INITIALLY_UNOWNED, nullptr));
    try
    {
        gobj_ptr<GObject> u(o);
        FAIL();
    }
    catch (const invalid_argument& c)
    {
        EXPECT_EQ("Tried to add a floating gobject into a gobj_ptr.", string(c.what()));
    }
    // Object accepted after sinking.
    g_object_ref_sink(o);
    gobj_ptr<GObject> u(o);
}

TEST(Gobj_ptr, move)
{
    GObject* o1 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GObject* o2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    g_object_ref(o1);
    gobj_ptr<GObject> u1(o1);
    gobj_ptr<GObject> u2(o2);
    u1 = std::move(u2);
    EXPECT_TRUE(u1.get() == o2);
    EXPECT_TRUE(!u2);
    EXPECT_TRUE(o1->ref_count == 1);
    g_object_unref(o1);
}

TEST(Gobj_ptr, null)
{
    GObject* o1 = NULL;
    GObject* o3 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u1(o1);
    gobj_ptr<GObject> u2(nullptr);
    gobj_ptr<GObject> u3(o3);

    EXPECT_TRUE(!u1);
    EXPECT_TRUE(!u2);
    u3 = nullptr;
    EXPECT_TRUE(!u3);
}

TEST(Gobj_ptr, reset)
{
    GObject* o1 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GObject* o2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    gobj_ptr<GObject> u(o1);

    u.reset(o2);
    EXPECT_EQ(o2, u.get());
    u.reset(nullptr);
    EXPECT_TRUE(!u);
}

TEST(Gobj_ptr, sizeoftest)
{
    EXPECT_EQ(sizeof(GObject*), sizeof(gobj_ptr<GObject>));
}

TEST(Gobj_ptr, deleter)
{
    gobj_ptr<GObject> u1;
    EXPECT_TRUE(g_object_unref == u1.get_deleter());
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#pragma GCC diagnostic pop
