#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/extras/include/boost/beast extras)
GroupSources (test/beast/websocket "/")

add_executable (tests-beast-websocket
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    Jamfile
    _detail_decorator.cpp
    _detail_prng.cpp
    _detail_impl_base.cpp
    test.hpp
    _detail_prng.cpp
    accept.cpp
    close.cpp
    error.cpp
    frame.cpp
    handshake.cpp
    option.cpp
    ping.cpp
    read1.cpp
    read2.cpp
    read3.cpp
    rfc6455.cpp
    ssl.cpp
    stream.cpp
    stream_base.cpp
    stream_explicit.cpp
    stream_fwd.cpp
    teardown.cpp
    timer.cpp
    utf8_checker.cpp
    write.cpp
)

target_link_libraries(tests-beast-websocket
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-beast-websocket PROPERTY FOLDER "tests")
