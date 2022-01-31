#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/json
#

cmake_minimum_required(VERSION 3.8...3.16)

set(BOOST_JSON_VERSION 2)
if(BOOST_SUPERPROJECT_VERSION)
    set(BOOST_JSON_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif()

project(boost_json VERSION "${BOOST_JSON_VERSION}" LANGUAGES CXX)


set(BOOST_JSON_IS_ROOT OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_JSON_IS_ROOT ON)
endif()

if(BOOST_JSON_IS_ROOT)
    include(CTest)
endif()
if(NOT BOOST_SUPERPROJECT_VERSION)
    option(BOOST_JSON_STANDALONE "Build boost::json as a standalone library" OFF)
    option(BOOST_JSON_INSTALL "Install boost::json files" ON)
    option(BOOST_JSON_BUILD_TESTS "Build boost::json tests" ${BUILD_TESTING})
    option(BOOST_JSON_BUILD_FUZZERS "Build boost::json fuzzers" ${BOOST_JSON_IS_ROOT})
    option(BOOST_JSON_BUILD_EXAMPLES "Build boost::json examples" ${BOOST_JSON_IS_ROOT})
    option(BOOST_JSON_BUILD_BENCHMARKS "Build boost::json benchmarks" OFF)
else()
    set(BOOST_JSON_BUILD_TESTS ${BUILD_TESTING})
endif()


include(GNUInstallDirs)
if(BOOST_JSON_IS_ROOT AND NOT BOOST_JSON_STANDALONE)
    #
    # Building inside Boost tree, but as a separate project e.g. on Travis or
    # other CI, or when producing Visual Studio Solution and Projects.

    set(BOOST_INCLUDE_LIBRARIES json)
    set(BOOST_EXCLUDE_LIBRARIES json)

    set(CMAKE_FOLDER _deps)
    add_subdirectory(../.. _deps/boost EXCLUDE_FROM_ALL)
    unset(CMAKE_FOLDER)
endif()


function(boost_json_setup_properties target)
    target_compile_features(${target} PUBLIC cxx_constexpr)
    target_compile_definitions(${target} PUBLIC BOOST_JSON_NO_LIB=1)

    if(BOOST_SUPERPROJECT_VERSION)
        target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    else()
        target_include_directories(${target}
            PUBLIC
                "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )
    endif()

  if(BOOST_JSON_STANDALONE)
      target_compile_definitions(${target} PUBLIC BOOST_JSON_STANDALONE)
      target_compile_features(${target} PUBLIC cxx_std_17)
  else()
      target_link_libraries(${target}
          PUBLIC
              Boost::align
              Boost::assert
              Boost::config
              Boost::container
              Boost::exception
              Boost::mp11
              Boost::system
              Boost::throw_exception
              Boost::utility
      )
  endif()
endfunction()


file(GLOB_RECURSE BOOST_JSON_HEADERS CONFIGURE_DEPENDS
    include/boost/*.hpp
    include/boost/*.ipp
    include/boost/*.natvis
)

set(BOOST_JSON_SOURCES src/src.cpp)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost PREFIX "" FILES ${BOOST_JSON_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "" FILES ${BOOST_JSON_SOURCES})


add_library(boost_json ${BOOST_JSON_HEADERS} ${BOOST_JSON_SOURCES})
add_library(Boost::json ALIAS boost_json)
boost_json_setup_properties(boost_json)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_json PUBLIC BOOST_JSON_DYN_LINK=1)
else()
    target_compile_definitions(boost_json PUBLIC BOOST_JSON_STATIC_LINK=1)
endif()


if(BOOST_JSON_INSTALL AND NOT BOOST_SUPERPROJECT_VERSION)
    install(TARGETS boost_json
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()


if(BOOST_JSON_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(BOOST_JSON_BUILD_FUZZERS)
    add_subdirectory(fuzzing)
endif()

if(BOOST_JSON_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(BOOST_JSON_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
