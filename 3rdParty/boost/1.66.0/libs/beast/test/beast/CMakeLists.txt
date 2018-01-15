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
GroupSources(test/beast "/")

add_executable (tests-beast
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    core.cpp
    http.cpp
    version.cpp
    websocket.cpp
    zlib.cpp
)

add_subdirectory (core)
add_subdirectory (http)
add_subdirectory (websocket)
add_subdirectory (zlib)

set_property(TARGET tests-beast PROPERTY FOLDER "tests")
