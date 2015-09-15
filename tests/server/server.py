#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (C) 2015 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>

import os
import sys
import time
import tornado.httpserver
import tornado.options
import tornado.web

class FileReaderProvider(tornado.web.RequestHandler):
    def initialize(self):
        self.extensions_map = {'jpeg': 'image/jpeg', 'jpg': 'image/jpeg', 'png': 'image/png', 'txt': 'text/plain', 'xml': 'application/xml'}

    def read_file(self, path):
        for extension, content_type in self.extensions_map.items():
            filename = os.path.join(os.path.dirname(__file__), "%s.%s" % (path, extension))
            if os.path.isfile(filename):
                self.set_header("Content-Type", content_type)
                with open(filename, 'rb') as fp:
                    self.write(fp.read())
                return

        self.set_status(404)
        self.write("<html><body>404 ERROR</body></html>")

class UbuntuAlbumImagesProvider(FileReaderProvider):
    def get(self):
        if self.get_argument('artist', None) == "test_threads":
            self.write("TEST_THREADS_TEST_%s" % self.get_argument('album', None ))
        elif self.get_argument('artist', None) == "sleep":
            seconds = int(self.get_argument('album', None))
            time.sleep(seconds)
            self.write("TEST_SLEEP_TEST_%s" % seconds)
        elif self.get_argument('artist', None) == "generate":
            file = 'images/coverart'
            self.read_file(file)
        elif self.get_argument('artist', None) == "error":
            error_code = self.get_argument('album', None)
            self.set_status(int(error_code))
            content = "<html><body>" + error_code + " ERROR</body></html>"
            self.write(content)
        else:
            file = 'images/%s_%s_album' % (self.get_argument('artist', None ), self.get_argument('album', None ))
            self.read_file(file)
        self.finish()

class UbuntuArtistImagesProvider(FileReaderProvider):
    def get(self):
        if self.get_argument('artist', None) == "test_threads":
            self.write("TEST_THREADS_TEST_%s" % self.get_argument('artist', None ))
        elif self.get_argument('artist', None) == "sleep":
            seconds = int(self.get_argument('album', None))
            time.sleep(seconds)
            self.write("TEST_SLEEP_TEST_%s" % seconds)
        elif self.get_argument('artist', None) == "generate":
            file = 'images/coverart'
            self.read_file(file)
        elif self.get_argument('artist', None) == "error":
            error_code = self.get_argument('album', None)
            self.set_status(int(error_code))
            content = "<html><body>" + error_code + " ERROR</body></html>"
            self.write(content)
        else:
            file = 'images/%s_%s' % (self.get_argument('artist', None ), self.get_argument('album', None ))
            self.read_file(file)
        self.finish()

def main(argv):
    tornado.options.parse_command_line(argv)
    application = tornado.web.Application([
        (r"/musicproxy/v1/album-art", UbuntuAlbumImagesProvider),
        (r"/musicproxy/v1/artist-art", UbuntuArtistImagesProvider)
    ], gzip=True)
    sockets = tornado.netutil.bind_sockets(0, '127.0.0.1')
    server = tornado.httpserver.HTTPServer(application)
    server.add_sockets(sockets)

    port = sockets[0].getsockname()[1]
    sys.stdout.write('%d\n' % port)
    sys.stdout.flush()

    tornado.ioloop.IOLoop.instance().start()

if __name__ == "__main__":
    sys.exit(main(sys.argv))
