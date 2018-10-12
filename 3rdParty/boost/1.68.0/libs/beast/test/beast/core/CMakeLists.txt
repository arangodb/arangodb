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
GroupSources(test/beast/core "/")

add_executable (tests-beast-core
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    buffer_test.hpp
    file_test.hpp
    bind_handler.cpp
    buffered_read_stream.cpp
    buffers_adapter.cpp
    buffers_cat.cpp
    buffers_prefix.cpp
    buffers_suffix.cpp
    buffers_to_string.cpp
    error.cpp
    file.cpp
    file_posix.cpp
    file_stdio.cpp
    file_win32.cpp
    flat_buffer.cpp
    flat_static_buffer.cpp
    handler_ptr.cpp
    multi_buffer.cpp
    ostream.cpp
    read_size.cpp
    span.cpp
    static_string.cpp
    string.cpp
    static_buffer.cpp
    string_param.cpp
    type_traits.cpp
    detail/base64.cpp
    detail/clamp.cpp
    detail/empty_base_optimization.cpp
    detail/sha1.cpp
    detail/variant.cpp
    detail/varint.cpp
)

set_property(TARGET tests-beast-core PROPERTY FOLDER "tests")
