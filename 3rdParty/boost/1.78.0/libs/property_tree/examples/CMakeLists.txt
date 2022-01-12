#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#

source_group("test" FILES
    custom_data_type.cpp
    debug_settings.cpp
    empty_ptree_trick.cpp
    info_grammar_spirit.cpp
    speed_test.cpp
)

macro(add_example)
    cmake_parse_arguments("addex" "" "NAME" "SRCS;DEPS" ${ARGN})
    if("${addex_NAME}" STREQUAL "")
        message(FATAL_ERROR "add_example: no NAME")
    endif()
    if("${addex_SRCS}" STREQUAL "")
        message(FATAL_ERROR "add_example: no SRCS")
    endif()
    message(STATUS "add_example: ${addex_NAME} SRCS: ${addex_SRCS} DEPS: ${addex_DEPS}")
    add_executable("${PROJECT_NAME}-example-${addex_NAME}"
        ${addex_SRCS})
    set_property(TARGET "${PROJECT_NAME}-example-${addex_NAME}" PROPERTY FOLDER "example")
    target_link_libraries("${PROJECT_NAME}-example-${addex_NAME}" PRIVATE ${addex_DEPS})
endmacro()
#

add_example(NAME custom_data_type SRCS custom_data_type.cpp DEPS Boost::property_tree)
add_example(NAME debug_settings SRCS debug_settings.cpp DEPS Boost::property_tree)
configure_file(debug_settings.xml debug_settings.xml COPYONLY)
add_example(NAME empty_ptree_trick SRCS empty_ptree_trick.cpp DEPS Boost::property_tree)
add_example(NAME info_grammar_spirit SRCS info_grammar_spirit.cpp DEPS Boost::property_tree)
add_example(NAME speed_test SRCS speed_test.cpp DEPS Boost::property_tree)
