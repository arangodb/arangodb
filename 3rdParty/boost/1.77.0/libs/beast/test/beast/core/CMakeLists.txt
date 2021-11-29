#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (test/beast/core "/")

add_executable (tests-beast-core
    ${BOOST_BEAST_FILES}
    Jamfile
    test_buffer.hpp
    file_test.hpp
    stream_tests.hpp
    test_handler.hpp
    _detail_base64.cpp
    _detail_bind_continuation.cpp
    _detail_buffer.cpp
    _detail_clamp.cpp
    _detail_get_io_context.cpp
    _detail_is_invocable.cpp
    _detail_read.cpp
    _detail_sha1.cpp
    _detail_tuple.cpp
    _detail_variant.cpp
    _detail_varint.cpp
    async_base.cpp
    basic_stream.cpp
    bind_handler.cpp
    buffer_traits.cpp
    buffered_read_stream.cpp
    buffers_adaptor.cpp
    buffers_cat.cpp
    buffers_prefix.cpp
    buffers_range.cpp
    buffers_suffix.cpp
    buffers_to_string.cpp
    detect_ssl.cpp
    error.cpp
    file.cpp
    file_base.cpp
    file_posix.cpp
    file_stdio.cpp
    file_win32.cpp
    flat_buffer.cpp
    flat_static_buffer.cpp
    flat_stream.cpp
    make_printable.cpp
    multi_buffer.cpp
    ostream.cpp
    rate_policy.cpp
    read_size.cpp
    role.cpp
    saved_handler.cpp
    span.cpp
    static_buffer.cpp
    static_string.cpp
    stream_traits.cpp
    string.cpp
    tcp_stream.cpp
)

target_link_libraries(tests-beast-core
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-beast-core PROPERTY FOLDER "tests")
