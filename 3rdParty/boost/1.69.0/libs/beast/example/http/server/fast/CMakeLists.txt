#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/common common)
GroupSources(example/http/server/fast "/")

add_executable (http-server-fast
    ${BOOST_BEAST_FILES}
    ${COMMON_FILES}
    Jamfile
    fields_alloc.hpp
    http_server_fast.cpp
)

set_property(TARGET http-server-fast PROPERTY FOLDER "example-http-server")
