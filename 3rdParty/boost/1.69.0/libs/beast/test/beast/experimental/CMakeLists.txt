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
GroupSources(test/beast/experimental "/")

add_executable (tests-beast-experimental
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    error.cpp
    flat_stream.cpp
    icy_stream.cpp
    ssl_stream.cpp
    stream.cpp
    timeout_socket.cpp
    timeout_service.cpp
    timeout_work_guard.cpp
)

set_property(TARGET tests-beast-experimental PROPERTY FOLDER "tests")
