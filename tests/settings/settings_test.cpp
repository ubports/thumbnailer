/*
 * Copyright (C) 2015 Canonical Ltd.
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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include <internal/gobj_memory.h>
#include <internal/settings.h>
#include <testsetup.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gio/gio.h>
#pragma GCC diagnostic pop
#include <gtest/gtest.h>
#include <QTemporaryDir>

#include <unistd.h>

using namespace unity::thumbnailer::internal;

TEST(Settings, defaults_from_schema)
{
    Settings settings;

    EXPECT_EQ("0f450aa882a6125ebcbfb3d7f7aa25bc", settings.art_api_key());
    EXPECT_EQ(50, settings.full_size_cache_size());
    EXPECT_EQ(100, settings.thumbnail_cache_size());
    EXPECT_EQ(2, settings.failure_cache_size());
}

TEST(Settings, missing_schema)
{
    // This constructor changes the GSettings schema that is looked
    // up.  This is usually non-sensical, but provides us with a way
    // to test the behaviour when the schema is not correctly
    // installed.
    Settings settings("no.such.schema");

    EXPECT_EQ("", settings.art_api_key());
    EXPECT_EQ(50, settings.full_size_cache_size());
    EXPECT_EQ(100, settings.thumbnail_cache_size());
    EXPECT_EQ(2, settings.failure_cache_size());
    EXPECT_EQ(1920, settings.max_thumbnail_size());
    EXPECT_EQ(168, settings.retry_not_found_hours());
    EXPECT_EQ(2, settings.retry_error_hours());
}

TEST(Settings, changed_settings)
{
    gobj_ptr<GSettings> gsettings(g_settings_new("com.canonical.Unity.Thumbnailer"));
    g_settings_set_string(gsettings.get(), "dash-ubuntu-com-key", "foo");
    g_settings_set_int(gsettings.get(), "thumbnail-cache-size", 42);

    Settings settings;
    EXPECT_EQ("foo", settings.art_api_key());
    EXPECT_EQ(42, settings.thumbnail_cache_size());

    g_settings_reset(gsettings.get(), "dash-ubuntu-com-key");
    g_settings_reset(gsettings.get(), "thumbnail-cache-size");
}

TEST(Settings, non_positive_int)
{
    gobj_ptr<GSettings> gsettings(g_settings_new("com.canonical.Unity.Thumbnailer"));
    g_settings_set_string(gsettings.get(), "dash-ubuntu-com-key", "foo");
    g_settings_set_int(gsettings.get(), "thumbnail-cache-size", 0);

    Settings settings;
    try
    {
        settings.thumbnail_cache_size();
        FAIL();
    }
    catch (std::domain_error const& e)
    {
        EXPECT_STREQ("Settings::get_positive_int(): invalid zero or negative value for thumbnail-cache-size: 0 "
                     "in schema com.canonical.Unity.Thumbnailer",
                     e.what());
    }

    g_settings_set_int(gsettings.get(), "thumbnail-cache-size", -1);
    try
    {
        settings.thumbnail_cache_size();
        FAIL();
    }
    catch (std::domain_error const& e)
    {
        EXPECT_STREQ("Settings::get_positive_int(): invalid zero or negative value for thumbnail-cache-size: -1 "
                     "in schema com.canonical.Unity.Thumbnailer",
                     e.what());
    }

    g_settings_reset(gsettings.get(), "dash-ubuntu-com-key");
    g_settings_reset(gsettings.get(), "thumbnail-cache-size");
}

int main(int argc, char** argv)
{
    QTemporaryDir tempdir(TESTBINDIR "/settings-test.XXXXXX");
    setenv("XDG_CACHE_HOME", tempdir.path().toUtf8().constData(), true);
    setenv("GSETTINGS_BACKEND", "memory", true);
    setenv("GSETTINGS_SCHEMA_DIR", GSETTINGS_SCHEMA_DIR, true);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
