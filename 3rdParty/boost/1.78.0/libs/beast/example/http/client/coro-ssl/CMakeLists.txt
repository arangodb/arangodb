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
    GroupSources(example/http/client/coro-ssl "/")

    add_executable (http-client-coro-ssl
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/root_certificates.hpp
        Jamfile
        http_client_coro_ssl.cpp
    )

    set_property(TARGET http-client-coro-ssl PROPERTY FOLDER "example-http-client")

    target_link_libraries (http-client-coro-ssl
        OpenSSL::SSL OpenSSL::Crypto
        lib-asio
        lib-asio-ssl
        lib-beast
        )

endif()
