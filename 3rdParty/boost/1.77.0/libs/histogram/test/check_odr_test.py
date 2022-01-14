#!/usr/bin/env python3

# Copyright 2019 Hans Dembinski, Henry Schreiner
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

"""
This test makes sure that all boost.histogram headers are included in the ODR test
carried out in odr_main_test.cpp. See that file for details on why this test needed.
"""

import os
import sys
import re

this_path = os.path.dirname(__file__)

all_headers = set()
# Includes are either in this_path/../include for the standalone version or ...
include_path = os.path.join(this_path, "..", "include", "boost", "histogram")
if not os.path.exists(include_path):
    # ... in this_path/../../.. for the bundled boost release
    include_path = os.path.join(this_path, "..", "..", "..", "boost", "histogram")
    assert os.path.exists(include_path)

# this has to be rindex, because any leading path could also be called boost
root_include_path = include_path[: include_path.rindex("boost")]

for root, dirs, files in os.walk(include_path):
    for fn in files:
        fn = os.path.join(root, fn)
        fn = fn[len(root_include_path) :]
        if fn.endswith("debug.hpp"):  # is never included
            continue
        all_headers.add(fn)


def get_headers(filename):
    with open(filename, "rb") as f:
        for hdr in re.findall(b'^#include [<"]([^>"]+)[>"]', f.read(), re.MULTILINE):
            hdr = hdr.decode()
            if not hdr.startswith("boost/histogram"):
                continue
            yield hdr.replace("/", os.path.sep)  # adapt the paths for Windows


included_headers = set()
unread_headers = set()
for hdr in get_headers(os.path.join(this_path, "odr_test.cpp")):
    unread_headers.add(hdr)

while unread_headers:
    included_headers.update(unread_headers)
    for hdr in tuple(unread_headers):  # copy needed because unread_headers is modified
        unread_headers.remove(hdr)
        for hdr2 in get_headers(os.path.join(root_include_path, hdr)):
            if hdr2 not in included_headers:
                unread_headers.add(hdr2)

diff = sorted(all_headers - set(included_headers))

if not diff:
    sys.exit(0)


print("Header not included in odr_test.cpp:")
for fn in diff:
    print(fn)

sys.exit(1)
