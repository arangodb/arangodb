#
# Copyright (c) 2020 Paul Dreik (github@pauldreik.se)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#

source_group("" FILES
    fuzz_basic_parser.cpp
    fuzz_parse.cpp
    fuzz_parser.cpp
)

# The fuzzers are built as libraries, to make
# sure they are built as part of normal builds
# preventing bit rot after refactorings etc.

# They are not built as executables, because they would need a main()
# function which (in a fuzzing build) already is provided
# by the fuzzer engine (e.g. libfuzzer).

add_library(fuzzerlib_basic_parser fuzz_basic_parser.cpp)
set_property(TARGET fuzzerlib_basic_parser PROPERTY FOLDER "fuzzing")
target_link_libraries(fuzzerlib_basic_parser PRIVATE Boost::json)

add_library(fuzzerlib_parse fuzz_parse.cpp)
set_property(TARGET fuzzerlib_parse PROPERTY FOLDER "fuzzing")
target_link_libraries(fuzzerlib_parse PRIVATE Boost::json)

add_library(fuzzerlib_parser fuzz_parser.cpp)
set_property(TARGET fuzzerlib_parser PROPERTY FOLDER "fuzzing")
target_link_libraries(fuzzerlib_parser PRIVATE Boost::json)

