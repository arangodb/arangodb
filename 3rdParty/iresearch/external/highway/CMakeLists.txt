# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.10)

# Set PIE flags for POSITION_INDEPENDENT_CODE targets, added in 3.14.
if(POLICY CMP0083)
  cmake_policy(SET CMP0083 NEW)
endif()

project(hwy VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Enabled PIE binaries by default if supported.
include(CheckPIESupported OPTIONAL RESULT_VARIABLE CHECK_PIE_SUPPORTED)
if(CHECK_PIE_SUPPORTED)
  check_pie_supported(LANGUAGES CXX)
  if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  endif()
endif()

include(GNUInstallDirs)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
   "int main() {
      #if !defined(__EMSCRIPTEN__)
      static_assert(false, \"__EMSCRIPTEN__ is not defined\");
      #endif
      return 0;
    }"
  HWY_EMSCRIPTEN
)

set(HWY_SOURCES
    contrib/image/image.cc
    contrib/image/image.h
    contrib/math/math-inl.h
    hwy/aligned_allocator.cc
    hwy/aligned_allocator.h
    hwy/base.h
    hwy/cache_control.h
    hwy/foreach_target.h
    hwy/highway.h
    hwy/nanobenchmark.cc
    hwy/nanobenchmark.h
    hwy/ops/arm_neon-inl.h
    hwy/ops/scalar-inl.h
    hwy/ops/set_macros-inl.h
    hwy/ops/shared-inl.h
    hwy/ops/wasm_128-inl.h
    hwy/ops/x86_128-inl.h
    hwy/ops/x86_256-inl.h
    hwy/ops/x86_512-inl.h
    hwy/targets.cc
    hwy/targets.h
    hwy/tests/test_util-inl.h
)

if (MSVC)
  # TODO(janwas): add flags
else()
  set(HWY_FLAGS
    # Avoid changing binaries based on the current time and date.
    -Wno-builtin-macro-redefined
    -D__DATE__="redacted"
    -D__TIMESTAMP__="redacted"
    -D__TIME__="redacted"

    # Optimizations
    -fmerge-all-constants

    # Warnings
    -Wall
    -Wextra
    -Wformat-security
    -Wno-unused-function
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wvla
  )

  if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND HWY_FLAGS
      -Wc++2a-extensions
      -Wfloat-overflow-conversion
      -Wfloat-zero-conversion
      -Wfor-loop-analysis
      -Wgnu-redeclared-enum
      -Winfinite-recursion
      -Wself-assign
      -Wstring-conversion
      -Wtautological-overlap-compare
      -Wthread-safety-analysis
      -Wundefined-func-template

      -fno-cxx-exceptions
      -fno-slp-vectorize
      -fno-vectorize

      # Use color in messages
      -fdiagnostics-show-option -fcolor-diagnostics
    )
  endif()

  if (WIN32)
    list(APPEND HWY_FLAGS
      -Wno-c++98-compat-pedantic
      -Wno-cast-align
      -Wno-double-promotion
      -Wno-float-equal
      -Wno-format-nonliteral
      -Wno-global-constructors
      -Wno-language-extension-token
      -Wno-missing-prototypes
      -Wno-shadow
      -Wno-shadow-field-in-constructor
      -Wno-sign-conversion
      -Wno-unused-member-function
      -Wno-unused-template
      -Wno-used-but-marked-unused
      -Wno-zero-as-null-pointer-constant
    )
  else()
    list(APPEND HWY_FLAGS
      -fmath-errno
      -fno-exceptions
    )
  endif()
endif()

