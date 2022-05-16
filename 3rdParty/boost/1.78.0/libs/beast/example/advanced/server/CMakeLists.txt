#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/advanced/server "/")

add_executable (advanced-server
    ${BOOST_BEAST_FILES}
    Jamfile
    advanced_server.cpp
)

target_link_libraries(advanced-server
    lib-asio
    lib-beast)

set_property(TARGET advanced-server PROPERTY FOLDER "example-advanced-server")
