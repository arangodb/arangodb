# Copyright 2018 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5)

project(BoostMp11 VERSION 1.71.0 LANGUAGES CXX)

add_library(boost_mp11 INTERFACE)
set_property(TARGET boost_mp11 PROPERTY EXPORT_NAME mp11)

add_library(Boost::mp11 ALIAS boost_mp11)

target_include_directories(boost_mp11 INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_compile_features(boost_mp11 INTERFACE cxx_alias_templates cxx_variadic_templates cxx_decltype)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    # --target check

    # `function` confuses FetchContent, sees empty CMAKE_CURRENT_LIST_DIR
    macro(fetch_and_include name)

        message(STATUS "Fetching ${name}")

        file(DOWNLOAD
            "https://raw.githubusercontent.com/boostorg/mincmake/master/${name}"
            "${CMAKE_BINARY_DIR}/fetch_and_include/${name}"
        )

        include("${CMAKE_BINARY_DIR}/fetch_and_include/${name}")

    endmacro()

    fetch_and_include(cmake/boost_fetch.cmake)
    fetch_and_include(cmake/boost_test.cmake)

    boost_fetch(boostorg/assert TAG develop)
    boost_fetch(boostorg/config TAG develop)
    boost_fetch(boostorg/core TAG develop)

    enable_testing()
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

    # --target install

    set(CONFIG_INSTALL_DIR lib/cmake/${PROJECT_NAME}-${PROJECT_VERSION})

    install(TARGETS boost_mp11 EXPORT ${PROJECT_NAME}Targets)
    install(EXPORT ${PROJECT_NAME}Targets DESTINATION ${CONFIG_INSTALL_DIR} NAMESPACE Boost:: FILE ${PROJECT_NAME}Config.cmake)

    install(DIRECTORY include/ DESTINATION include)

    include(CMakePackageConfigHelpers)

    # Mp11 is independent of 32/64, so this hack makes BoostMp11ConfigVersion.cmake skip the check
    set(OLD_CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" COMPATIBILITY AnyNewerVersion)
    set(CMAKE_SIZEOF_VOID_P ${OLD_CMAKE_SIZEOF_VOID_P})

    install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" DESTINATION ${CONFIG_INSTALL_DIR})

    #export(EXPORT ${PROJECT_NAME}Targets NAMESPACE Boost:: FILE ${PROJECT_NAME}Config.cmake)

endif()

if(COMMAND boost_test)

    add_subdirectory(test)

endif()
