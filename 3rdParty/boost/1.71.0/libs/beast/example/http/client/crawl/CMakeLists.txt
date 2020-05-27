#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/client/crawl "/")

add_executable (http-crawl
    ${BOOST_BEAST_FILES}
    Jamfile
    urls_large_data.hpp
    urls_large_data.cpp
    http_crawl.cpp
)

target_link_libraries(http-crawl
    lib-asio
    lib-beast)

set_property(TARGET http-crawl PROPERTY FOLDER "example-http-client")

