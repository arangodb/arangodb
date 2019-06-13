#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# validates the clang-darwin toolset using a mock of clang

from TestToolset import test_toolset

test_toolset("clang-darwin", "3.9.0", [
    ["target-os=darwin"],
    ["target-os=darwin", "release", "strip=on", "linkflags=-v"],
    ["target-os=darwin", "threading=multi"],
    ["target-os=darwin", "link=static"],
    ["target-os=darwin", "link=static", "runtime-link=static"],
    ["target-os=darwin", "architecture=x86", "address-model=32"],
    ["target-os=darwin", "cxxstd=latest"]])