add_library(hwy STATIC ${HWY_SOURCES})
target_compile_options(hwy PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(hwy PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# -------------------------------------------------------- install library
install(TARGETS hwy
  DESTINATION "${CMAKE_INSTALL_LIBDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

# Add a pkg-config file for libhwy and the test library.
set(HWY_LIBRARY_VERSION "${CMAKE_PROJECT_VERSION}")
foreach (pc libhwy.pc libhwy-test.pc)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${pc}.in" "${pc}" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${pc}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endforeach()

# -------------------------------------------------------- hwy_list_targets
# Generate a tool to print the compiled-in targets as defined by the current
# flags. This tool will print to stderr at build time, after building hwy.
add_executable(hwy_list_targets hwy/tests/list_targets.cc)
target_compile_options(hwy_list_targets PRIVATE ${HWY_FLAGS})
target_include_directories(hwy_list_targets PRIVATE
  $<TARGET_PROPERTY:hwy,INCLUDE_DIRECTORIES>)
# TARGET_FILE always returns the path to executable
# Naked target also not always could be run (due to the lack of '.\' prefix)
# Thus effective command to run should contain the full path
# and emulator prefix (if any).
add_custom_command(TARGET hwy POST_BUILD
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:hwy_list_targets> || (exit 0))

# -------------------------------------------------------- Examples

# Avoids mismatch between GTest's static CRT and our dynamic.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(skeleton hwy/examples/skeleton_main.cc)
target_sources(skeleton PRIVATE
    hwy/examples/skeleton-inl.h
    hwy/examples/skeleton.cc
    hwy/examples/skeleton.h
    hwy/examples/skeleton_shared.h)
# Try adding either -DHWY_COMPILE_ONLY_SCALAR or -DHWY_COMPILE_ONLY_STATIC to
# observe the difference in targets printed.
target_compile_options(skeleton PRIVATE ${HWY_FLAGS})
target_link_libraries(skeleton hwy)
set_target_properties(skeleton
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

# Similar: shared headers but without the runtime dispatch in skeleton.cc/h
add_executable(skeleton_static hwy/examples/skeleton_static_main.cc)
target_sources(skeleton_static PRIVATE
    hwy/examples/skeleton-inl.h
    hwy/examples/skeleton_shared.h)
target_compile_options(skeleton_static PRIVATE ${HWY_FLAGS})
target_link_libraries(skeleton_static hwy)
set_target_properties(skeleton_static
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

# Programming exercise with integrated benchmark
add_executable(hwy_benchmark hwy/examples/benchmark.cc)
target_sources(hwy_benchmark PRIVATE
    hwy/nanobenchmark.cc
    hwy/nanobenchmark.h)
target_compile_options(hwy_benchmark PRIVATE ${HWY_FLAGS})
target_link_libraries(hwy_benchmark hwy)
set_target_properties(hwy_benchmark
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

# -------------------------------------------------------- Tests

include(CTest)

if(BUILD_TESTING)
enable_testing()
include(GoogleTest)

set(HWY_SYSTEM_GTEST OFF CACHE BOOL "Use pre-installed googletest?")
if(HWY_SYSTEM_GTEST)
find_package(GTest REQUIRED)
else()
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()
endif() # HWY_SYSTEM_GTEST

set(HWY_TEST_FILES
  contrib/image/image_test.cc
  # contrib/math/math_test.cc
  hwy/examples/skeleton_test.cc
  hwy/tests/arithmetic_test.cc
  hwy/tests/combine_test.cc
  hwy/tests/compare_test.cc
  hwy/tests/convert_test.cc
  hwy/tests/hwy_test.cc
  hwy/tests/logical_test.cc
  hwy/tests/memory_test.cc
  hwy/tests/swizzle_test.cc
  hwy/aligned_allocator_test.cc
  hwy/targets_test.cc
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS HWY_TEST_FILES)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE})
  target_compile_options(${TESTNAME} PRIVATE ${HWY_FLAGS})

  if(HWY_SYSTEM_GTEST)
    target_link_libraries(${TESTNAME} hwy GTest::GTest GTest::Main)
  else()
    target_link_libraries(${TESTNAME} hwy gtest gtest_main)
  endif()
  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES PREFIX "tests/")

  if (HWY_EMSCRIPTEN)
    set_target_properties(${TESTNAME} PROPERTIES LINK_FLAGS "-s SINGLE_FILE=1")
  endif()

  if(${CMAKE_VERSION} VERSION_LESS "3.10.3")
    gtest_discover_tests(${TESTNAME} TIMEOUT 60)
  else ()
    gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 60)
  endif ()
endforeach ()

# The skeleton test uses the skeleton library code.
target_sources(skeleton_test PRIVATE hwy/examples/skeleton.cc)

endif() # BUILD_TESTING
