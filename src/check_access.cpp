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

#include <internal/check_access.h>
#include <internal/safe_strerror.h>

#include <errno.h>
#include <stdexcept>
#include <sys/apparmor.h>

using namespace std;

namespace
{

enum class Access
{
    write = 1,
    read = 2
};

char const AA_CLASS_FILE = 2;

bool query_file(Access access, string const& label, string const& path)
{
    static bool enabled = aa_is_enabled();
    if (!enabled)
    {
        // If AppArmor is not enabled, assume access is granted.
        return true;
    }

    string query(AA_QUERY_CMD_LABEL, AA_QUERY_CMD_LABEL_SIZE);
    query += label;
    query += '\0';
    query += AA_CLASS_FILE;
    query += path;

    int allowed = 0, audited = 0;
    if (aa_query_label(static_cast<uint32_t>(access), const_cast<char*>(query.data()), query.size(), &allowed, &audited) < 0)
    {
        using namespace unity::thumbnailer::internal;
        throw runtime_error("query_file(): Could not query AppArmor access: " + safe_strerror(errno));
    }
    return allowed;
}

}

namespace unity
{

namespace thumbnailer
{

namespace internal
{

bool apparmor_can_read(string const& apparmor_label, string const& path)
{
    return query_file(Access::read, apparmor_label, path);
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
