#!/usr/bin/python3
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
# Authored by: James Henstridge <james.henstridge@canonical.com>

import base64
import mimetypes
import sys

import mutagen

def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: add-cover-art.py SONG COVER\n")
        return 1

    input, cover_art = argv[1:]

    cover_mime = mimetypes.guess_type(cover_art)[0]
    with open(cover_art, 'rb') as fp:
        cover_data = fp.read()

    f = mutagen.File(input)
    if isinstance(f, mutagen.oggvorbis.OggVorbis):
        picture = mutagen.flac.Picture()
        picture.data = cover_data
        picture.type = 3 # front
        picture.mime = cover_mime
        f.tags['METADATA_BLOCK_PICTURE'] = [
            base64.standard_b64encode(picture.write())]
    elif isinstance(f, mutagen.mp3.MP3):
        f.tags.delall('APIC')
        f.tags.add(mutagen.id3.APIC(
            encoding=0,
            mime=cover_mime,
            type=3, # front
            data=cover_data))
    elif isinstance(f, mutagen.mp4.MP4):
        if cover_mime == "image/jpeg":
            picture = mutagen.mp4.MP4Cover(
                cover_data, mutagen.mp4.MP4Cover.FORMAT_JPEG)
        elif cover_mime == "image/png":
            picture = mutagen.mp4.MP4Cover(
                cover_data, mutagen.mp4.MP4Cover.FORMAT_PNG)
        else:
            sys.stderr.write("Can only save jpeg or png cover art\n")
            return 1
        f.tags['covr'] = [picture]
    else:
        sys.stderr.write("Unknown image type: %r\n" % type(f))
        return 1
    f.save()

if __name__ == '__main__':
    sys.exit(main(sys.argv))
