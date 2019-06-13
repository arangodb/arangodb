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
t.write("libjpeg/jpeglib.h", 'libjpeg')
t.write("libjpeg/jerror.c", 'jpeg')

t.write("Jamroot.jam", """
path-constant here : . ;
using libjpeg : : <source>$(here)/libjpeg ;
alias libjpeg : /libjpeg//libjpeg : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, '''
source_file('jpeg.c', 'jpeg')
action('-c -x c -I./libjpeg -o $jpeg.o $jpeg.c')
action('--dll $jpeg.o -o $jpeg.so')
action('--archive $jpeg.o -o $jpeg.a')
''')

t.run_build_system()
t.expect_addition('bin/standalone/libjpeg/mock/debug/jpeg.dll')
t.expect_addition('bin/standalone/libjpeg/mock/debug/link-static/jpeg.lib')

t.rm('libjpeg')

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('jpeg.h.cpp', '#include <stdio.h>\\n#include <jpeglib.h>\\n')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

# Default initialization - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libjpeg ;
exe test : test.cpp /libjpeg//libjpeg : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=jpeg -o $config.exe')
action('-c -x c++ $jpeg.h.cpp -o $jpeg.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=jpeg -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Default initialization - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libjpeg ;
exe test : test.cpp /libjpeg//libjpeg : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=jpeg -o $config.exe')
action('-c -x c++ $jpeg.h.cpp -o $jpeg.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=jpeg -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - static library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libjpeg : : <name>mylibjpeg <include>$(here)/libjpeg <search>$(here)/libjpeg ;
exe test : test.cpp /libjpeg//libjpeg : : <link>static <link>shared ;
""")

t.write('libjpeg/jpeglib.h', 'libjpeg')

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libjpeg --static-lib=mylibjpeg -o $config.exe')
action('-c -x c++ $test.cpp -I./libjpeg -o $test.o')
action('$test.o -L./libjpeg --static-lib=mylibjpeg -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

# Initialization in explicit location - shared library
t.rm('bin')
t.write("Jamroot.jam", """
path-constant here : . ;
using libjpeg : : <name>mylibjpeg <include>$(here)/libjpeg <search>$(here)/libjpeg ;
exe test : test.cpp /libjpeg//libjpeg : : <link>static <link>shared ;
""")

MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./libjpeg --shared-lib=mylibjpeg -o $config.exe')
action('-c -x c++ $test.cpp -I./libjpeg -o $test.o')
action('$test.o -L./libjpeg --shared-lib=mylibjpeg -o $test')
''')
t.run_build_system()
t.expect_addition('bin/mock/debug/test.exe')
t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()
