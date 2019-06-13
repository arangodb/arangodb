#!/usr/bin/python
#
# Copyright 2018 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

if allow_properties('variant=debug', 'link=shared', 'threading=single', 'runtime-link=shared'):
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/libl1.so'), input_file('bin/clang-vxworks-4.0.1/debug/lib.o'), unordered('-g', '-fPIC'), '-fpic', '-shared', '-non-static')
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/test'), input_file('bin/clang-vxworks-4.0.1/debug/main.o'), input_file('bin/clang-vxworks-4.0.1/debug/libl1.so'), unordered('-g', '-fPIC'))

if allow_properties('variant=release', 'link=shared', 'threading=single', 'runtime-link=shared', 'strip=on'):
    command('ld', '-t', '-o', output_file('bin/clang-vxworks-4.0.1/release/libl1.so'), input_file('bin/clang-vxworks-4.0.1/release/lib.o'), unordered('-fPIC', '-Wl,--strip-all'), '-fpic', '-shared', '-non-static')
    command('ld', '-t', '-o', output_file('bin/clang-vxworks-4.0.1/release/test'), input_file('bin/clang-vxworks-4.0.1/release/main.o'), input_file('bin/clang-vxworks-4.0.1/release/libl1.so'), unordered('-fPIC', '-Wl,--strip-all'))

if allow_properties('variant=debug', 'link=shared', 'threading=multi', 'runtime-link=shared'):
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/threading-multi/libl1.so'), input_file('bin/clang-vxworks-4.0.1/debug/threading-multi/lib.o'), unordered('-g', '-fPIC'), '-fpic', '-shared', '-non-static')
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/threading-multi/test'), input_file('bin/clang-vxworks-4.0.1/debug/threading-multi/main.o'), input_file('bin/clang-vxworks-4.0.1/debug/threading-multi/libl1.so'), unordered('-g', '-fPIC'))

if allow_properties('variant=debug', 'link=static', 'threading=single', 'runtime-link=shared'):
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/test'), input_file('bin/clang-vxworks-4.0.1/debug/link-static/main.o'), input_file('bin/clang-vxworks-4.0.1/debug/link-static/libl1.a'), '-g')

if allow_properties('variant=debug', 'link=static', 'threading=single', 'runtime-link=static'):
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/link-static/runtime-link-static/test'), input_file('bin/clang-vxworks-4.0.1/debug/link-static/runtime-link-static/main.o'), input_file('bin/clang-vxworks-4.0.1/debug/link-static/runtime-link-static/libl1.a'), unordered('-g'))

if allow_properties('variant=debug', 'link=shared', 'threading=single', 'runtime-link=shared', 'architecture=x86', 'address-model=32'):
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/libl1.so'), input_file('bin/clang-vxworks-4.0.1/debug/lib.o'), unordered('-g', '-march=i686', '-fPIC', '-m32'), '-fpic', '-shared', '-non-static')
    command('ld', '-o', output_file('bin/clang-vxworks-4.0.1/debug/test'), input_file('bin/clang-vxworks-4.0.1/debug/main.o'), input_file('bin/clang-vxworks-4.0.1/debug/libl1.so'), unordered('-g', '-march=i686', '-fPIC', '-m32'))

main()
