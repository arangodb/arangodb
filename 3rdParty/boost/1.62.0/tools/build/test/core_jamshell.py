#!/usr/bin/python

# Copyright 2014 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild
import sys

t = BoostBuild.Tester(pass_toolset=False, pass_d0=False)

t.write("file.jam", """
actions run {
    $(ACTION)
}

# Raw commands only work on Windows
if $(OS) = NT
{
    JAMSHELL on test-raw = % ;
    JAMSHELL on test-raw-fail = % ;
}
ACTION on test-raw = "\"$(PYTHON)\" -V" ;
run test-raw ;

ACTION on test-raw-fail = missing-executable ;
run test-raw-fail ;

# On Windows, the command is stored in a temporary
# file.  On other systems it is passed directly.
if $(OS) = NT
{
    JAMSHELL on test-py = $(PYTHON) ;
}
else
{
    JAMSHELL on test-py = $(PYTHON) -c ;
}
ACTION on test-py = "
print \\\",\\\".join([str(x) for x in range(3)])
" ;
run test-py ;

DEPENDS all : test-raw test-raw-fail test-py ;
""")

t.run_build_system(["-ffile.jam", "-d1", "-sPYTHON=" + sys.executable], status=1)
t.expect_output_lines([
    "...failed run test-raw-fail...",
    "0,1,2",
    "...failed updating 1 target...",
    "...updated 2 targets..."])

t.cleanup()
