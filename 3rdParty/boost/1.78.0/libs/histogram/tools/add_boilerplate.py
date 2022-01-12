#!/usr/bin/env python3

# Copyright Hans Dembinski 2019
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

import sys
from os.path import abspath, join
import re
import datetime

project_dir = "/".join(abspath(__file__).split("/")[:-2])

filename = abspath(sys.argv[1])

copyright = """// Copyright Hans Dembinski {}
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

""".format(datetime.datetime.today().year)

if filename.endswith(".hpp"):
    with open(filename) as f:
        content = f.read()
    if not content.startswith("// Copyright"):
        content = copyright + content

    sub = filename[len(project_dir) + 1:]
    if sub.startswith("include/boost/"):
        sub = sub[len("include/boost/"):]
    if sub.startswith("test/"):
        sub = "histogram/" + sub
    guard_name = "BOOST_" + sub.replace(".", "_").replace("/", "_").upper()

    if guard_name not in content:
        lines = content.split("\n")
        for end, line in enumerate(lines):
            if line.startswith("//"):
                continue
            break
        for start in range(end, len(lines)):
            if lines[start] != "":
                break
        lines = lines[:end] + ["", "#ifndef " + guard_name, "#define " + guard_name, ""] + lines[start:]
        while lines[-1] == "":
            lines.pop()
        lines += ["", "#endif // " + guard_name, ""]
        content = "\n".join(lines)

    with open(filename, "w") as f:
        f.write(content)
