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
    GroupSources(example/websocket/server/async-ssl "/")

    add_executable (websocket-server-async-ssl
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/server_certificate.hpp
        Jamfile
        websocket_server_async_ssl.cpp
    )

    set_property(TARGET websocket-server-async-ssl PROPERTY FOLDER "example-websocket-server")

    target_link_libraries (websocket-server-async-ssl
        ${OPENSSL_LIBRARIES}
        )

endif()
