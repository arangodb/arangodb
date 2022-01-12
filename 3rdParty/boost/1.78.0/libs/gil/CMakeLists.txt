#
# Copyright (c) 2017-2019 Mateusz Loskot <mateusz at loskot dot net>
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
option(BOOST_GIL_BUILD_EXAMPLES "Build examples" ON)
option(BOOST_GIL_BUILD_HEADER_TESTS "Enable self-contained header tests" ON)
option(BOOST_GIL_ENABLE_EXT_DYNAMIC_IMAGE "Enable Dynamic Image extension, tests and examples" ON)
option(BOOST_GIL_ENABLE_EXT_IO "Enable IO extension, tests and examples (require libjpeg, libpng, libtiff)" ON)
option(BOOST_GIL_ENABLE_EXT_NUMERIC "Enable Numeric extension, tests and examples" ON)
option(BOOST_GIL_ENABLE_EXT_TOOLBOX "Enable Toolbox extension, tests and examples" ON)
option(BOOST_GIL_USE_CONAN "Use Conan to install dependencies" OFF)
option(BOOST_GIL_USE_CLANG_TIDY "Set CMAKE_CXX_CLANG_TIDY property on targets to enable clang-tidy linting" OFF)
set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard version to use (default is 11)")

#-----------------------------------------------------------------------------
# Project
#-----------------------------------------------------------------------------
project(Boost.GIL
  LANGUAGES CXX
  DESCRIPTION "Boost.GIL - Generic Image Library | Requires C++11 since Boost 1.68")

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_BINARY_DIR}/cmake)

#-----------------------------------------------------------------------------
# C++ language version and compilation flags
#-----------------------------------------------------------------------------
message(STATUS "Boost.GIL: Require C++${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Avoid warnings flood from CI builds
if(DEFINED ENV{CI} OR DEFINED ENV{AGENT_JOBSTATUS} OR DEFINED ENV{GITHUB_ACTIONS})
  set(BOOST_GIL_BUILD_CI ON)
  message(STATUS "Boost.GIL: Turning off detailed compiler warnings for CI build short log")
else()
  set(BOOST_GIL_BUILD_CI OFF)
  message(STATUS "Boost.GIL: Turning on detailed compiler warnings")
endif()

add_library(gil_compile_options INTERFACE)

# See https://cmake.org/pipermail/cmake/2018-December/068716.html
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REGEX REPLACE "-W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif()

# See https://svn.boost.org/trac10/wiki/Guidelines/WarningsGuidelines
target_compile_options(gil_compile_options
  INTERFACE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${BOOST_GIL_BUILD_CI}>>:-W1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<BOOL:${BOOST_GIL_BUILD_CI}>>>:-W4>
    $<$<CXX_COMPILER_ID:MSVC>:-bigobj>
    $<$<CXX_COMPILER_ID:MSVC>:-FC> # Need absolute path for __FILE__ used in tests
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-fstrict-aliasing>
    $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<NOT:$<BOOL:${BOOST_GIL_BUILD_CI}>>>:-Wall -Wconversion -Wextra -Wfloat-equal -Wshadow -Wsign-promo -Wstrict-aliasing -Wunused-parameter -pedantic>)

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

find_package(Boost 1.72.0 REQUIRED COMPONENTS filesystem)
message(STATUS "Boost.GIL: Using Boost_INCLUDE_DIRS=${Boost_INCLUDE_DIRS}")
message(STATUS "Boost.GIL: Using Boost_LIBRARY_DIRS=${Boost_LIBRARY_DIRS}")

target_link_libraries(gil_dependencies INTERFACE Boost::filesystem)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  target_link_libraries(gil_dependencies INTERFACE Boost::disable_autolinking)
endif()

target_compile_definitions(gil_dependencies
  INTERFACE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:BOOST_TEST_DYN_LINK>)

#-----------------------------------------------------------------------------
# Dependency: libpng, libjpeg, libtiff, libraw via Vcpkg or Conan
#-----------------------------------------------------------------------------
if(BOOST_GIL_USE_CONAN)
  # Download automatically, you can also just copy the conan.cmake file
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Boost.GIL: Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.15/conan.cmake"
      "${CMAKE_BINARY_DIR}/conan.cmake")
  endif()

  # NOTE: See RelWithDebInfo for Release builds, http://docs.conan.io/en/latest/howtos/vs2017_cmake.html
  set(_build_type_saved ${CMAKE_BUILD_TYPE})
  if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "Release")
  endif()

  include(${CMAKE_BINARY_DIR}/conan.cmake)
  conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP CMAKE_TARGETS BUILD missing)

  set(CMAKE_BUILD_TYPE ${_build_type_saved})
  unset(_build_type_saved)
endif()

if(BOOST_GIL_ENABLE_EXT_IO)
  if (BOOST_GIL_USE_CONAN)
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

    if (TIFF_LIBRARY)
      set(TIFFXX_NAMES tiffxx)
      foreach(name ${TIFFXX_NAMES})
        list(APPEND TIFFXX_NAMES_DEBUG "${name}d")
      endforeach()
      find_library(TIFFXX_LIBRARY_RELEASE NAMES ${TIFFXX_NAMES})
      find_library(TIFFXX_LIBRARY_DEBUG NAMES ${TIFFXX_NAMES_DEBUG})
      find_path(TIFFXX_INCLUDE_DIR NAMES tiffio.hxx)
      include(SelectLibraryConfigurations)
      select_library_configurations(TIFFXX)
      mark_as_advanced(TIFFXX_LIBRARY_RELEASE TIFFXX_LIBRARY_DEBUG)
      include(FindPackageHandleStandardArgs)
      find_package_handle_standard_args(TIFFXX REQUIRED_VARS TIFFXX_LIBRARY TIFFXX_INCLUDE_DIR)
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
    set(BOOST_GIL_ENABLE_EXT_IO_RAW ${LibRaw_FOUND} CACHE BOOL "Enable IO RAW extension (requires libraw)" FORCE)
    if(BOOST_GIL_ENABLE_EXT_IO_RAW)
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
if(BOOST_GIL_USE_CLANG_TIDY AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.6)
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
include(CTest)

if(BUILD_TESTING)
  # On CI services, test the self-contained headers on-demand only to avoid build timeouts.
  # CI environment is common for Travis CI, AppVeyor, CircleCI, etc.
  # On Boost regression builds, CMake does not run, but Boost.Build,
  # so the header tests are not enabled there either.
  if(DEFINED ENV{CI})
    set(BOOST_GIL_BUILD_HEADER_TESTS OFF)
  endif()

  add_subdirectory(test)
endif()

#-----------------------------------------------------------------------------
# Examples
#-----------------------------------------------------------------------------
if(BOOST_GIL_BUILD_EXAMPLES AND BOOST_GIL_ENABLE_EXT_IO)
  add_subdirectory(example)
endif()
