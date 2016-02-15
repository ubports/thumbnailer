/*
 * Copyright (C) 2016 Canonical Ltd.
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

#include <internal/version.h>

#include <QDebug>

#include <fstream>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

int const Version::major;
int const Version::minor;
int const Version::micro;
int const Version::cache_version;

Version::Version(string const& cache_dir)
    : version_file_(cache_dir + "/" + THUMBNAILER_VERSION_FILENAME)
    , cache_version_file_(cache_dir + "/" + THUMBNAILER_CACHE_VERSION_FILENAME)
    , prev_major_(2)
    , prev_minor_(3)
    , prev_micro_(0)
    , prev_cache_version_(0)
{
    {
        // Check if a version file exists. If not, we assume that
        // the previous version was 2.3.0. Otherwise, read the
        // current version from the version file. The destructor
        // writes the file back out, provided there is a difference
        // in any of the version numbers.
        ifstream ifile(version_file_);
        if (ifile.is_open())
        {
            ifile >> prev_major_ >> prev_minor_ >> prev_micro_;
            if (ifile.fail())
            {
                qCritical() << "Cannot read" << version_file_.c_str();
            }
        }
        update_version_ = prev_major_ != major || prev_minor_ != minor || prev_micro_ != micro;
    }

    {
        // Check if a cache version file exists. If not, we assume that
        // the previous version was 0.
        ifstream ifile(cache_version_file_);
        if (ifile.is_open())
        {
            ifile >> prev_cache_version_;
            if (ifile.fail())
            {
                qCritical() << "Cannot read" << cache_version_file_.c_str();
            }
        }
        update_cache_version_ = prev_cache_version_ != cache_version;
    }

}

Version::~Version()
{
    if (update_version_)
    {
        ofstream ofile(version_file_, ios_base::trunc);
        if (!ofile.is_open())
        {
            // LCOV_EXCL_START
            qCritical() << "Cannot open" << version_file_.c_str();
            return;
            // LCOV_EXCL_STOP
        }
        ofile << major << " " << minor << " " << micro << endl;
        ofile.close();
        if (ofile.fail())
        {
            qCritical() << "Cannot write" << version_file_.c_str();  // LCOV_EXCL_LINE
        }
    }

    if (update_cache_version_)
    {
        ofstream ofile(cache_version_file_, ios_base::trunc);
        if (!ofile.is_open())
        {
            // LCOV_EXCL_START
            qCritical() << "Cannot open" << cache_version_file_.c_str();
            return;
            // LCOV_EXCL_STOP
        }
        ofile << cache_version << endl;
        ofile.close();
        if (ofile.fail())
        {
            qCritical() << "Cannot write" << cache_version_file_.c_str();  // LCOV_EXCL_LINE
        }
    }
}

int Version::prev_major() const
{
    return prev_major_;
}

int Version::prev_minor() const
{
    return prev_minor_;
}

int Version::prev_micro() const
{
    return prev_micro_;
}

int Version::prev_cache_version() const
{
    return prev_cache_version_;
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
