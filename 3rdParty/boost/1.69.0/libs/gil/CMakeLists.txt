#
# Copyright (c) 2017-2018 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
cmake_minimum_required(VERSION 3.10)
project(Boost.GIL CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CMakeDependentOption)
option(GIL_BUILD_TESTS "Build GIL tests" ON)
option(GIL_BUILD_EXAMPLES "Build GIL examples" OFF) # FIXME: Switch to ON after https://github.com/boostorg/gil/issues/40
option(GIL_ENABLE_IO "Enable GIL IO in tests and examples (require libjpeg, libpng, libtiff)" ON)
option(GIL_USE_CONAN "Use Conan to install dependencies" OFF)
option(GIL_ENABLE_FINDBOOST_DOWNLOAD "Enable auto-download FindBoost.cmake for CMake" OFF)

#-----------------------------------------------------------------------------
# Dependency: Boost
# - look for stage Build
# - look for default installation location
# - look for location specified with BOOST_ROOT
#-----------------------------------------------------------------------------
if (CMAKE_VERSION VERSION_LESS 3.13 AND NOT GIL_ENABLE_FINDBOOST_DOWNLOAD)
  message(STATUS "Boost.GIL: You are using CMake older than 3.13")
  message(STATUS "Boost.GIL: FindBoost.cmake has likely been updated to detect newer or even not yet released Boost")
  message(STATUS "Boost.GIL: Run CMake with -DGIL_ENABLE_FINDBOOST_DOWNLOAD=ON to get latest version of FindBoost.cmake")
  message(STATUS "Boost.GIL: WARNING:")
  message(STATUS "Boost.GIL:    Newer FindBoost.cmake may fail to find your Boost for many reasons.")
  message(STATUS "Boost.GIL:    For example, this may be due to unrecognised toolset eg. latest Visual Studio 2017.")
  message(STATUS "Boost.GIL:    Try run CMake with -DBoost_COMPILER=\"-vc141\" or value for your toolset.")
endif()

if (GIL_ENABLE_FINDBOOST_DOWNLOAD)
  if (NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/FindBoost.cmake")
    message(STATUS "Boost.GIL: Downloading FindBoost.cmake from https://gitlab.kitware.com/cmake/ release branch")
    file(DOWNLOAD
      "https://gitlab.kitware.com/cmake/cmake/raw/release/Modules/FindBoost.cmake"
      "${CMAKE_BINARY_DIR}/cmake/FindBoost.cmake")
  endif()
  list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_BINARY_DIR}/cmake)
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

set(BOOST_COMPONENTS)
if(GIL_BUILD_TESTS)
  list(APPEND BOOST_COMPONENTS unit_test_framework filesystem)
endif()

#set(Boost_DEBUG ON)
set(Boost_DETAILED_FAILURE_MSG ON)
if(MSVC)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_STATIC_RUNTIME OFF)
endif()

find_package(Boost 1.65.0 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
message(STATUS "Boost.GIL: using Boost_INCLUDE_DIRS=${Boost_INCLUDE_DIRS}")
message(STATUS "Boost.GIL: using Boost_LIBRARY_DIRS=${Boost_LIBRARY_DIRS}")

# The boostorg/gil repository includes must come first,
# before Boost includes from cloned Boost superproject or installed distribution.
# Otherwise the IDE sees the wrong file (ie. due to boost/ symlinks or
# GIL headers from installed Boost instead of this clone of boostog/gil).
include_directories(include)

# GIL header gil_test_common.hpp shared between tests
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test)

#-----------------------------------------------------------------------------
# Dependency: libpng, libjpeg, libtiff via Vcpkg or Conan
#-----------------------------------------------------------------------------
if (GIL_ENABLE_IO)
  if(GIL_USE_CONAN)
    # Download automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
      message(STATUS "Boost.GIL: Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
      file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.9/conan.cmake"
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
  else()
    find_package(JPEG)
    find_package(PNG)
    find_package(TIFF)

    if (NOT MSVC)
      # Typically, Linux packages provide C++ stream interface for TIFF
      find_path(_tiffxx_include_dir NAMES tiffio.hxx)
      find_library(_tiffxx_library NAMES tiffxx)
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
# Compiler
#
# Follows https://svn.boost.org/trac10/wiki/Guidelines/WarningsGuidelines
#-----------------------------------------------------------------------------
if(MSVC)
  string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  string(REGEX REPLACE "-W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  add_compile_options(-W4)
  add_compile_options(-bigobj)
  add_compile_options(-FC) # Need absolute path for __FILE__ used in tests

  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE=1)
  add_definitions(-D_SCL_SECURE_NO_DEPRECATE=1)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS=1)
  add_definitions(-DNOMINMAX=1)
  add_definitions(-DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE=1)
else()
  # Assumes compilers which recognize GCC and clang speak
  add_compile_options(-pedantic)
  add_compile_options(-fstrict-aliasing)
  add_compile_options(-Wall)
  add_compile_options(-Wextra)
  add_compile_options(-Wstrict-aliasing)
  add_compile_options(-Wconversion )
  add_compile_options(-Wsign-promo)
  add_compile_options(-Wfloat-equal)
  add_compile_options(-Wunused-parameter)
  add_compile_options(-Wshadow)
endif()

#-----------------------------------------------------------------------------
# Headers
#-----------------------------------------------------------------------------
file(GLOB _boost_gil_headers
     ${PROJECT_SOURCE_DIR}/include/boost/gil/*.hpp
     ${PROJECT_SOURCE_DIR}/include/boost/gil.hpp)

#-----------------------------------------------------------------------------
# Tests
#-----------------------------------------------------------------------------
if(GIL_BUILD_TESTS)
  file(GLOB _boost_gil_test_headers
    ${PROJECT_SOURCE_DIR}/test/gil_test_common.hpp)

  enable_testing()
  add_subdirectory(test)
  if (GIL_ENABLE_IO)
    add_subdirectory(io/test)
  endif()
  add_subdirectory(numeric/test)
  add_subdirectory(toolbox/test)
endif()

#-----------------------------------------------------------------------------
# Examples
#-----------------------------------------------------------------------------
if(GIL_BUILD_EXAMPLES AND GIL_ENABLE_IO)
  add_subdirectory(example)
endif()
