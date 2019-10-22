#!/usr/bin/env python3
from __future__ import print_function
import sys
import glob
import os
import re

exit_code = 0

for dir in ("test", "examples"):
    cpp = set([os.path.basename(x) for x in glob.glob(dir + "/*.cpp")])

    for build_file in ("Jamfile", "CMakeLists.txt"):
        filename = os.path.join(dir, build_file)
        if not os.path.exists(filename): continue
        run = set(re.findall("([a-zA-Z0-9_]+\.cpp)", open(filename).read()))

        diff = cpp - run

        if diff:
            print("NOT TESTED in %s\n  " % filename +
                  "\n  ".join(["%s/%s" % (dir, x) for x in diff]))
            exit_code = 1

sys.exit(exit_code)
