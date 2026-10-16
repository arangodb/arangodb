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

# Workaround for 3.19 raising error 'IMPORTED_LOCATION not set for imported
# target "GTest::gtest_main"'.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 OLD)
endif()

project(hwy VERSION 1.0.4)  # Keep in sync with highway.h version
# `hwy` is lowercase to handle find_package() in Config mode:
set(namespace "${PROJECT_NAME}::")

# Directly define the ABI version from the cmake project() version values:
set(LIBRARY_VERSION "${hwy_VERSION}")
set(LIBRARY_SOVERSION ${hwy_VERSION_MAJOR})

set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Search for Atomics implementation:
find_package(Atomics REQUIRED)

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

# The following is only required with GCC < 6.1.0 or CLANG < 16.0
set(HWY_CMAKE_ARM7 OFF CACHE BOOL "Set copts for Armv7 with NEON (requires vfpv4)?")

# Unconditionally adding -Werror risks breaking the build when new warnings
# arise due to compiler/platform changes. Enable this in CI/tests.
set(HWY_WARNINGS_ARE_ERRORS OFF CACHE BOOL "Add -Werror flag?")

set(HWY_ENABLE_CONTRIB ON CACHE BOOL "Include contrib/")
set(HWY_ENABLE_EXAMPLES ON CACHE BOOL "Build examples")
set(HWY_ENABLE_INSTALL ON CACHE BOOL "Install library")
set(HWY_ENABLE_TESTS ON CACHE BOOL "Enable HWY tests")

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

check_cxx_source_compiles(
   "int main() {
      #if !defined(__riscv)
      static_assert(false, \"__riscv is not defined\");
      #endif
      return 0;
    }"
  HWY_RISCV
)

if (HWY_ENABLE_CONTRIB)
# Glob all the traits so we don't need to modify this file when adding
# additional special cases.
file(GLOB HWY_CONTRIB_SOURCES "hwy/contrib/sort/vqsort_*.cc")
list(APPEND HWY_CONTRIB_SOURCES
    hwy/contrib/dot/dot-inl.h
    hwy/contrib/image/image.cc
    hwy/contrib/image/image.h
    hwy/contrib/math/math-inl.h
    hwy/contrib/sort/shared-inl.h
    hwy/contrib/sort/sorting_networks-inl.h
    hwy/contrib/sort/traits-inl.h
    hwy/contrib/sort/traits128-inl.h
    hwy/contrib/sort/vqsort-inl.h
    hwy/contrib/sort/vqsort.cc
    hwy/contrib/sort/vqsort.h
    hwy/contrib/algo/copy-inl.h
    hwy/contrib/algo/find-inl.h
    hwy/contrib/algo/transform-inl.h
)
endif()  # HWY_ENABLE_CONTRIB

set(HWY_SOURCES
    hwy/aligned_allocator.cc
    hwy/aligned_allocator.h
    hwy/base.h
    hwy/cache_control.h
    hwy/detect_compiler_arch.h  # private
    hwy/detect_targets.h  # private
    hwy/foreach_target.h
    hwy/highway.h
    hwy/highway_export.h
    hwy/nanobenchmark.cc
    hwy/nanobenchmark.h
    hwy/ops/arm_neon-inl.h
    hwy/ops/arm_sve-inl.h
    hwy/ops/emu128-inl.h
    hwy/ops/generic_ops-inl.h
    hwy/ops/ppc_vsx-inl.h
    hwy/ops/rvv-inl.h
    hwy/ops/scalar-inl.h
    hwy/ops/set_macros-inl.h
    hwy/ops/shared-inl.h
    hwy/ops/wasm_128-inl.h
    hwy/ops/tuple-inl.h
    hwy/ops/x86_128-inl.h
    hwy/ops/x86_256-inl.h
    hwy/ops/x86_512-inl.h
    hwy/per_target.cc
    hwy/per_target.h
    hwy/print-inl.h
    hwy/print.cc
    hwy/print.h
    hwy/targets.cc
    hwy/targets.h
)

