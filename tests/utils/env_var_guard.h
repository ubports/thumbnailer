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

#pragma once

#include <internal/env_vars.h>

#include <cassert>
#include <cstdlib>
#include <string>


class EnvVarGuard final
{
public:
    // Set environment variable 'name' to 'val'.
    // To clear the variable, pass nullptr for 'val'.
    // The destructor restores the original setting.
    EnvVarGuard(char const* name, char const* val)
        : name_(name)
    {
        assert(name && *name != '\0');
        auto const old_val = getenv(name);
        if ((was_set_ = old_val != nullptr))
        {
            old_value_ = old_val;
        }
        if (val)
        {
            setenv(name, val, true);
        }
        else
        {
            unsetenv(name);
        }
    }

    // Restore the original setting.
    ~EnvVarGuard()
    {
        if (was_set_)
        {
            setenv(name_.c_str(), old_value_.c_str(), true);
        }
        else
        {
            unsetenv(name_.c_str());
        }
    }

    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
    std::string name_;
    std::string old_value_;
    bool was_set_;
};
