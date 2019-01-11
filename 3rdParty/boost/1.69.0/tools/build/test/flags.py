#!/usr/bin/python

# Copyright (C) Steven Watanabe 2018
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests the check-has-flag rule

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

# We need an object file before we can run the actual test.
t.write('input.cpp', 'void f() {}\n')
t.write('Jamroot.jam', 'obj input : input.cpp ;')
t.run_build_system()

linker_input = t.glob_file('bin/$toolset/debug*/input.obj')

# Check every possible result of pass or fail.
t.write('Jamroot.jam', '''
import flags ;
import modules ;
OBJECT_FILE = [ modules.peek : OBJECT_FILE ] ;
obj fail_cpp : test.cpp : [ check-has-flag <cxxflags>--illegal-flag-cpp
                            : <define>ERROR : <define>OK ] ;
obj pass_cpp : test.cpp : [ check-has-flag <cxxflags>-DMACRO_CPP
                            : <define>OK : <define>ERROR ] ;
obj fail_c : test.cpp : [ check-has-flag <cflags>--illegal-flag-c
                         : <define>ERROR : <define>OK ] ;
obj pass_c : test.cpp : [ check-has-flag <cflags>-DMACRO_C
                          : <define>OK : <define>ERROR ] ;
obj fail_link : test.cpp : [ check-has-flag <linkflags>--illegal-flag-link
                             : <define>ERROR : <define>OK ] ;
# The only thing that we can be certain the linker
# will accept is the name of an object file.
obj pass_link : test.cpp : [ check-has-flag <linkflags>$(OBJECT_FILE)
                             : <define>OK : <define>ERROR ] ;
''')

t.write('test.cpp', '''
#ifdef ERROR
#error ERROR defined
#endif
#ifndef OK
#error ERROR not defined
#endif
''')

# Don't check the status immediately, so that we have a chance
# to print config.log.  Also, we need a minimum of d2 to make
# sure that we always see the commands and output.
t.run_build_system(['-sOBJECT_FILE=' + linker_input, '-d2'], status=None)

if t.status != 0:
    log_file = t.read('bin/config.log')
    BoostBuild.annotation("config.log", log_file)
    t.fail_test(True)

t.expect_output_lines(['    - has --illegal-flag-cpp   : no',
                       '    - has -DMACRO_CPP          : yes',
                       '    - has --illegal-flag-c     : no',
                       '    - has -DMACRO_C            : yes',
                       '    - has --illegal-flag-link  : no',
                       '    - has *bin*/input.* : yes'])
t.expect_addition('bin/$toolset/debug*/fail_cpp.obj')
t.expect_addition('bin/$toolset/debug*/pass_cpp.obj')
t.expect_addition('bin/$toolset/debug*/fail_c.obj')
t.expect_addition('bin/$toolset/debug*/pass_c.obj')
t.expect_addition('bin/$toolset/debug*/fail_link.obj')
t.expect_addition('bin/$toolset/debug*/pass_link.obj')

t.cleanup()
