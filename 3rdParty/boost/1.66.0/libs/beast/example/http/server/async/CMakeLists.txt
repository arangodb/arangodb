#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/server/async "/")

add_executable (http-server-async
    ${BOOST_BEAST_FILES}
    Jamfile
    http_server_async.cpp
)

set_property(TARGET http-server-async PROPERTY FOLDER "example-http-server")
