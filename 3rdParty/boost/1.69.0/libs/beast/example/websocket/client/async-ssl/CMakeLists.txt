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
    GroupSources(example/websocket/client/async-ssl "/")

    add_executable (websocket-client-async-ssl
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/root_certificates.hpp
        Jamfile
        websocket_client_async_ssl.cpp
    )

    set_property(TARGET websocket-client-async-ssl PROPERTY FOLDER "example-websocket-client")

    target_link_libraries (websocket-client-async-ssl
        ${OPENSSL_LIBRARIES}
        )

endif()
