#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#


if(NOT TARGET tests)
    add_custom_target(tests)
    set_property(TARGET tests PROPERTY FOLDER _deps)
endif()


file(GLOB_RECURSE BOOST_JSON_TESTS_FILES CONFIGURE_DEPENDS Jamfile *.cpp *.hpp)
list(FILTER BOOST_JSON_TESTS_FILES EXCLUDE REGEX cmake_install_test/.*$)
list(FILTER BOOST_JSON_TESTS_FILES EXCLUDE REGEX standalone_with_boost.cpp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${BOOST_JSON_TESTS_FILES})
add_executable(boost_json-tests ${BOOST_JSON_TESTS_FILES})
target_include_directories(boost_json-tests PRIVATE .)
target_link_libraries(boost_json-tests PRIVATE Boost::json)
add_test(NAME boost_json-tests COMMAND boost_json-tests)
add_dependencies(tests boost_json-tests)


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES limits.cpp main.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../src PREFIX "" FILES ../src/src.cpp)
add_executable(boost_json-limits limits.cpp main.cpp ../src/src.cpp Jamfile)
boost_json_setup_properties(boost_json-limits)

target_compile_definitions(boost_json-limits PRIVATE
    BOOST_JSON_MAX_STRING_SIZE=1000
    BOOST_JSON_MAX_STRUCTURED_SIZE=20
    BOOST_JSON_STACK_BUFFER_SIZE=256
    BOOST_JSON_NO_LIB=1
)

add_test(NAME boost_json-limits COMMAND boost_json-limits)
add_dependencies(tests boost_json-limits)


if(NOT BOOST_JSON_STANDALONE)
    add_library(boost_json-standalone_with_boost STATIC standalone_with_boost.cpp)
    target_compile_definitions(boost_json-standalone_with_boost PRIVATE BOOST_JSON_STANDALONE)
    target_compile_features(boost_json-standalone_with_boost PRIVATE cxx_std_17)
    target_link_libraries(boost_json-standalone_with_boost PRIVATE Boost::assert)
    boost_json_setup_properties(boost_json-standalone_with_boost)
    add_dependencies(tests boost_json-standalone_with_boost)
endif()
