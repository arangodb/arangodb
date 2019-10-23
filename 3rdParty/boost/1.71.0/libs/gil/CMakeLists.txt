#
# Copyright (c) 2017-2018 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# **WARNING:**
# The CMake configuration is only provided for convenience
# of contributors. It does not export or install any targets,
# deploy config files or support subproject workflow.
#
cmake_minimum_required(VERSION 3.10)

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------
option(GIL_BUILD_EXAMPLES "Build examples" ON)
option(GIL_BUILD_HEADERS_TESTS "Enable self-contained header tests" ON)
option(GIL_ENABLE_EXT_IO "Enable IO extension, tests and examples (require libjpeg, libpng, libtiff)" ON)
option(GIL_ENABLE_EXT_NUMERIC "Enable Numeric extension, tests and examples" ON)
option(GIL_ENABLE_EXT_TOOLBOX "Enable Toolbox extension, tests and examples" ON)
option(GIL_USE_CONAN "Use Conan to install dependencies" OFF)
option(GIL_USE_CLANG_TIDY "Set CMAKE_CXX_CLANG_TIDY property on targets to enable clang-tidy linting" OFF)
option(GIL_DOWNLOAD_FINDBOOST "Download FindBoost.cmake from latest CMake release" OFF)
set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard version to use (default is 11)")

#-----------------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------------
file(STRINGS include/boost/gil/version.hpp
  BOOST_GIL_LIB_VERSION
  REGEX "#define BOOST_GIL_LIB_VERSION ")

if(BOOST_GIL_LIB_VERSION)
  string(REGEX REPLACE ".*\"(.*)\".*" "\\1" BOOST_GIL_LIB_VERSION "${BOOST_GIL_LIB_VERSION}")
  string(REPLACE "_" "." BOOST_GIL_LIB_VERSION "${BOOST_GIL_LIB_VERSION}")
else()
  message(SEND_ERROR "Cannot find version in '${CMAKE_CURRENT_SOURCE_DIR}/include/boost/gil/version.hpp'")
endif()

#-----------------------------------------------------------------------------
# Project
#-----------------------------------------------------------------------------
project(Boost.GIL
  LANGUAGES CXX
  VERSION ${BOOST_GIL_LIB_VERSION}
  DESCRIPTION "Boost.GIL - Generic Image Library | Requires C++11 since Boost 1.68")

message(STATUS "Boost.GIL: Version ${PROJECT_VERSION}")
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_BINARY_DIR}/cmake)

