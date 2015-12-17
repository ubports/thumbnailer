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

#include <settings.h>

#include <internal/env_vars.h>
#include "settings-defaults.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <gio/gio.h>
#pragma GCC diagnostic pop
#include <QDebug>

#include <memory>

using namespace std;

namespace unity
{

namespace thumbnailer
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
    return get_string("dash-ubuntu-com-key", DASH_UBUNTU_COM_KEY_DEFAULT);
}

int Settings::full_size_cache_size() const
{
    return get_positive_int("full-size-cache-size", FULL_SIZE_CACHE_SIZE_DEFAULT);
}

int Settings::thumbnail_cache_size() const
{
    return get_positive_int("thumbnail-cache-size", THUMBNAIL_CACHE_SIZE_DEFAULT);
}

int Settings::failure_cache_size() const
{
    return get_positive_int("failure-cache-size", FAILURE_CACHE_SIZE_DEFAULT);
}

int Settings::max_thumbnail_size() const
{
    return get_positive_int("max-thumbnail-size", MAX_THUMBNAIL_SIZE_DEFAULT);
}

int Settings::retry_not_found_hours() const
{
    return get_positive_int("retry-not-found-hours", RETRY_NOT_FOUND_HOURS_DEFAULT);
}

int Settings::retry_error_hours() const
{
    return get_positive_int("retry-error-hours", RETRY_ERROR_HOURS_DEFAULT);
}

int Settings::max_downloads() const
{
    return get_positive_int("max-downloads", MAX_DOWNLOADS_DEFAULT);
}

int Settings::max_extractions() const
{
    return get_positive_or_zero_int("max-extractions", MAX_EXTRACTIONS_DEFAULT);
}

int Settings::extraction_timeout() const
{
    return get_positive_int("extraction-timeout", EXTRACTION_TIMEOUT_DEFAULT);
}

int Settings::max_backlog() const
{
    return get_positive_int("max-backlog", MAX_BACKLOG_DEFAULT);
}

bool Settings::trace_client() const
{
    return get_bool("trace-client", TRACE_CLIENT_DEFAULT);
}

int Settings::log_level() const
{
    using namespace unity::thumbnailer::internal;

    int log_level = get_positive_or_zero_int("log-level", LOG_LEVEL_DEFAULT);
    char const* level = getenv(LOG_LEVEL);
    if (level && *level)
    {
        int l;
        try
        {
            l = std::stoi(level);
        }
        catch (std::exception const& e)
        {
            qCritical() << "Environment variable" << LOG_LEVEL << "has invalid setting:" << level
                        << "(expected value in range 0..2) - variable ignored";
            return log_level;
        }
        if (l < 0 || l > 2)
        {
            qCritical() << "Environment variable" << LOG_LEVEL << "has invalid setting:" << level
                        << "(expected value in range 0..2) - variable ignored";
            return log_level;
        }
        log_level = l;
    }
    return log_level;
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

int Settings::get_positive_or_zero_int(char const* key, int default_value) const
{
    int i = get_int(key, default_value);
    if (i < 0)
    {
        throw domain_error(string("Settings::get_positive_or_zero_int(): invalid negative value for ")
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

bool Settings::get_bool(char const* key, bool default_value) const
{
    if (!settings_ || !g_settings_schema_has_key(schema_.get(), key))
    {
        qDebug() << "get_bool, returning default value:" << default_value;
        return default_value;
    }

    bool b = g_settings_get_boolean(settings_.get(), key);
    qDebug() << "get_bool, returning settings value:" << b;
    return b;
    return g_settings_get_boolean(settings_.get(), key);
}

}  // namespace thumbnailer

}  // namespace unity
