#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

command('clang++', '-print-prog-name=ar', stdout=script('ar.py'))
command('clang++', '-print-prog-name=ranlib', stdout=script('ranlib.py'))

# all builds are multi-threaded for darwin
if allow_properties("variant=debug", "link=shared", "runtime-link=shared"):
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-fPIC'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', '@rpath/libl1.dylib', input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/lib.o'), unordered('-g', '-fPIC'))
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-fPIC'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/libl1.dylib'), unordered('-g', '-fPIC'))

if allow_properties("variant=release", "link=shared", "runtime-link=shared"):
    command('clang++', '-x', 'c++', unordered('-O3', '-Wno-inline', '-Wall', '-fPIC'), '-DNDEBUG', '-c', '-o', output_file('bin/clang-darwin-3.9.0/release/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-v', '-o', output_file('bin/clang-darwin-3.9.0/release/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', '@rpath/libl1.dylib', input_file('bin/clang-darwin-3.9.0/release/target-os-darwin/lib.o'), '-fPIC')
    command('clang++', '-x', 'c++', unordered('-O3', '-Wno-inline', '-Wall', '-fPIC'), '-DNDEBUG', '-c', '-o', output_file('bin/clang-darwin-3.9.0/release/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-v', '-o', output_file('bin/clang-darwin-3.9.0/release/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/release/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/release/target-os-darwin/libl1.dylib'), '-fPIC')

if allow_properties("variant=debug", "link=static", "runtime-link=shared"):
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/libl1.a'), '-g')

if allow_properties("variant=debug", "link=static", "runtime-link=static"):
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/libl1.a'), unordered('-g', '-static'))

if allow_properties("variant=debug", "link=shared", "runtime-link=shared", "architecture=x86", "address-model=32"):
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-march=i686', '-fPIC', '-m32'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', '@rpath/libl1.dylib', input_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/lib.o'), unordered('-g', '-march=i686', '-fPIC', '-m32'))
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-march=i686', '-fPIC', '-m32'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/debug/x86/target-os-darwin/libl1.dylib'), unordered('-g', '-march=i686', '-fPIC', '-m32'))

if allow_properties("variant=debug", "link=shared", "runtime-link=shared", "cxxstd=latest"):
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-std=c++1z'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', '@rpath/libl1.dylib', input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/lib.o'), unordered('-g', '-fPIC', '-std=c++1z'))
    command('clang++', '-x', 'c++', unordered('-O0', '-fno-inline', '-Wall', '-g', '-fPIC', '-std=c++1z'), '-c', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('clang++', '-o', output_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/test'), input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/main.o'), input_file('bin/clang-darwin-3.9.0/debug/target-os-darwin/libl1.dylib'), unordered('-g', '-fPIC', '-std=c++1z'))

main()
