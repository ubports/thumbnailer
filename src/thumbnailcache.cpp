/*
 * Copyright (C) 2013 Canonical Ltd.
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
 * Authored by: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 */

#include<thumbnailer.h>
#include<internal/thumbnailcache.h>
#include<stdexcept>
#include<glib.h>
#include<sys/stat.h>
#include<cassert>
#include<cstdio>
#include<cstring>

using namespace std;

class ThumbnailCachePrivate {
public:
    string tndir;
    string smalldir;
    string largedir;

    ThumbnailCachePrivate();
    string md5(const string &str) const;
    string get_cache_file_name(const std::string &original, ThumbnailSize desired) const;
};

ThumbnailCachePrivate::ThumbnailCachePrivate() {
    string xdg_base = g_get_user_cache_dir();
    if (xdg_base == "") {
        string s("Could not determine cache dir.");
        throw runtime_error(s);
    }
    int ec = mkdir(xdg_base.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    if (ec < 0 && errno != EEXIST) {
        string s("Could not create base dir - ");
        s += strerror(errno);
        throw runtime_error(s);
    }
    tndir = xdg_base + "/thumbnails";
    ec = mkdir(tndir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    if (ec < 0 && errno != EEXIST) {
        string s("Could not create thumbnail dir - ");
        s += strerror(errno);
        throw runtime_error(s);
    }
    smalldir = tndir + "/normal";
    ec = mkdir(smalldir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    if (ec < 0 && errno != EEXIST) {
        string s("Could not create small dir - ");
        s += strerror(errno);
        throw runtime_error(s);
    }
    largedir = tndir + "/large";
    ec = mkdir(largedir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    if (ec < 0 && errno != EEXIST) {
        string s("Could not create large dir - ");
        s += strerror(errno);
        throw runtime_error(s);
    }
}

string ThumbnailCachePrivate::md5(const string &str) const {
    const unsigned char *buf = (const unsigned char *)str.c_str();
    char *normalized = g_utf8_normalize((const gchar*)buf, str.size(), G_NORMALIZE_ALL);
    string final;
    gchar *result;

    if(normalized) {
        buf = (const unsigned char*)normalized;
    }
    gssize bytes = str.length();

    result = g_compute_checksum_for_data(G_CHECKSUM_MD5, buf, bytes);
    final = result;
    g_free((gpointer)normalized);
    g_free(result);
    return final;
}

string ThumbnailCachePrivate::get_cache_file_name(const std::string & abs_original, ThumbnailSize desired) const {
    assert(abs_original[0] == '/');
    string path = desired == TN_SIZE_SMALL ? smalldir : largedir;
    path += "/" + md5("file://" + abs_original) + ".png";
    return path;
}

ThumbnailCache::ThumbnailCache() : p(new ThumbnailCachePrivate()) {
}

ThumbnailCache::~ThumbnailCache() {
    delete p;
}

std::string ThumbnailCache::get_if_exists(const std::string &abs_path, ThumbnailSize desired_size) const {
    assert(abs_path[0] == '/');
    string fname = p->get_cache_file_name(abs_path, desired_size);
    FILE *f = fopen(fname.c_str(), "r");
    bool existed = false;
    if(f) {
        existed = true;
        fclose(f);
    }
    return existed ? fname : string("");
}

std::string ThumbnailCache::get_cache_file_name(const std::string &abs_path, ThumbnailSize desired) const {
    return p->get_cache_file_name(abs_path, desired);
}
