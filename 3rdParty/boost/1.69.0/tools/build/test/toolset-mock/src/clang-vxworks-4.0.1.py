#!/usr/bin/python
#
# Copyright 2018 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

command('clang++', '-print-prog-name=ar', stdout=script('ar.py'))
command('clang++', '-print-prog-name=ranlib', stdout=script('ranlib.py'))

if allow_properties('variant=debug', 'link=shared', 'threading=single', 'runtime-link=shared'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=release', 'link=shared', 'threading=single', 'runtime-link=shared', 'strip=on'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O3', '-Wno-inline', '-Wall', '-fPIC', '-DNDEBUG', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/release/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O3', '-Wno-inline', '-Wall', '-fPIC', '-DNDEBUG', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/release/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=debug', 'link=shared', 'threading=multi', 'runtime-link=shared'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/threading-multi/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/threading-multi/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=debug', 'link=static', 'threading=single', 'runtime-link=shared'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=debug', 'link=static', 'threading=single', 'runtime-link=static'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/runtime-link-static/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/runtime-link-static/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=debug', 'link=shared', 'threading=single', 'runtime-link=shared', 'architecture=x86', 'address-model=32'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-march=i686', '-m32', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-Wall', '-g', '-march=i686', '-m32', '-fPIC', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/main.o'), input_file(source='main.cpp'))

if allow_properties('variant=debug', 'link=shared', 'threading=single', 'runtime-link=shared', 'rtti=off', 'exception-handling=off'):
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-fno-rtti', '-fno-exceptions', '-Wall', '-g', '-fPIC', '-D_NO_RTTI', '-D_NO_EX=1', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/lib.o'), input_file(source='lib.cpp'))
    command('clang++', unordered(ordered('-x', 'c++'), '-O0', '-fno-inline', '-fno-rtti', '-fno-exceptions', '-Wall', '-g', '-fPIC', '-D_NO_RTTI', '-D_NO_EX=1', '-c'), '-o', output_file('bin/clang-vxworks-4.0.1/debug/main.o'), input_file(source='main.cpp'))

main()
