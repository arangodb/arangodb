#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# validates the clang_linux toolset using a mock of clang

from TestToolset import test_toolset

test_toolset("clang-linux", "3.9.0", [
    ["target-os=linux"],
    ["target-os=linux", "release", "strip=on"],
    ["target-os=linux", "threading=multi"],
    ["target-os=linux", "link=static"],
    ["target-os=linux", "link=static", "runtime-link=static"],
    ["target-os=linux", "architecture=x86", "address-model=32"]])
