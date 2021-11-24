# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_mp11 VERSION 1.77.0 LANGUAGES CXX)

add_library(boost_mp11 INTERFACE)
add_library(Boost::mp11 ALIAS boost_mp11)

target_include_directories(boost_mp11 INTERFACE include)
target_compile_features(boost_mp11 INTERFACE cxx_alias_templates cxx_variadic_templates cxx_decltype)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

  # Fetch support files

  message(STATUS "Fetching BoostFetch.cmake")

  file(DOWNLOAD
    "https://raw.githubusercontent.com/boostorg/cmake/develop/include/BoostFetch.cmake"
    "${CMAKE_BINARY_DIR}/fetch_and_include/BoostFetch.cmake"
  )

  include("${CMAKE_BINARY_DIR}/fetch_and_include/BoostFetch.cmake")

  boost_fetch(boostorg/cmake TAG develop NO_ADD_SUBDIR)

  FetchContent_GetProperties(boostorg_cmake)

  list(APPEND CMAKE_MODULE_PATH ${boostorg_cmake_SOURCE_DIR}/include)

  # Enable testing

  include(CTest)
  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

  if(BUILD_TESTING)

    set(BUILD_TESTING OFF) # hide cache variable

    boost_fetch(boostorg/assert TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/config TAG develop EXCLUDE_FROM_ALL)
    boost_fetch(boostorg/core TAG develop EXCLUDE_FROM_ALL)

    unset(BUILD_TESTING)

  endif()

  # Do not use the default BoostInstall versioned layout on Windows when standalone
  set(BOOST_INSTALL_LAYOUT "system" CACHE STRING "Installation layout (versioned, tagged, or system)")

endif()

include(BoostInstall OPTIONAL RESULT_VARIABLE HAVE_BOOST_INSTALL)

if(HAVE_BOOST_INSTALL)

  boost_install(TARGETS boost_mp11 HEADER_DIRECTORY include/)

endif()

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
