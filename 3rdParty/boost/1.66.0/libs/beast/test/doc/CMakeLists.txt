#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(example example)
GroupSources(test/extras/include/boost/beast extras)
GroupSources(subtree/unit_test/include/boost/beast extras)
GroupSources(include/boost/beast beast)
GroupSources(test/doc "/")

add_executable (tests-doc
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    core_examples.cpp
    core_snippets.cpp
    http_examples.cpp
    http_snippets.cpp
    websocket_snippets.cpp
    exemplars.cpp
)

set_property(TARGET tests-doc PROPERTY FOLDER "tests")