#-----------------------------------------------------------------------------
# C++ language version and compilation flags
#-----------------------------------------------------------------------------
message(STATUS "Boost.GIL: Require C++${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(gil_compile_options INTERFACE)

# See https://cmake.org/pipermail/cmake/2018-December/068716.html
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REGEX REPLACE "-W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif()

# See https://svn.boost.org/trac10/wiki/Guidelines/WarningsGuidelines

target_compile_options(gil_compile_options
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:-W4>
    $<$<CXX_COMPILER_ID:MSVC>:-bigobj>
    $<$<CXX_COMPILER_ID:MSVC>:-FC> # Need absolute path for __FILE__ used in tests
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-fstrict-aliasing -pedantic>)

# Do not mix warnings due to strict compilation level with linter warnings
if(NOT CMAKE_CXX_CLANG_TIDY)
  target_compile_options(gil_compile_options
    INTERFACE
      $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wconversion -Wextra -Wfloat-equal -Wshadow -Wsign-promo -Wstrict-aliasing -Wunused-parameter>)
endif()

target_compile_definitions(gil_compile_options
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_DEPRECATE>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<CXX_COMPILER_ID:MSVC>:BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE>)

#-----------------------------------------------------------------------------
# Dependency target
#-----------------------------------------------------------------------------
add_library(gil_dependencies INTERFACE)

#-----------------------------------------------------------------------------
# Dependency: Boost
# - look for stage Build
# - look for default installation location
# - look for location specified with BOOST_ROOT
#-----------------------------------------------------------------------------
if(CMAKE_VERSION VERSION_LESS 3.13 AND NOT GIL_DOWNLOAD_FINDBOOST)
  message(STATUS "Boost.GIL: You are using CMake older than 3.13")
  message(STATUS "Boost.GIL: FindBoost.cmake has likely been updated to detect newer or even not yet released Boost")
  message(STATUS "Boost.GIL: Run CMake with -DGIL_DOWNLOAD_FINDBOOST=ON to get latest version of FindBoost.cmake")
  message(STATUS "Boost.GIL: WARNING:")
  message(STATUS "Boost.GIL:    Newer FindBoost.cmake may fail to find your Boost for many reasons.")
  message(STATUS "Boost.GIL:    For example, this may be due to unrecognised toolset eg. latest Visual Studio 2017.")
  message(STATUS "Boost.GIL:    Try run CMake with -DBoost_COMPILER=\"-vc141\" or value for your toolset.")
endif()

if(GIL_DOWNLOAD_FINDBOOST)
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/FindBoost.cmake")
    message(STATUS "Boost.GIL: Downloading FindBoost.cmake from https://gitlab.kitware.com/cmake/ release branch")
    file(DOWNLOAD
      "https://gitlab.kitware.com/cmake/cmake/raw/release/Modules/FindBoost.cmake"
      "${CMAKE_BINARY_DIR}/cmake/FindBoost.cmake")
  endif()
endif()

if(NOT DEFINED BOOST_ROOT AND NOT DEFINED ENV{BOOST_ROOT})
  message(STATUS "Boost.GIL: Looking for Boost from current source tree and libraries from stage.")
  message(STATUS "Boost.GIL: Disable stage look-up with passing -DBOOST_ROOT=/path/to/your/boost.")
  get_filename_component(_boost_root ../../ ABSOLUTE)
  if(EXISTS ${_boost_root}/boost-build.jam)
    set(BOOST_ROOT ${_boost_root})
    message(STATUS "Boost.GIL: Using Boost libraries from stage directory in BOOST_ROOT=${BOOST_ROOT}")
  endif()
endif()

set(Boost_DETAILED_FAILURE_MSG ON)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_STATIC_RUNTIME OFF)
endif()

find_package(Boost 1.65.0 REQUIRED
  COMPONENTS
    filesystem
    unit_test_framework)
message(STATUS "Boost.GIL: Using Boost_INCLUDE_DIRS=${Boost_INCLUDE_DIRS}")
message(STATUS "Boost.GIL: Using Boost_LIBRARY_DIRS=${Boost_LIBRARY_DIRS}")

target_link_libraries(gil_dependencies
  INTERFACE
    Boost::filesystem
    Boost::unit_test_framework)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  target_link_libraries(gil_dependencies INTERFACE Boost::disable_autolinking)
endif()

target_compile_definitions(gil_dependencies
  INTERFACE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:BOOST_TEST_DYN_LINK>)

