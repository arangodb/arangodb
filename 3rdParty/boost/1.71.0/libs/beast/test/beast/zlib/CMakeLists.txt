#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/extern/zlib-1.2.11 zlib)
GroupSources (test/beast/zlib "/")

if (MSVC)
    set_source_files_properties (${ZLIB_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4127 /wd4131 /wd4244")
endif()

add_executable (tests-beast-zlib
    ${BOOST_BEAST_FILES}
    ${ZLIB_SOURCES}
    Jamfile
    error.cpp
    deflate_stream.cpp
    inflate_stream.cpp
    zlib.cpp
)

target_link_libraries(tests-beast-zlib
    lib-asio
    lib-beast
    lib-test
    )

set_property(TARGET tests-beast-zlib PROPERTY FOLDER "tests")
