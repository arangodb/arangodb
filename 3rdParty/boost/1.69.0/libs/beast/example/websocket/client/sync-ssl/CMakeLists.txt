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
    GroupSources(example/websocket/client/sync-ssl "/")

    add_executable (websocket-client-sync-ssl
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/root_certificates.hpp
        Jamfile
        websocket_client_sync_ssl.cpp
    )

    set_property(TARGET websocket-client-sync-ssl PROPERTY FOLDER "example-websocket-client")

    target_link_libraries (websocket-client-sync-ssl
        ${OPENSSL_LIBRARIES}
        )

endif()
