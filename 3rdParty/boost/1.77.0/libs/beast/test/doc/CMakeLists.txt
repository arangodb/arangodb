#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (example example)
GroupSources (test/doc "/")

add_executable (tests-doc
    ${BOOST_BEAST_FILES}
    Jamfile
    snippets.hpp
    snippets.ipp
    core_snippets.cpp
    core_1_refresher.cpp
    core_3_timeouts.cpp
    core_4_layers.cpp
    http_10_custom_parser.cpp
    http_examples.cpp
    http_snippets.cpp
    websocket_common.ipp
    websocket.cpp
    websocket_1_connecting.cpp
    websocket_2_handshaking.cpp
    websocket_3_decorator.cpp
    websocket_4_messages.cpp
    websocket_5_control_frames.cpp
    websocket_6_timeouts.cpp
    websocket_7_teardown.cpp
    websocket_8_notes.cpp
    exemplars.cpp
)

target_link_libraries(tests-doc
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-doc PROPERTY FOLDER "tests")
