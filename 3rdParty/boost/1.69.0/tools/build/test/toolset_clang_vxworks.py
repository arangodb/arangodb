#!/usr/bin/python
#
# Copyright 2018 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# validates the clang_vxworks toolset using a mock of clang

from TestToolset import test_toolset

test_toolset("clang-vxworks", "4.0.1", [
    ["target-os=vxworks"],
    ["target-os=vxworks", "release", "strip=on", "linkflags=-t"],
    ["target-os=vxworks", "threading=multi"],
    ["target-os=vxworks", "link=static"],
    ["target-os=vxworks", "link=static", "runtime-link=static"],
    ["target-os=vxworks", "architecture=x86", "address-model=32"],
    ["target-os=vxworks", "rtti=off", "exception-handling=off"]])
