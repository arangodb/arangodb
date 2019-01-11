#!/usr/bin/python
#
# Copyright 2017 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# validates the gcc toolset using a mock of gcc

from TestToolset import test_toolset

test_toolset("gcc", "4.8.3", [
    ["target-os=linux"],
    ["target-os=linux", "release"],
    ["target-os=linux", "threading=multi"],
    ["target-os=linux", "link=static"],
    ["target-os=linux", "link=static", "runtime-link=static"],
    ["target-os=linux", "cxxstd=latest"]])

test_toolset("gcc", "4.2.1", [
    ["target-os=darwin"],
    ["target-os=darwin", "release"],
    ["target-os=darwin", "threading=multi"],
    ["target-os=darwin", "link=static"],
    ["target-os=darwin", "link=static", "runtime-link=static"]])
