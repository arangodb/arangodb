#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

command('g++', '-print-prog-name=ar', stdout=script('ar.py'))
command('g++', '-print-prog-name=ranlib', stdout=script('ranlib.py'))

if allow_properties("variant=debug", "link=shared", "threading=single", "runtime-link=shared"):
    command("g++", unordered("-O0", "-fno-inline", "-Wall", "-g", "-fPIC"), "-c", "-o", output_file("bin/gcc-gnu-4.8.3/debug/lib.o"), input_file(source="lib.cpp"))
    command("g++", "-o", output_file("bin/gcc-gnu-4.8.3/debug/libl1.so"), "-Wl,-h", "-Wl,libl1.so", "-shared", "-Wl,--start-group", input_file("bin/gcc-gnu-4.8.3/debug/lib.o"), "-Wl,-Bstatic", "-Wl,-Bdynamic", "-Wl,--end-group", unordered("-g", "-fPIC"))
    command("g++", unordered("-O0", "-fno-inline", "-Wall", "-g", "-fPIC"), "-c", "-o", output_file("bin/gcc-gnu-4.8.3/debug/main.o"), input_file(source="main.cpp"))
    command("g++", "-Wl,-rpath", arg("-Wl,", target_path("bin/gcc-gnu-4.8.3/debug/libl1.so")), "-Wl,-rpath-link", arg("-Wl,", target_path("bin/gcc-gnu-4.8.3/debug/libl1.so")), "-o", output_file("bin/gcc-gnu-4.8.3/debug/test"), "-Wl,--start-group", input_file("bin/gcc-gnu-4.8.3/debug/main.o"), input_file("bin/gcc-gnu-4.8.3/debug/libl1.so"), "-Wl,-Bstatic", "-Wl,-Bdynamic", "-Wl,--end-group", unordered("-g", "-fPIC"))

if allow_properties("variant=release", "link=shared", "threading=single", "runtime-link=shared"):
    command('g++', unordered('-O3', '-finline-functions', '-Wno-inline', '-Wall', '-fPIC', '-DNDEBUG'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/release/lib.o'), input_file(source='lib.cpp'))
    command('g++', '-o', output_file('bin/gcc-gnu-4.8.3/release/libl1.so'), '-Wl,-h', '-Wl,libl1.so', '-shared', '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/release/lib.o'), '-Wl,-Bstatic', '-Wl,-Bdynamic', '-Wl,--end-group', '-fPIC')
    command('g++', unordered('-O3', '-finline-functions', '-Wno-inline', '-Wall', '-fPIC', '-DNDEBUG'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/release/main.o'), input_file(source='main.cpp'))
    command('g++', '-Wl,-rpath', arg('-Wl,', target_path('bin/gcc-gnu-4.8.3/release/libl1.so')), '-Wl,-rpath-link', arg('-Wl,', target_path('bin/gcc-gnu-4.8.3/release/libl1.so')), '-o', output_file('bin/gcc-gnu-4.8.3/release/test'), '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/release/main.o'), input_file('bin/gcc-gnu-4.8.3/release/libl1.so'), '-Wl,-Bstatic', '-Wl,-Bdynamic', '-Wl,--end-group', '-fPIC')

if allow_properties("variant=debug", "link=shared", "threading=multi", "runtime-link=shared"):
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-pthread', '-fPIC'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/debug/threading-multi/lib.o'), input_file(source='lib.cpp'))
    command('g++', '-o', output_file('bin/gcc-gnu-4.8.3/debug/threading-multi/libl1.so'), '-Wl,-h', '-Wl,libl1.so', '-shared', '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/debug/threading-multi/lib.o'), '-Wl,-Bstatic', '-Wl,-Bdynamic', '-lrt', '-Wl,--end-group', unordered('-g', '-pthread', '-fPIC'))
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-pthread', '-fPIC'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/debug/threading-multi/main.o'), input_file(source='main.cpp'))
    command('g++', '-Wl,-rpath', arg('-Wl,', target_path('bin/gcc-gnu-4.8.3/debug/threading-multi/libl1.so')), '-Wl,-rpath-link', arg('-Wl,', target_path('bin/gcc-gnu-4.8.3/debug/threading-multi/libl1.so')), '-o', output_file('bin/gcc-gnu-4.8.3/debug/threading-multi/test'), '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/debug/threading-multi/main.o'), input_file('bin/gcc-gnu-4.8.3/debug/threading-multi/libl1.so'), '-Wl,-Bstatic', '-Wl,-Bdynamic', '-lrt', '-Wl,--end-group', unordered('-g', '-pthread', '-fPIC'))

if allow_properties("variant=debug", "link=static", "threading=single", "runtime-link=shared"):
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/lib.o'), input_file(source='lib.cpp'))
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/main.o'), input_file(source='main.cpp'))
    command('g++', '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/test'), '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/debug/link-static/main.o'), input_file('bin/gcc-gnu-4.8.3/debug/link-static/libl1.a'), '-Wl,-Bstatic', '-Wl,-Bdynamic', '-Wl,--end-group', '-g')

if allow_properties("variant=debug", "link=static", "threading=single", "runtime-link=static"):
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/lib.o'), input_file(source='lib.cpp'))
    command('g++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/main.o'), input_file(source='main.cpp'))
    command('g++', '-o', output_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/test'), '-Wl,--start-group', input_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/main.o'), input_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/libl1.a'), '-Wl,--end-group', unordered('-g', '-static'))


if allow_properties("variant=debug", "link=shared", "threading=single", "runtime-link=shared"):
    command("g++", unordered("-O0", "-fno-inline", "-Wall", "-g", "-fPIC", "-std=c++1y"), "-c", "-o", output_file("bin/gcc-gnu-4.8.3/debug/lib.o"), input_file(source="lib.cpp"))
    command("g++", "-o", output_file("bin/gcc-gnu-4.8.3/debug/libl1.so"), "-Wl,-h", "-Wl,libl1.so", "-shared", "-Wl,--start-group", input_file("bin/gcc-gnu-4.8.3/debug/lib.o"), "-Wl,-Bstatic", "-Wl,-Bdynamic", "-Wl,--end-group", unordered("-g", "-fPIC", "-std=c++1y"))
    command("g++", unordered("-O0", "-fno-inline", "-Wall", "-g", "-fPIC", "-std=c++1y"), "-c", "-o", output_file("bin/gcc-gnu-4.8.3/debug/main.o"), input_file(source="main.cpp"))
    command("g++", "-Wl,-rpath", arg("-Wl,", target_path("bin/gcc-gnu-4.8.3/debug/libl1.so")), "-Wl,-rpath-link", arg("-Wl,", target_path("bin/gcc-gnu-4.8.3/debug/libl1.so")), "-o", output_file("bin/gcc-gnu-4.8.3/debug/test"), "-Wl,--start-group", input_file("bin/gcc-gnu-4.8.3/debug/main.o"), input_file("bin/gcc-gnu-4.8.3/debug/libl1.so"), "-Wl,-Bstatic", "-Wl,-Bdynamic", "-Wl,--end-group", unordered("-g", "-fPIC", "-std=c++1y"))


main()
