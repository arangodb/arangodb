# Copyright 2018 Mike Dev
# Copyright 2018 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet

cmake_minimum_required(VERSION 3.5)

project(BoostAssert VERSION 1.71.0 LANGUAGES CXX)

add_library(boost_assert INTERFACE)
add_library(Boost::assert ALIAS boost_assert)

target_include_directories(boost_assert INTERFACE include)

target_link_libraries(boost_assert
    INTERFACE
        Boost::config
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    # `function` confuses FetchContent, sees empty CMAKE_CURRENT_LIST_DIR
    macro(fetch_and_include name)

        message(STATUS "Fetching ${name}")

        file(DOWNLOAD
            "https://raw.githubusercontent.com/pdimov/mincmake/master/${name}"
            "${CMAKE_BINARY_DIR}/fetch_and_include/${name}"
        )

        include("${CMAKE_BINARY_DIR}/fetch_and_include/${name}")

    endmacro()

    fetch_and_include(cmake/boost_fetch.cmake)
    fetch_and_include(cmake/boost_test.cmake)

    boost_fetch(boostorg/config TAG develop)
    boost_fetch(boostorg/core TAG develop)

    enable_testing()
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

endif()

if(COMMAND boost_test)

    add_subdirectory(test)

endif()
