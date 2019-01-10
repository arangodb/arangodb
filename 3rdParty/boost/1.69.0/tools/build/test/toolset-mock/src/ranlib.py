#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from MockProgram import *

command('ranlib', input_file('bin/gcc-gnu-4.8.3/debug/link-static/libl1.a'))
command('ranlib', input_file('bin/gcc-gnu-4.8.3/debug/link-static/runtime-link-static/libl1.a'))
command('ranlib', input_file('bin/gcc-darwin-4.2.1/debug/link-static/target-os-darwin/libl1.a'))
command('ranlib', input_file('bin/gcc-darwin-4.2.1/debug/link-static/runtime-link-static/target-os-darwin/libl1.a'))
command('ranlib', input_file('bin/clang-darwin-3.9.0/debug/link-static/target-os-darwin/libl1.a'))
command('ranlib', input_file('bin/clang-darwin-3.9.0/debug/link-static/runtime-link-static/target-os-darwin/libl1.a'))
command('ranlib', '-cs', input_file('bin/intel-darwin-10.2/debug/link-static/target-os-darwin/libl1.a'))
command('ranlib', '-cs', input_file('bin/intel-darwin-10.2/debug/link-static/runtime-link-static/target-os-darwin/libl1.a'))
command('ranlib', input_file('bin/clang-linux-3.9.0/debug/link-static/libl1.a'))
command('ranlib', input_file('bin/clang-linux-3.9.0/debug/link-static/runtime-link-static/libl1.a'))

main()
