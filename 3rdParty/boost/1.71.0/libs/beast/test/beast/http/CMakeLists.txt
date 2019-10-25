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
GroupSources (test/beast/http "/")

add_executable (tests-beast-http
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    Jamfile
    message_fuzz.hpp
    test_parser.hpp
    basic_dynamic_body.cpp
    basic_file_body.cpp
    basic_parser.cpp
    buffer_body.cpp
    chunk_encode.cpp
    dynamic_body.cpp
    empty_body.cpp
    error.cpp
    field.cpp
    fields.cpp
    file_body.cpp
    message.cpp
    parser.cpp
    read.cpp
    rfc7230.cpp
    serializer.cpp
    span_body.cpp
    status.cpp
    string_body.cpp
    type_traits.cpp
    vector_body.cpp
    verb.cpp
    write.cpp
)

target_link_libraries(tests-beast-http
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-beast-http PROPERTY FOLDER "tests")
