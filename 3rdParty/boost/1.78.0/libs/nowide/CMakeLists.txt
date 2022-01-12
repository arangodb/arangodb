# Copyright 2019 - 2020 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt)

# Builds the libraries for Boost.Nowide
#
# Options:
# Boost_NOWIDE_INSTALL
# Boost_NOWIDE_WERROR
# BUILD_TESTING
#
# Created target: Boost::nowide
#
# When not using CMake to link against the shared library on windows define -DBOOST_NOWIDE_DYN_LINK

cmake_minimum_required(VERSION 3.9)
# Version number starts at 10 to avoid conflicts with Boost version
set(_version 11.1.3)
if(BOOST_SUPERPROJECT_SOURCE_DIR)
  set(_version ${BOOST_SUPERPROJECT_VERSION})
endif()
project(boost_nowide VERSION ${_version} LANGUAGES CXX)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  # Make sure all binarys (especially exe/dll) are in one directory for tests to work
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(BoostAddWarnings)
include(CheckCXXSourceCompiles)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CTest)
  set(def_INSTALL ON)
  set(def_WERROR ON)
elseif(BOOST_SUPERPROJECT_SOURCE_DIR)
  set(Boost_NOWIDE_INSTALL OFF) # Done by the superproject
  set(def_WERROR ON)
else()
  set(def_INSTALL OFF)
  set(def_WERROR OFF)
endif()

if(NOT BOOST_SUPERPROJECT_SOURCE_DIR)
  option(Boost_NOWIDE_INSTALL "Install library" "${def_INSTALL}")
endif()
option(Boost_NOWIDE_WERROR "Treat warnings as errors" "${def_WERROR}")


file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/check_lfs_support.cpp lfsSource)
check_cxx_source_compiles("${lfsSource}" BOOST_NOWIDE_HAS_LFS)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/check_attribute_init_priority.cpp attributeInitPrioritySource)
check_cxx_source_compiles("${attributeInitPrioritySource}" BOOST_NOWIDE_HAS_INIT_PRIORITY)

# Using glob here is ok as it is only for headers
file(GLOB_RECURSE headers include/*.hpp)
add_library(boost_nowide src/console_buffer.cpp src/cstdio.cpp src/cstdlib.cpp src/filebuf.cpp src/iostream.cpp src/stat.cpp ${headers})
add_library(Boost::nowide ALIAS boost_nowide)
set_target_properties(boost_nowide PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    EXPORT_NAME nowide
)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_nowide PUBLIC BOOST_NOWIDE_DYN_LINK)
endif()
if(NOT BOOST_NOWIDE_HAS_LFS)
  target_compile_definitions(boost_nowide PRIVATE BOOST_NOWIDE_NO_LFS)
endif()
if(BOOST_NOWIDE_HAS_INIT_PRIORITY)
  target_compile_definitions(boost_nowide PRIVATE BOOST_NOWIDE_HAS_INIT_PRIORITY)
endif()
target_compile_definitions(boost_nowide PUBLIC BOOST_NOWIDE_NO_LIB)
target_include_directories(boost_nowide PUBLIC include)
boost_add_warnings(boost_nowide pedantic ${Boost_NOWIDE_WERROR})
target_compile_features(boost_nowide PUBLIC cxx_std_11)

if(BOOST_SUPERPROJECT_SOURCE_DIR OR TARGET Boost::config)
  target_link_libraries(boost_nowide PUBLIC
    Boost::config
  )
else()
  if(NOT TARGET Boost::boost)
    # Default boost libs are static on windows and dynamic on linux
    if(WIN32 AND NOT DEFINED Boost_USE_STATIC_LIBS)
      set(Boost_USE_STATIC_LIBS ON)
    endif()
    find_package(Boost 1.56 REQUIRED)
  endif()
  target_link_libraries(boost_nowide PUBLIC Boost::boost)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

if(Boost_NOWIDE_INSTALL)
  include(InstallTargets)
  install_targets(TARGETS boost_nowide NAMESPACE Boost CONFIG_FILE ${PROJECT_SOURCE_DIR}/Config.cmake.in)
endif()
