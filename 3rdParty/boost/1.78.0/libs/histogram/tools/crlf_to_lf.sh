#!/bin/sh

# Copyright Hans Dembinski 2019
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# Find files with CRLF: `find <path> -not -type d -exec file "{}" ";" | grep CRLF`

perl -pi -e 's/\r\n/\n/g' $1
