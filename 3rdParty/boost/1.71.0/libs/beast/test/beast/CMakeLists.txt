#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_definitions (-DBOOST_BEAST_ALLOW_DEPRECATED)

# For buffers_cat
add_definitions (-DBOOST_BEAST_TESTS)

add_subdirectory (_experimental)
add_subdirectory (core)
add_subdirectory (http)
add_subdirectory (ssl)
add_subdirectory (websocket)
add_subdirectory (zlib)

#-------------------------------------------------------------------------------

GroupSources (include/boost/beast beast)
GroupSources (test/extras/include/boost /)
GroupSources (test/beast /)

add_executable (tests-beast
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    Jamfile
    core.cpp
    http.cpp
    ssl.cpp
    version.cpp
    websocket.cpp
    zlib.cpp
)

target_link_libraries(tests-beast
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test)

set_property(TARGET tests-beast PROPERTY FOLDER "tests")
