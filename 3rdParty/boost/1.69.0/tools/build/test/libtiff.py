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
t.write("libtiff/tiff.h", 'libtiff')
t.write("libtiff/tiff.c", 'tiff')

t.write("Jamroot.jam", """
path-constant here : . ;
using libtiff : : <source>$(here)/libtiff ;
alias libtiff : /libtiff//libtiff : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, '''
source_file('tiff.c', 'tiff')
action('-c -x c -I./libtiff -o $tiff.o $tiff.c')
action('--dll $tiff.o -o $tiff.so')
action('--archive $tiff.o -o $tiff.a')
''')

t.run_build_system()
t.expect_addition('bin/standalone/libtiff/mock/debug/tiff.dll')
t.expect_addition('bin/standalone/libtiff/mock/debug/link-static/tiff.lib')

t.rm('libtiff')

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('tiff.h.cpp', '#include <tiff.h>')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libtiff ;
exe test : test.cpp /libtiff//libtiff : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=tiff -o $config.exe')
action('-c -x c++ $tiff.h.cpp -o $tiff.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=tiff -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libtiff ;
exe test : test.cpp /libtiff//libtiff : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=tiff -o $config.exe')
action('-c -x c++ $tiff.h.cpp -o $tiff.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=tiff -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libtiff : : <name>mylibtiff <include>$(here)/libtiff <search>$(here)/libtiff ;
exe test : test.cpp /libtiff//libtiff : : <link>static <link>shared ;
""")

t.write('libtiff/tiff.h', 'libtiff')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libtiff --static-lib=mylibtiff -o $config.exe')
action('-c -x c++ $test.cpp -I./libtiff -o $test.o')
action('$test.o -L./libtiff --static-lib=mylibtiff -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libtiff : : <name>mylibtiff <include>$(here)/libtiff <search>$(here)/libtiff ;
exe test : test.cpp /libtiff//libtiff : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libtiff --shared-lib=mylibtiff -o $config.exe')
action('-c -x c++ $test.cpp -I./libtiff -o $test.o')
action('$test.o -L./libtiff --shared-lib=mylibtiff -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
