#!/usr/bin/python

# Copyright 2014 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests the cxxflags feature

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

# cxxflags should be applied to C++ compilation,
# but not to C.
t.write("Jamroot.jam", """
obj test-cpp : test.cpp : <cxxflags>-DOKAY ;
obj test-c : test.c : <cxxflags>-DBAD ;
""")

t.write("test.cpp", """
#ifndef OKAY
#error Cannot compile without OKAY
#endif
""")

t.write("test.c", """
#ifdef BAD
#error Cannot compile with BAD
#endif
""")

t.run_build_system()
t.expect_addition("bin/$toolset/debug*/test-cpp.obj")
t.expect_addition("bin/$toolset/debug*/test-c.obj")

t.cleanup()
