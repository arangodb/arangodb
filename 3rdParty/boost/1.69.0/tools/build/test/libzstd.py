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
source_file('zstd.h.cpp', '#include <zstd.h>\\n')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using zstd ;
exe test : test.cpp /zstd//zstd : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=zstd -o $config.exe')
action('-c -x c++ $zstd.h.cpp -o $zstd.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=zstd -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using zstd ;
exe test : test.cpp /zstd//zstd : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=zstd -o $config.exe')
action('-c -x c++ $zstd.h.cpp -o $zstd.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=zstd -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using zstd : : <name>myzstd <include>$(here)/zstd <search>$(here)/zstd ;
exe test : test.cpp /zstd//zstd : : <link>static <link>shared ;
""")

t.write('zstd/zstd.h', 'zstd')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./zstd --static-lib=myzstd -o $config.exe')
action('-c -x c++ $test.cpp -I./zstd -o $test.o')
action('$test.o -L./zstd --static-lib=myzstd -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using zstd : : <name>myzstd <include>$(here)/zstd <search>$(here)/zstd ;
exe test : test.cpp /zstd//zstd : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./zstd --shared-lib=myzstd -o $config.exe')
action('-c -x c++ $test.cpp -I./zstd -o $test.o')
action('$test.o -L./zstd --shared-lib=myzstd -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - both static and shared libraries
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using zstd : : <name>myzstd <include>$(here)/zstd <search>$(here)/zstd ;
exe test : test.cpp /zstd//zstd
  : <link>shared:<define>SHARED : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./zstd --static-lib=myzstd -o $config.exe')
action('$main.o -L./zstd --shared-lib=myzstd -o $config.exe')
action('-c -x c++ $test.cpp -I./zstd -o $test-static.o')
action('-c -x c++ $test.cpp -I./zstd -DSHARED -o $test-shared.o')
action('$test-static.o -L./zstd --static-lib=myzstd -o $test')
action('$test-shared.o -L./zstd --shared-lib=myzstd -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