#-----------------------------------------------------------------------------
# Dependency: libpng, libjpeg, libtiff, libraw via Vcpkg or Conan
#-----------------------------------------------------------------------------
if(GIL_ENABLE_EXT_IO)
  if(GIL_USE_CONAN)
    # Download automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
      message(STATUS "Boost.GIL: Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
      file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.13/conan.cmake"
        "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()

    # NOTE: See RelWithDebInfo for Release builds, http://docs.conan.io/en/latest/howtos/vs2017_cmake.html
    set(_build_type_saved ${CMAKE_BUILD_TYPE})
    if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
      set(CMAKE_BUILD_TYPE "Release")
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)
    conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP CMAKE_TARGETS)

    set(CMAKE_BUILD_TYPE ${_build_type_saved})
    unset(_build_type_saved)

    target_link_libraries(gil_dependencies
      INTERFACE
        CONAN_PKG::libjpeg
        CONAN_PKG::libpng
        CONAN_PKG::libtiff)
  else()
    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    find_package(TIFF REQUIRED)
    target_include_directories(gil_dependencies
      INTERFACE
        ${JPEG_INCLUDE_DIR})

    target_link_libraries(gil_dependencies
      INTERFACE
        ${JPEG_LIBRARIES}
        PNG::PNG
        TIFF::TIFF)

    if(UNIX)
      # Typically, Linux packages provide C++ stream interface for TIFF
      find_path(TIFFXX_INCLUDE_DIR NAMES tiffio.hxx)
      find_library(TIFFXX_LIBRARY NAMES tiffxx)
      target_include_directories(gil_dependencies INTERFACE ${TIFFXX_INCLUDE_DIR})
      target_link_libraries(gil_dependencies INTERFACE ${TIFFXX_LIBRARY})
    endif()

    # LibRaw is optional, because it is not easy to install pre-built libraw on Windows and Mac OSX
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/FindLibRaw.cmake")
      message(STATUS "Boost.GIL: Downloading FindLibRaw.cmake from https://github.com/LibRaw/LibRaw-cmake")
      file(DOWNLOAD
        "https://raw.githubusercontent.com/LibRaw/LibRaw-cmake/master/cmake/modules/FindLibRaw.cmake"
        "${CMAKE_BINARY_DIR}/cmake/FindLibRaw.cmake")
    endif()
    find_package(LibRaw)
    set(GIL_ENABLE_EXT_IO_RAW ${LibRaw_FOUND} CACHE BOOL "Enable IO RAW extension (requires libraw)" FORCE)
    if(GIL_ENABLE_EXT_IO_RAW)
      target_include_directories(gil_dependencies INTERFACE ${LibRaw_INCLUDE_DIR})
      target_link_libraries(gil_dependencies INTERFACE ${LibRaw_LIBRARIES})
      target_compile_definitions(gil_dependencies INTERFACE ${LibRaw_DEFINITIONS})
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# clang-tidy
# - default checks specified in .clang-tidy configuration file
#-----------------------------------------------------------------------------
if(GIL_USE_CLANG_TIDY AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.6)
  find_program(_clang_tidy
    NAMES clang-tidy-7 clang-tidy-6.0 clang-tidy-5.0 clang-tidy-4.0 clang-tidy
    DOC "Path to clang-tidy executable")

  if(_clang_tidy)
    message(STATUS "Boost.GIL: Configuring ${_clang_tidy} to run linting analysis for targets")
    set(CMAKE_CXX_CLANG_TIDY ${_clang_tidy})
  endif()
  unset(_clang_tidy)
endif()

#-----------------------------------------------------------------------------
# Common include directories
#
# The boostorg/gil repository includes must come first,
# before Boost includes from cloned Boost superproject or installed distribution.
# Otherwise IDEs may see the wrong file (ie. due to boost/ symlinks or
# GIL headers from installed Boost instead of this clone of boostog/gil).
#-----------------------------------------------------------------------------
add_library(gil_include_directories INTERFACE)
target_include_directories(gil_include_directories
  BEFORE
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test)

#-----------------------------------------------------------------------------
# Tests
#-----------------------------------------------------------------------------
enable_testing()

# On CI services, test the self-contained headers on-demand only to avoid build timeouts.
# CI environment is common for Travis CI, AppVeyor, CircleCI, etc.
# On Boost regression builds, CMake does not run, but Boost.Build,
# so the header tests are not enabled there either.
if(DEFINED ENV{CI})
  set(GIL_BUILD_HEADERS_TESTS OFF)
endif()
add_subdirectory(test)

if(GIL_ENABLE_EXT_IO)
  add_subdirectory(io/test)
endif()

if(GIL_ENABLE_EXT_NUMERIC)
  add_subdirectory(numeric/test)
endif()

if(GIL_ENABLE_EXT_TOOLBOX)
  add_subdirectory(toolbox/test)
endif()

#-----------------------------------------------------------------------------
# Examples
#-----------------------------------------------------------------------------
if(GIL_BUILD_EXAMPLES AND GIL_ENABLE_EXT_IO)
  add_subdirectory(example)
endif()
