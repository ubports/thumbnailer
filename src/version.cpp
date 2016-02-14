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

Version::Version(string const& cache_dir)
    : version_file_(cache_dir + "/" + THUMBNAILER_VERSION_FILENAME)
{
    // Check if a version file exists. If not, we assume that
    // the previous version was 2.3.0. Otherwise, read the
    // current version from the version file. The destructor
    // writes the file back out, provided there is a difference
    // in any of the version numbers.
    ifstream ifile(version_file_);
    if (!ifile.is_open())
    {
        prev_major_ = 2;
        prev_minor_ = 3;
        prev_micro_ = 0;
    }
    else
    {
        ifile >> prev_major_ >> prev_minor_ >> prev_micro_;
        if (ifile.fail())
        {
            prev_major_ = 2;
            prev_minor_ = 3;
            prev_micro_ = 0;
            qCritical() << "Cannot read" << version_file_.c_str();  // LCOV_EXCL_LINE
        }
    }
    update_version_ = prev_major_ != major || prev_minor_ != minor || prev_micro_ != micro;
}

Version::~Version()
{
    if (update_version_)
    {
        ofstream ofile(version_file_, ios_base::trunc);
        if (!ofile.is_open())
        {
            qCritical() << "Cannot open" << version_file_.c_str();
        }
        ofile << major << " " << minor << " " << micro << endl;
        if (!ofile.fail())
        {
            qCritical() << "Cannot write" << version_file_.c_str();  // LCOV_EXCL_LINE
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

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
