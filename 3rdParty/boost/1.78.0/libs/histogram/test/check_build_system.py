#!/usr/bin/env python3

# Copyright 2019 Hans Dembinski
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

import sys
import glob
import os
import re

ps = os.path.split
pj = os.path.join

# assumes that check_build_system.py sits in <project_path>/tests
project_path = ps(ps(__file__)[0])[0]

exit_code = 0

for dir in (pj(project_path, "test"), pj(project_path, "examples")):
    cpp = set([os.path.basename(x) for x in glob.glob(dir + "/*.cpp")])

    for build_file in ("Jamfile", "CMakeLists.txt"):
        filename = os.path.join(dir, build_file)
        if not os.path.exists(filename):
            continue
        run = set(re.findall(r"([a-zA-Z0-9_]+\.cpp)", open(filename).read()))

        diff = cpp - run
        diff.discard("check_cmake_version.cpp")  # ignore
        diff.discard("check_build_system.py")  # ignore

        if diff:
            print(
                "NOT TESTED in %s\n  " % filename
                + "\n  ".join(["%s/%s" % (dir, x) for x in diff])
            )
            exit_code = 1

sys.exit(exit_code)
