#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#

source_group("" FILES
    Jamfile
    bench.cpp
)

add_executable(bench
    Jamfile
    bench.cpp
)

target_include_directories(bench PRIVATE ../test)
target_link_libraries(bench PRIVATE Boost::json)