set(HWY_TEST_SOURCES
    hwy/tests/hwy_gtest.h
    hwy/tests/test_util-inl.h
    hwy/tests/test_util.cc
    hwy/tests/test_util.h
)

if (MSVC)
  set(HWY_FLAGS
    # fix build error C1128 in blockwise*_test & arithmetic_test
    /bigobj
  )
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
    # These are not included in Wall nor Wextra:
    -Wconversion
    -Wsign-conversion
    -Wvla
    -Wnon-virtual-dtor
  )

  if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND HWY_FLAGS
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
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
      list(APPEND HWY_FLAGS -Wc++2a-extensions)
    endif()
  endif()

  if (WIN32)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
      list(APPEND HWY_FLAGS
        -Wno-global-constructors
        -Wno-language-extension-token
        -Wno-used-but-marked-unused
        -Wno-shadow-field-in-constructor
        -Wno-unused-member-function
        -Wno-unused-template
        -Wno-c++98-compat-pedantic
        -Wno-used-but-marked-unused
        -Wno-zero-as-null-pointer-constant
      )
    endif()

    list(APPEND HWY_FLAGS
      -Wno-cast-align
      -Wno-double-promotion
      -Wno-float-equal
      -Wno-format-nonliteral
      -Wno-shadow
      -Wno-sign-conversion
    )
  else()
    list(APPEND HWY_FLAGS
      -fmath-errno
      -fno-exceptions
    )
  endif()  # WIN32

  if (HWY_CMAKE_ARM7)
    list(APPEND HWY_FLAGS
      -march=armv7-a
      -mfpu=neon-vfpv4
      -mfloat-abi=hard  # must match the toolchain specified as CXX=
      -mfp16-format=ieee  # required for vcvt_f32_f16
    )
  endif()  # HWY_CMAKE_ARM7

  if(HWY_RISCV)
    # gcc(13) and recent clang both support V, but not yet runtime dispatch, so
    # we add the gcv compiler flag, which then requires the CPU (now when using
    # either compiler) to support V.
    list(APPEND HWY_FLAGS -march=rv64gcv1p0)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
      list(APPEND HWY_FLAGS -menable-experimental-extensions)
    endif()
  endif()

  if (HWY_WARNINGS_ARE_ERRORS)
    list(APPEND HWY_FLAGS -Werror)
  endif()

  # Prevent "wasm-ld: error: --shared-memory is disallowed by targets.cc.o
  # because it was not compiled with 'atomics' or 'bulk-memory' features."
  if (HWY_EMSCRIPTEN)
    list(APPEND HWY_FLAGS -matomics)
  endif()

endif()  # !MSVC

include(CheckIncludeFile)
check_include_file(sys/auxv.h  HAVE_SYS_AUXV_H)
check_include_file(asm/hwcap.h HAVE_ASM_HWCAP_H)
if (NOT HAVE_ASM_HWCAP_H)
  list(APPEND HWY_FLAGS -DTOOLCHAIN_MISS_ASM_HWCAP_H)
endif()

# By default prefer STATIC build (legacy behavior)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(HWY_FORCE_STATIC_LIBS "Ignore BUILD_SHARED_LIBS" OFF)
# only expose shared/static options to advanced users:
mark_as_advanced(BUILD_SHARED_LIBS)
mark_as_advanced(HWY_FORCE_STATIC_LIBS)
# Define visibility settings globally:
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Copy-cat "add_library" logic + add override.
set(HWY_LIBRARY_TYPE "SHARED")
if (NOT BUILD_SHARED_LIBS OR HWY_FORCE_STATIC_LIBS)
  set(HWY_LIBRARY_TYPE "STATIC")
endif()

# This preprocessor define will drive the build, also used in the *.pc files:
if("${HWY_LIBRARY_TYPE}" STREQUAL "SHARED")
  set(DLLEXPORT_TO_DEFINE "HWY_SHARED_DEFINE")
else()
  set(DLLEXPORT_TO_DEFINE "HWY_STATIC_DEFINE")
endif()

