#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(test/extras/include/boost/beast extras)
GroupSources(subtree/unit_test/include/boost/beast extras)
GroupSources(include/boost/beast beast)
GroupSources(test/beast/http "/")
GroupSources(test/bench/parser "/")

add_executable (bench-parser
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    ${PROJECT_SOURCE_DIR}/test/beast/http/message_fuzz.hpp
    nodejs_parser.hpp
    nodejs_parser.cpp
    bench_parser.cpp
)

set_property(TARGET bench-parser PROPERTY FOLDER "tests-bench")
