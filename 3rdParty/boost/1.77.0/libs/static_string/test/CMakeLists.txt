#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/static_string
#

GroupSources (include/boost/static_string static_string)
GroupSources (test "/")

add_executable (tests
    ${PROJECT_FILES}
    Jamfile
    static_string.cpp
)

target_link_libraries(tests
    )

set_property(TARGET tests PROPERTY FOLDER "tests")
