#!/bin/sh

#
# Copyright (C) 2015 Canonical Ltd
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Michi Henning <michi.henning@canonical.com>
#

[ $# -ne 2 ] && {
    echo "usage: $(basename $0) error-file plugin-dir" 1>&2
    exit 1
}

xvfb-run -a -s "-screen 0 800x600x24" -e "$1" ./qml_test -import "$2"
rc=$?
[ $rc -ne 0 ] && {
    cat "$1" 1>&2
}

exit $rc
