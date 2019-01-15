#!/usr/bin/python

# Copy-paste-modify from zlib.py
# Copyright (C) 2013 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild
import MockToolset

t = BoostBuild.Tester(arguments=['toolset=mock', '--ignore-site-config', '--user-config='], pass_toolset=0)

MockToolset.create(t)

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('lzma.h.cpp', '#include <lzma.h>\\n')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using lzma ;
exe test : test.cpp /lzma//lzma : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=lzma -o $config.exe')
action('-c -x c++ $lzma.h.cpp -o $lzma.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=lzma -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using lzma ;
exe test : test.cpp /lzma//lzma : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=lzma -o $config.exe')
action('-c -x c++ $lzma.h.cpp -o $lzma.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=lzma -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using lzma : : <name>mylzma <include>$(here)/lzma <search>$(here)/lzma ;
exe test : test.cpp /lzma//lzma : : <link>static <link>shared ;
""")

t.write('lzma/lzma.h', 'lzma')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./lzma --static-lib=mylzma -o $config.exe')
action('-c -x c++ $test.cpp -I./lzma -o $test.o')
action('$test.o -L./lzma --static-lib=mylzma -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using lzma : : <name>mylzma <include>$(here)/lzma <search>$(here)/lzma ;
exe test : test.cpp /lzma//lzma : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./lzma --shared-lib=mylzma -o $config.exe')
action('-c -x c++ $test.cpp -I./lzma -o $test.o')
action('$test.o -L./lzma --shared-lib=mylzma -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - both static and shared libraries
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using lzma : : <name>mylzma <include>$(here)/lzma <search>$(here)/lzma ;
exe test : test.cpp /lzma//lzma
  : <link>shared:<define>SHARED : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./lzma --static-lib=mylzma -o $config.exe')
action('$main.o -L./lzma --shared-lib=mylzma -o $config.exe')
action('-c -x c++ $test.cpp -I./lzma -o $test-static.o')
action('-c -x c++ $test.cpp -I./lzma -DSHARED -o $test-shared.o')
action('$test-static.o -L./lzma --static-lib=mylzma -o $test')
action('$test-shared.o -L./lzma --shared-lib=mylzma -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
