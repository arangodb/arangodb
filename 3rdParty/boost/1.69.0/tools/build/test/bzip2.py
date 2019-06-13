#!/usr/bin/python

# Copyright (C) 2013 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild
import MockToolset

t = BoostBuild.Tester(arguments=['toolset=mock', '--ignore-site-config', '--user-config='], pass_toolset=0)

MockToolset.create(t)

# Build from source
t.write("bzip2/bzlib.h", 'bzip2')
t.write("bzip2/blocksort.c", 'blocksort')

t.write("Jamroot.jam", """
path-constant here : . ;
using bzip2 : : <source>$(here)/bzip2 ;
alias bzip2 : /bzip2//bzip2 : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, '''
source_file('blocksort.c', 'blocksort')
action('-c -x c -I./bzip2 -o $blocksort.o $blocksort.c')
action('--dll $blocksort.o -o $bz2.so')
action('--archive $blocksort.o -o $bz2.a')
''')

t.run_build_system()
t.expect_addition('bin/standalone/bzip2/mock/debug/bz2.dll')
t.expect_addition('bin/standalone/bzip2/mock/debug/link-static/bz2.lib')

t.rm('bzip2')

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('bzlib.h.cpp', '#include <bzlib.h>\\n')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using bzip2 ;
exe test : test.cpp /bzip2//bzip2 : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=bz2 -o $config.exe')
action('-c -x c++ $bzlib.h.cpp -o $bzlib.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=bz2 -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using bzip2 ;
exe test : test.cpp /bzip2//bzip2 : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=bz2 -o $config.exe')
action('-c -x c++ $bzlib.h.cpp -o $bzlib.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=bz2 -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using bzip2 : : <name>mybzlib <include>$(here)/bzip2 <search>$(here)/bzip2 ;
exe test : test.cpp /bzip2//bzip2 : : <link>static <link>shared ;
""")

t.write('bzip2/bzlib.h', 'bzip2')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./bzip2 --static-lib=mybzlib -o $config.exe')
action('-c -x c++ $test.cpp -I./bzip2 -o $test.o')
action('$test.o -L./bzip2 --static-lib=mybzlib -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using bzip2 : : <name>mybzlib <include>$(here)/bzip2 <search>$(here)/bzip2 ;
exe test : test.cpp /bzip2//bzip2 : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./bzip2 --shared-lib=mybzlib -o $config.exe')
action('-c -x c++ $test.cpp -I./bzip2 -o $test.o')
action('$test.o -L./bzip2 --shared-lib=mybzlib -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
