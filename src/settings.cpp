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

#include <internal/settings.h>

#include <internal/trace.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gio/gio.h>
#pragma GCC diagnostic pop

#include <memory>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

Settings::Settings()
    : Settings("com.canonical.Unity.Thumbnailer")
{
}

Settings::Settings(string const& schema_name)
    : schema_(nullptr, &g_settings_schema_unref)
    , schema_name_(schema_name)
{
    GSettingsSchemaSource* src = g_settings_schema_source_get_default();
    schema_.reset(g_settings_schema_source_lookup(src, schema_name.c_str(), true));
    if (schema_)
    {
        settings_.reset(g_settings_new(schema_name.c_str()));
    }
    else
    {
        qCritical() << "The schema" << schema_name.c_str() << "is missing";
    }
}

Settings::~Settings() = default;

string Settings::art_api_key() const
{
    return get_string("dash-ubuntu-com-key", "");
}

int Settings::full_size_cache_size() const
{
    return get_positive_int("full-size-cache-size", 50);
}

int Settings::thumbnail_cache_size() const
{
    return get_positive_int("thumbnail-cache-size", 100);
}

int Settings::failure_cache_size() const
{
    return get_positive_int("failure-cache-size", 2);
}

int Settings::max_thumbnail_size() const
{
    return get_positive_int("max-thumbnail-size", 1920);
}

int Settings::retry_not_found_hours() const
{
    return get_positive_int("retry-not-found-hours", 24 * 7);
}

int Settings::retry_error_hours() const
{
    return get_positive_int("retry-error-hours", 2);
}

int Settings::max_downloads() const
{
    return get_positive_int("max-downloads", 2);
}

int Settings::max_extractions() const
{
    return get_positive_int("max-extractions", 2);
}

string Settings::get_string(char const* key, string const& default_value) const
{
    if (!settings_ || !g_settings_schema_has_key(schema_.get(), key))
    {
        return default_value;
    }

    char *value = g_settings_get_string(settings_.get(), key);
    if (value)
    {
        string result = value;
        g_free(value);
        return result;
    }
    return default_value; // LCOV_EXCL_LINE
}

int Settings::get_positive_int(char const* key, int default_value) const
{
    int i = get_int(key, default_value);
    if (i <= 0)
    {
        throw domain_error(string("Settings::get_positive_int(): invalid zero or negative value for ")
                           + key + ": " + to_string(i) + " in schema " + schema_name_);
    }
    return i;
}

int Settings::get_int(char const* key, int default_value) const
{
    if (!settings_ || !g_settings_schema_has_key(schema_.get(), key))
    {
        return default_value;
    }

    return g_settings_get_int(settings_.get(), key);
}


}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
