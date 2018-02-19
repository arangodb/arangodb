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
    GroupSources(example/advanced/server-flex "/")

    add_executable (advanced-server-flex
        ${BOOST_BEAST_FILES}
        ${PROJECT_SOURCE_DIR}/example/common/detect_ssl.hpp
        ${PROJECT_SOURCE_DIR}/example/common/server_certificate.hpp
        ${PROJECT_SOURCE_DIR}/example/common/ssl_stream.hpp
        Jamfile
        advanced_server_flex.cpp
    )

    set_property(TARGET advanced-server-flex PROPERTY FOLDER "example-advanced-server")
    
    target_link_libraries (advanced-server-flex
        ${OPENSSL_LIBRARIES}
        )

endif()
