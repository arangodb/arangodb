#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

if (OPENSSL_FOUND)
    GroupSources(include/boost/beast beast)
    GroupSources(example/common common)
    GroupSources(example/http/server/coro-ssl "/")

    add_executable (http-server-coro-ssl
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/server_certificate.hpp
        Jamfile
        http_server_coro_ssl.cpp
    )

    set_property(TARGET http-server-coro-ssl PROPERTY FOLDER "example-http-server")

    target_link_libraries (http-server-coro-ssl
        ${OPENSSL_LIBRARIES}
        )

endif()
