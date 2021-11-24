#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#

file(GLOB_RECURSE BOOST_PROPERTY_TREE_TESTS_FILES CONFIGURE_DEPENDS Jamfile *.cpp *.hpp)
list(FILTER BOOST_PROPERTY_TREE_TESTS_FILES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/cmake_install_test/.*$")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${BOOST_PROPERTY_TREE_TESTS_FILES})

macro(PTREE_TEST name)
    message(STATUS "ptree_test ${PROJECT_NAME}-${name} SRCS ${ARGN}")
    add_executable("${PROJECT_NAME}-${name}" ${ARGN})
    target_include_directories("${PROJECT_NAME}-${name}" PRIVATE .)
    target_link_libraries("${PROJECT_NAME}-${name}" PRIVATE Boost::property_tree Boost::serialization)
    add_test(NAME "${PROJECT_NAME}-${name}" COMMAND "${PROJECT_NAME}-${name}")
endmacro()

PTREE_TEST(test-multi test_multi_module1.cpp test_multi_module2.cpp)
PTREE_TEST(test-property-tree test_property_tree.cpp test_property_tree.hpp)
PTREE_TEST(test-info-parser test_info_parser.cpp)
PTREE_TEST(test-rapidxml test_rapidxml.cpp)
PTREE_TEST(test-json-parser test_json_parser.cpp)
PTREE_TEST(test-json-parser2 test_json_parser2.cpp)
PTREE_TEST(test-ini-parser test_ini_parser.cpp)
PTREE_TEST(test-xml-parser-rapidxml test_xml_parser_rapidxml.cpp)

#[[
add_executable(tests ${BOOST_PROPERTY_TREE_TESTS_FILES})
target_include_directories(tests PRIVATE .)
target_link_libraries(tests PRIVATE Boost::property_tree Boost::serialization Boost::unit_test_framework)
add_test(NAME property_tree-tests COMMAND tests)
]]

#[[
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES limits.cpp main.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../src PREFIX "" FILES ../src/src.cpp)
add_executable(limits limits.cpp main.cpp ../src/src.cpp Jamfile)

target_compile_features(limits PUBLIC cxx_constexpr)

target_include_directories(limits PRIVATE ../include .)
target_compile_definitions(limits PRIVATE
    BOOST_PROPERTY_TREE_MAX_STRING_SIZE=1000
    BOOST_PROPERTY_TREE_MAX_STRUCTURED_SIZE=20
    BOOST_PROPERTY_TREE_STACK_BUFFER_SIZE=256
    BOOST_PROPERTY_TREE_NO_LIB=1
)
]]

if(BOOST_SUPERPROJECT_VERSION)
#[[
    target_link_libraries(limits
        PRIVATE
            Boost::align
            Boost::assert
            Boost::config
            Boost::container
            Boost::exception
            Boost::system
            Boost::throw_exception
            Boost::utility
    )
]]
elseif(BOOST_PROPERTY_TREE_IN_BOOST_TREE)
#[[
    target_include_directories(limits PRIVATE ${BOOST_ROOT})
    target_link_directories(limits PRIVATE ${BOOST_ROOT}/stage/lib)
]]
else()
#[[
    target_link_libraries(limits
        PRIVATE
            Boost::system
            Boost::container
    )
]]
endif()

