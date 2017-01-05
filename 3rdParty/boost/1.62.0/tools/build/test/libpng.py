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
t.write("libpng/png.h", 'libpng')
t.write("libpng/png.c", 'png')

t.write("Jamroot.jam", """
path-constant here : . ;
using libpng : : <source>$(here)/libpng ;
alias libpng : /libpng//libpng : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, '''
source_file('png.c', 'png')
action('-c -x c -I./libpng -o $png.o $png.c')
action('--dll $png.o -o $png.so')
action('--archive $png.o -o $png.a')
''')

t.run_build_system()
t.expect_addition('bin/standalone/libpng/mock/debug/png.dll')
t.expect_addition('bin/standalone/libpng/mock/debug/link-static/png.lib')

t.rm('libpng')

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('png.h.cpp', '#include <png.h>')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libpng ;
exe test : test.cpp /libpng//libpng : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=png -o $config.exe')
action('-c -x c++ $png.h.cpp -o $png.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=png -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libpng ;
exe test : test.cpp /libpng//libpng : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=png -o $config.exe')
action('-c -x c++ $png.h.cpp -o $png.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=png -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libpng : : <name>mylibpng <include>$(here)/libpng <search>$(here)/libpng ;
exe test : test.cpp /libpng//libpng : : <link>static <link>shared ;
""")

t.write('libpng/png.h', 'libpng')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libpng --static-lib=mylibpng -o $config.exe')
action('-c -x c++ $test.cpp -I./libpng -o $test.o')
action('$test.o -L./libpng --static-lib=mylibpng -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libpng : : <name>mylibpng <include>$(here)/libpng <search>$(here)/libpng ;
exe test : test.cpp /libpng//libpng : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libpng --shared-lib=mylibpng -o $config.exe')
action('-c -x c++ $test.cpp -I./libpng -o $test.o')
action('$test.o -L./libpng --shared-lib=mylibpng -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