add_library(hwy ${HWY_LIBRARY_TYPE} ${HWY_SOURCES})
if(NOT HAVE_SYS_AUXV_H)
  target_compile_definitions(hwy PUBLIC TOOLCHAIN_MISS_SYS_AUXV_H)
endif()
target_compile_definitions(hwy PUBLIC "${DLLEXPORT_TO_DEFINE}")
target_compile_options(hwy PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy PUBLIC cxx_std_11)
set_target_properties(hwy PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# For GCC __atomic_store_8, see #887
target_link_libraries(hwy PRIVATE ${ATOMICS_LIBRARIES})
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()

if (HWY_ENABLE_CONTRIB)
add_library(hwy_contrib ${HWY_LIBRARY_TYPE} ${HWY_CONTRIB_SOURCES})
target_link_libraries(hwy_contrib hwy)
target_compile_options(hwy_contrib PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy_contrib PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_contrib PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_contrib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy_contrib PUBLIC cxx_std_11)
set_target_properties(hwy_contrib PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy_contrib APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()
endif()  # HWY_ENABLE_CONTRIB

add_library(hwy_test ${HWY_LIBRARY_TYPE} ${HWY_TEST_SOURCES})
target_link_libraries(hwy_test hwy)
target_compile_options(hwy_test PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy_test PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_test PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_test PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy_test PUBLIC cxx_std_11)
set_target_properties(hwy_test PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy_test APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()

# -------------------------------------------------------- hwy_list_targets
# Generate a tool to print the compiled-in targets as defined by the current
# flags. This tool will print to stderr at build time, after building hwy.
add_executable(hwy_list_targets hwy/tests/list_targets.cc)
target_compile_options(hwy_list_targets PRIVATE ${HWY_FLAGS})
target_link_libraries(hwy_list_targets hwy)
target_include_directories(hwy_list_targets PRIVATE
  $<TARGET_PROPERTY:hwy,INCLUDE_DIRECTORIES>)
# TARGET_FILE always returns the path to executable
# Naked target also not always could be run (due to the lack of '.\' prefix)
# Thus effective command to run should contain the full path
# and emulator prefix (if any).
if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
add_custom_command(TARGET hwy_list_targets POST_BUILD
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:hwy_list_targets> || (exit 0))
endif()

# --------------------------------------------------------
# Allow skipping the following sections for projects that do not need them:
# tests, examples, benchmarks and installation.

# -------------------------------------------------------- install library
if (HWY_ENABLE_INSTALL)

install(TARGETS hwy EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

if (HWY_ENABLE_CONTRIB)
install(TARGETS hwy_contrib EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_CONTRIB_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()
endif()  # HWY_ENABLE_CONTRIB

install(TARGETS hwy_test EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_TEST_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

# Add a pkg-config file for libhwy and the contrib/test libraries.
set(HWY_LIBRARY_VERSION "${CMAKE_PROJECT_VERSION}")
set(HWY_PC_FILES libhwy.pc libhwy-test.pc)
if (HWY_ENABLE_CONTRIB)
list(APPEND HWY_PC_FILES libhwy-contrib.pc)
endif()  # HWY_ENABLE_CONTRIB
foreach (pc ${HWY_PC_FILES})
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${pc}.in" "${pc}" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${pc}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endforeach()

endif()  # HWY_ENABLE_INSTALL
# -------------------------------------------------------- Examples
if (HWY_ENABLE_EXAMPLES)

# Avoids mismatch between GTest's static CRT and our dynamic.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Programming exercise with integrated benchmark
add_executable(hwy_benchmark hwy/examples/benchmark.cc)
target_sources(hwy_benchmark PRIVATE
    hwy/nanobenchmark.h)
# Try adding one of -DHWY_COMPILE_ONLY_SCALAR, -DHWY_COMPILE_ONLY_EMU128 or
# -DHWY_COMPILE_ONLY_STATIC to observe the difference in targets printed.
target_compile_options(hwy_benchmark PRIVATE ${HWY_FLAGS})
target_link_libraries(hwy_benchmark PRIVATE hwy)
target_link_libraries(hwy_benchmark PRIVATE ${ATOMICS_LIBRARIES})
set_target_properties(hwy_benchmark
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

endif()  # HWY_ENABLE_EXAMPLES
# -------------------------------------------------------- Tests

include(CTest)

if(BUILD_TESTING AND HWY_ENABLE_TESTS)
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
endif()  # HWY_SYSTEM_GTEST

set(HWY_TEST_FILES
  hwy/contrib/algo/copy_test.cc
  hwy/contrib/algo/find_test.cc
  hwy/contrib/algo/transform_test.cc
  hwy/aligned_allocator_test.cc
  hwy/base_test.cc
  hwy/highway_test.cc
  hwy/nanobenchmark_test.cc
  hwy/targets_test.cc
  hwy/examples/skeleton_test.cc
  hwy/tests/arithmetic_test.cc
  hwy/tests/blockwise_shift_test.cc
  hwy/tests/blockwise_test.cc
  hwy/tests/combine_test.cc
  hwy/tests/compare_test.cc
  hwy/tests/compress_test.cc
  hwy/tests/convert_test.cc
  hwy/tests/count_test.cc
  hwy/tests/crypto_test.cc
  hwy/tests/demote_test.cc
  hwy/tests/expand_test.cc
  hwy/tests/float_test.cc
  hwy/tests/if_test.cc
  hwy/tests/interleaved_test.cc
  hwy/tests/logical_test.cc
  hwy/tests/mask_mem_test.cc
  hwy/tests/mask_test.cc
  hwy/tests/memory_test.cc
  hwy/tests/mul_test.cc
  hwy/tests/reduction_test.cc
  hwy/tests/reverse_test.cc
  hwy/tests/shift_test.cc
  hwy/tests/swizzle_test.cc
  hwy/tests/test_util_test.cc
  hwy/tests/tuple_test.cc
)

set(HWY_TEST_LIBS hwy hwy_test)

if (HWY_ENABLE_CONTRIB)
list(APPEND HWY_TEST_LIBS hwy_contrib)

list(APPEND HWY_TEST_FILES
  hwy/contrib/dot/dot_test.cc
  hwy/contrib/image/image_test.cc
  # Disabled due to SIGILL in clang7 debug build during gtest discovery phase,
  # not reproducible locally. Still tested via bazel build.
  # hwy/contrib/math/math_test.cc
  hwy/contrib/sort/sort_test.cc
  hwy/contrib/sort/bench_sort.cc
)
endif()  # HWY_ENABLE_CONTRIB

if(HWY_SYSTEM_GTEST)
  if (CMAKE_VERSION VERSION_LESS 3.20)
    set(HWY_GTEST_LIBS GTest::GTest GTest::Main)
  else()
    set(HWY_GTEST_LIBS GTest::gtest GTest::gtest_main)
  endif()
else()
  set(HWY_GTEST_LIBS gtest gtest_main)
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS HWY_TEST_FILES)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE})
  target_compile_options(${TESTNAME} PRIVATE ${HWY_FLAGS})
  # Test all targets, not just the best/baseline. This changes the default
  # policy to all-attainable; note that setting -DHWY_COMPILE_* directly can
  # cause compile errors because only one may be set, and other CMakeLists.txt
  # that include us may set them.
  target_compile_options(${TESTNAME} PRIVATE -DHWY_IS_TEST=1)

  target_link_libraries(${TESTNAME} PRIVATE ${HWY_TEST_LIBS} ${HWY_GTEST_LIBS})
  # For GCC __atomic_store_8, see #887
  target_link_libraries(${TESTNAME} PRIVATE ${ATOMICS_LIBRARIES})
  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")

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

endif()  # BUILD_TESTING

if (HWY_ENABLE_INSTALL)
  # write hwy-config file to handle `Config` mode
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/hwy-config-version.cmake" COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hwy-config-version.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hwy")
  install(EXPORT hwy_targets NAMESPACE "${namespace}" FILE hwy-config.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hwy")
endif()
