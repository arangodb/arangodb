#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(test/extras/include/boost/beast extras)
GroupSources(subtree/unit_test/include/boost/beast extras)
GroupSources(test/extern/zlib-1.2.11 zlib)
GroupSources(test/bench/zlib "/")

if (MSVC)
    set_source_files_properties (${ZLIB_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4127 /wd4131 /wd4244")
endif()

add_executable (bench-zlib
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${ZLIB_SOURCES}
    ${TEST_MAIN}
    Jamfile
    deflate_stream.cpp
    inflate_stream.cpp
)

set_property(TARGET bench-zlib PROPERTY FOLDER "tests-bench")
