#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

command('icc', '-print-prog-name=ar', stdout=script('ar.py'))
command('icc', '-print-prog-name=ranlib', stdout=script('ranlib.py'))

# all builds are multi-threaded for darwin
if allow_properties("variant=debug", "link=shared", "runtime-link=shared"):
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0', '-fPIC'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', 'libl1.dylib', input_file('bin/intel-darwin-10.2/debug/target-os-darwin/lib.o'), unordered('-g', ordered('-shared-intel', '-lstdc++', '-lpthread'), '-fPIC'))
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0', '-fPIC'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/target-os-darwin/test'), input_file('bin/intel-darwin-10.2/debug/target-os-darwin/main.o'), input_file('bin/intel-darwin-10.2/debug/target-os-darwin/libl1.dylib'), unordered('-g', ordered('-shared-intel', '-lstdc++', '-lpthread'), '-fPIC'))

if allow_properties("variant=release", "link=shared", "runtime-link=shared"):
    command('icc', '-xc++', unordered('-O3', '-inline-level=2', '-w1', '-vec-report0', '-fPIC'), '-DNDEBUG', '-c', '-o', output_file('bin/intel-darwin-10.2/release/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/release/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', 'libl1.dylib', input_file('bin/intel-darwin-10.2/release/target-os-darwin/lib.o'), unordered(ordered('-shared-intel', '-lstdc++', '-lpthread'), '-fPIC'))
    command('icc', '-xc++', unordered('-O3', '-inline-level=2', '-w1', '-vec-report0', '-fPIC'), '-DNDEBUG', '-c', '-o', output_file('bin/intel-darwin-10.2/release/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/release/target-os-darwin/test'), input_file('bin/intel-darwin-10.2/release/target-os-darwin/main.o'), input_file('bin/intel-darwin-10.2/release/target-os-darwin/libl1.dylib'), unordered(ordered('-shared-intel', '-lstdc++', '-lpthread'), '-fPIC'))

if allow_properties("variant=debug", "link=static", "runtime-link=shared"):
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/test'), input_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/main.o'), input_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/libl1.a'), '-g', ordered('-shared-intel', '-lstdc++', '-lpthread'))

if allow_properties("variant=debug", "link=static", "runtime-link=static"):
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/test'), input_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/main.o'), input_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/libl1.a'), unordered('-g', ordered('-static', '-static-intel', '-lstdc++', '-lpthread'), '-static'))

if allow_properties("variant=debug", "link=shared", "runtime-link=shared", "architecture=x86", "address-model=32"):
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0', '-march=i686', '-fPIC', '-m32'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/lib.o'), input_file(source='lib.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/libl1.dylib'), '-single_module', '-dynamiclib', '-install_name', 'libl1.dylib', input_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/lib.o'), unordered('-g', ordered('-shared-intel', '-lstdc++', '-lpthread'), '-march=i686', '-fPIC', '-m32'))
    command('icc', '-xc++', unordered('-O0', '-inline-level=0', '-w1', '-g', '-vec-report0', '-march=i686', '-fPIC', '-m32'), '-c', '-o', output_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/main.o'), input_file(source='main.cpp'))
    command('icc', '-o', output_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/test'), input_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/main.o'), input_file('bin/intel-darwin-10.2/debug/x86/target-os-darwin/libl1.dylib'), unordered('-g', ordered('-shared-intel', '-lstdc++', '-lpthread'), '-march=i686', '-fPIC', '-m32'))

main()
