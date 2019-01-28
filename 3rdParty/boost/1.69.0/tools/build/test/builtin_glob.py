#!/usr/bin/python

# Copyright 2014 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# This tests the GLOB rule.

import os
import BoostBuild

def test_glob(files, glob, expected, setup=""):
    t = BoostBuild.Tester(["-ffile.jam"], pass_toolset=0)
    t.write("file.jam", setup + """
    for local p in [ SORT %s ]
    {
        ECHO $(p) ;
    }
    UPDATE ;
    """ % glob)
    for f in files:
        t.write(f, "")
    # convert / into \ on windows
    expected = [os.path.join(*p.split("/")) for p in expected]
    expected.sort()
    t.run_build_system(stdout="\n".join(expected + [""]))
    t.cleanup()

# one or both arguments empty
test_glob([], "[ GLOB : ]", [])
test_glob([], "[ GLOB . : ]", [])
test_glob([], "[ GLOB : * ]", [])

# a single result
test_glob([], "[ GLOB . : * ]", ["./file.jam"])

# * can match any number of characters
test_glob([], "[ GLOB . : file*.jam ]", ["./file.jam"])
test_glob([], "[ GLOB . : f*am ]", ["./file.jam"])
# ? should match a single character, but not more than one
test_glob([], "[ GLOB . : fi?e.?am ]", ["./file.jam"])
test_glob([], "[ GLOB . : fi?.jam ]", [])
# [abc-fh-j] matches a set of characters
test_glob([], '[ GLOB . : "[f][i][l][e].jam" ]', ["./file.jam"])
test_glob([], '[ GLOB . : "[fghau][^usdrwe][k-o][^f-s].jam" ]', ["./file.jam"])
# \x matches x
test_glob([], "[ GLOB . : \\f\\i\\l\\e.jam ]", ["./file.jam"])

# multiple results
test_glob(["test.txt"], "[ GLOB . : * ]", ["./file.jam", "./test.txt"])

# directories
test_glob(["dir1/dir2/test.txt"], "[ GLOB dir1 : * ]", ["dir1/dir2"]);

# non-existent directory
test_glob([], "[ GLOB dir1 : * ] ", [])

# multiple directories and patterns
test_glob(["dir1/file1.txt", "dir2/file1.txt",
           "dir2/file2.txt"],
          "[ GLOB dir1 dir2 : file1* file2* ]",
          ["dir1/file1.txt", "dir2/file1.txt",
           "dir2/file2.txt"])

# The directory can contain . and ..
test_glob(["dir/test.txt"], "[ GLOB dir/. : test.txt ]", ["dir/./test.txt"])
test_glob(["dir/test.txt"], "[ GLOB dir/.. : file.jam ]", ["dir/../file.jam"])

# On case insensitive filesystems, the result should
# be normalized.  It should NOT be downcased.
test_glob(["TEST.TXT"], "[ GLOB . : TEST.TXT ]", ["./TEST.TXT"])

case_insensitive = (os.path.normcase("FILE") == "file")

if case_insensitive:
    test_glob(["TEST.TXT"], "[ GLOB . : test.txt ]", ["./TEST.TXT"])
    # This used to fail because the caching routines incorrectly
    # reported that . and .. do not exist.
    test_glob(["D1/D2/TEST.TXT"], "[ GLOB D1/./D2 : test.txt ]",
              ["D1/./D2/TEST.TXT"])
    test_glob(["D1/TEST.TXT", "TEST.TXT"], "[ GLOB D1/../D1 : test.txt ]",
              ["D1/../D1/TEST.TXT"])
    # This also failed because directories that were first found
    # by GLOB were recorded as non-existent.
    test_glob(["D1/D2/TEST.TXT"], "[ GLOB d1/d2 : test.txt ]",
              ["D1/D2/TEST.TXT"],
              "GLOB . : * ;")
