#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources (include/boost/beast beast)
GroupSources (example/common common)
GroupSources (test/extras/include/boost/beast extras)
GroupSources (test/example/common "/")

add_executable (tests-example-common
    ${BOOST_BEAST_FILES}
    ${COMMON_FILES}
    ${EXTRAS_FILES}
    Jamfile
    root_certificates.cpp
    server_certificate.cpp
)

target_link_libraries(tests-example-common
    lib-asio
    lib-asio-ssl
    lib-beast
    lib-test
    )

set_property(TARGET tests-example-common PROPERTY FOLDER "tests")