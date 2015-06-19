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

#include <gtest/gtest.h>
#include <testsetup.h>

#include <cstdio>
#include <errno.h>
#include <stdexcept>
#include <sys/apparmor.h>

using namespace std;
using namespace unity::thumbnailer::internal;

TEST(check_acess, unconfined)
{
    if (!aa_is_enabled()) {
        printf("AppArmor is disabled\n");
        return;
    }

    EXPECT_TRUE(apparmor_can_read("unconfined", "/etc/passwd"));
}

TEST(check_access, confined) {
    if (!aa_is_enabled()) {
        printf("AppArmor is disabled\n");
        return;
    }

    // We can't load new profiles into the kernel from the tests, so
    // try one of the base system profiles that is probably loaded.
    try
    {
        EXPECT_FALSE(apparmor_can_read("/sbin/dhclient", "/etc/passwd"));
    }
    catch (std::runtime_error const&) {
        EXPECT_EQ(ENOENT, errno);
        printf("Test AppArmor label not loaded in kernel\n");
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
