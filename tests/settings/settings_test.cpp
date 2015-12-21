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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include <internal/env_vars.h>
#include <internal/gobj_memory.h>
#include <settings.h>
#include <testsetup.h>
#include <utils/env_var_guard.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gio/gio.h>
#pragma GCC diagnostic pop
#include <gtest/gtest.h>
#include <QTemporaryDir>

#include <unistd.h>

using namespace unity::thumbnailer;
using namespace unity::thumbnailer::internal;

TEST(Settings, defaults_from_schema)
{
    Settings settings;

    EXPECT_EQ("0f450aa882a6125ebcbfb3d7f7aa25bc", settings.art_api_key());
    EXPECT_EQ(50, settings.full_size_cache_size());
    EXPECT_EQ(100, settings.thumbnail_cache_size());
    EXPECT_EQ(2, settings.failure_cache_size());
    EXPECT_EQ(2, settings.max_downloads());
    EXPECT_EQ(0, settings.max_extractions());
    EXPECT_EQ(10, settings.extraction_timeout());
    EXPECT_EQ(20, settings.max_backlog());
    EXPECT_FALSE(settings.trace_client());
    EXPECT_EQ(1, settings.log_level());
}

TEST(Settings, missing_schema)
{
    // This constructor changes the GSettings schema that is looked
    // up.  This is usually non-sensical, but provides us with a way
    // to test the behaviour when the schema is not correctly
    // installed.
    Settings settings("no.such.schema");

    EXPECT_EQ("0f450aa882a6125ebcbfb3d7f7aa25bc", settings.art_api_key());
    EXPECT_EQ(50, settings.full_size_cache_size());
    EXPECT_EQ(100, settings.thumbnail_cache_size());
    EXPECT_EQ(2, settings.failure_cache_size());
    EXPECT_EQ(1920, settings.max_thumbnail_size());
    EXPECT_EQ(168, settings.retry_not_found_hours());
    EXPECT_EQ(2, settings.retry_error_hours());
    EXPECT_EQ(2, settings.max_downloads());
    EXPECT_EQ(0, settings.max_extractions());
    EXPECT_EQ(10, settings.extraction_timeout());
    EXPECT_EQ(20, settings.max_backlog());
    EXPECT_FALSE(settings.trace_client());
    EXPECT_EQ(1, settings.log_level());
}

TEST(Settings, changed_settings)
{
    gobj_ptr<GSettings> gsettings(g_settings_new("com.canonical.Unity.Thumbnailer"));
    g_settings_set_string(gsettings.get(), "dash-ubuntu-com-key", "foo");
    g_settings_set_int(gsettings.get(), "full-size-cache-size", 41);
    g_settings_set_int(gsettings.get(), "thumbnail-cache-size", 42);
    g_settings_set_int(gsettings.get(), "failure-cache-size", 43);
    g_settings_set_int(gsettings.get(), "max-downloads", 5);
    g_settings_set_int(gsettings.get(), "extraction-timeout", 9);
    g_settings_set_int(gsettings.get(), "max-backlog", 30);
    g_settings_set_boolean(gsettings.get(), "trace-client", true);
    g_settings_set_int(gsettings.get(), "log-level", 2);

    Settings settings;
    EXPECT_EQ("foo", settings.art_api_key());
    EXPECT_EQ(41, settings.full_size_cache_size());
    EXPECT_EQ(42, settings.thumbnail_cache_size());
    EXPECT_EQ(43, settings.failure_cache_size());
    EXPECT_EQ(5, settings.max_downloads());
    EXPECT_EQ(9, settings.extraction_timeout());
    EXPECT_EQ(30, settings.max_backlog());
    EXPECT_TRUE(settings.trace_client());
    EXPECT_EQ(2, settings.log_level());

    g_settings_reset(gsettings.get(), "dash-ubuntu-com-key");
    g_settings_reset(gsettings.get(), "full-size-cache-size");
    g_settings_reset(gsettings.get(), "thumbnail-cache-size");
    g_settings_reset(gsettings.get(), "failure-cache-size");
    g_settings_reset(gsettings.get(), "max-downloads");
    g_settings_reset(gsettings.get(), "extraction_timeout");
    g_settings_reset(gsettings.get(), "max-backlog");
    g_settings_reset(gsettings.get(), "trace-client");
    g_settings_reset(gsettings.get(), "log-level");
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

TEST(Settings, negative_int)
{
    gobj_ptr<GSettings> gsettings(g_settings_new("com.canonical.Unity.Thumbnailer"));
    g_settings_set_int(gsettings.get(), "max-extractions", -1);

    Settings settings;
    try
    {
        settings.max_extractions();
        FAIL();
    }
    catch (std::domain_error const& e)
    {
        EXPECT_STREQ("Settings::get_positive_or_zero_int(): invalid negative value for max-extractions: -1 "
                     "in schema com.canonical.Unity.Thumbnailer",
                     e.what());
    }

    g_settings_reset(gsettings.get(), "max-extractions");
}

TEST(Settings, log_level_env_override)
{
    EnvVarGuard ev_guard(LOG_LEVEL, "0");

    Settings settings;
    EXPECT_EQ(0, settings.log_level());
}

TEST(Settings, log_level_env_bad_setting)
{
    EnvVarGuard ev_guard(LOG_LEVEL, "abc");

    Settings settings;
    EXPECT_EQ(1, settings.log_level());
}

TEST(Settings, log_level_out_of_range)
{
    EnvVarGuard ev_guard(LOG_LEVEL, "3");

    Settings settings;
    EXPECT_EQ(1, settings.log_level());
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
