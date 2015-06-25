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

#include <internal/file_io.h>

#include <internal/raii.h>
#include <internal/safe_strerror.h>

#include <boost/filesystem.hpp>

#include <cstring>

#include <fcntl.h>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Read entire file and return contents as a string.

string read_file(string const& filename)
{
    int fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1)
    {
        throw runtime_error("read_file(): cannot open \"" + filename + "\": " + safe_strerror(errno));
    }
    FdPtr fd_ptr(fd, do_close);

    string contents;
    char buf[4096];
    int rc;
    while ((rc = read(fd_ptr.get(), buf, sizeof(buf))) != 0)
    {
        if (rc == -1)
        {
            // LCOV_EXCL_START
            throw runtime_error("read_file(): cannot read from \"" + filename + "\": " + safe_strerror(errno));
            // LCOV_EXCL_STOP
        }
        contents.append(buf, rc);
    }
    return contents;
}

namespace
{

static string tmp_dir = []
{
    char const* dirp = getenv("TMPDIR");
    string dir = dirp ? dirp : "/tmp";
    return dir;
}();

}  // namespace

// Atomically write contents to filename.

void write_file(string const& filename, string const& contents)
{
    using namespace boost::filesystem;

    path abs_path = absolute(filename);
    path dir = abs_path.parent_path();

    string tmp_path = dir.native() + "/thumbnailer.XXXXXX";
    int fd = mkstemp(&tmp_path[0]);
    if (fd == -1)
    {
        string s = string("write_file(): mkstemp() failed for " + tmp_path + ": ") + safe_strerror(errno);
        throw runtime_error(s);
    }

    {
        FdPtr fd_ptr(fd, do_close);

        int rc = write(fd_ptr.get(), &contents[0], contents.size());
        if (rc == -1)
        {
            // LCOV_EXCL_START
            throw runtime_error("write_file(): cannot write to \"" + filename + "\": " + safe_strerror(errno));
            // LCOV_EXCL_STOP
        }
        if (string::size_type(rc) != contents.size())
        {
            throw runtime_error("write_file(): short write for \"" + filename + "\"");  // LCOV_EXCL_LINE
        }
    }  // Close tmp file

    if (rename(tmp_path.c_str(), filename.c_str()) == -1)
    {
        // LCOV_EXCL_START
        unlink(tmp_path.c_str());
        throw runtime_error("write_file(): cannot rename " + tmp_path + " to " + filename + ": " +
                            safe_strerror(errno));
        // LCOV_EXCL_STOP
    }
}

// Write contents of in_fd to out_fd, using current read position of in_fd.

void write_file(int in_fd, int out_fd)
{
    char buf[16 * 1024];
    int bytes_read;
    do
    {
        if ((bytes_read = read(in_fd, buf, sizeof(buf))) == -1)
        {
            throw runtime_error("read failed: " + safe_strerror(errno));
        }
        int bytes_written = write(out_fd, buf, bytes_read);
        if (bytes_written == -1)
        {
            throw runtime_error("write failed: " + safe_strerror(errno));
        }
        // LCOV_EXCL_START
        else if (bytes_written != bytes_read)
        {
            throw runtime_error("short write, requested " + to_string(bytes_read) + " bytes, wrote " +
                                to_string(bytes_written) + " bytes");
        }
        // LCOV_EXCL_STOP
    } while (bytes_read != 0);
}

// Write contents of fd to path.

void write_file(int fd, string const& path)
{
    FdPtr out_fd(open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, 0600), do_close);
    if (out_fd.get() == -1)
    {
        throw runtime_error("write_file(): cannot open " + path + ": " + safe_strerror(errno));
    }
    write_file(fd, out_fd.get());
}

// Return a temporary file name in TMPDIR.

string create_tmp_filename()
{
    string tmp_path = tmp_dir + "/thumbnailer.XXXXXX";
    int fd = mkstemp(&tmp_path[0]);
    if (fd == -1)
    {
        // LCOV_EXCL_START
        string s = string("create_tmp_filename(): mkstemp() failed for " + tmp_path + ": ") + safe_strerror(errno);
        throw runtime_error(s);
        // LCOV_EXCL_STOP
    }
    close(fd);
    return tmp_path;
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
