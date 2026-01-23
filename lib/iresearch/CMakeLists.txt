cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

find_package(Git 1.5.3 REQUIRED)

# we use of xxx_ROOT variables for modules sources search
if (POLICY CMP0074)
  cmake_policy(SET CMP0074 OLD)
endif ()

################################################################################
### set required standard version
################################################################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

################################################################################
### interprocedural optimization (link time optimization)
################################################################################

set(USE_IPO AUTO CACHE STRING "Use interprocedural optimization: ON, OFF or AUTO")
set_property(CACHE USE_IPO PROPERTY STRINGS AUTO ON OFF)

# Determine value if IPO_ENABLED from USE_IPO and CMAKE_BUILD_TYPE
if (USE_IPO STREQUAL "AUTO")
  # When USE_IPO=AUTO, enable IPO for optimized / release builds.
  # But to work around a g++ segfault triggered by using both -flto and
  # -fno-devirtualize-functions, we disable IPO when using google tests, because
  # this will set no-devirtualize. See
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91387 and
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91375.
  # So this check may be removed later as soon as we use fixed gcc versions.
  # - Tobias, 2019-08-08
  if (CMAKE_BUILD_TYPE STREQUAL "Release"
    OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
    OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(IPO_ENABLED True)
  else ()
    set(IPO_ENABLED False)
  endif ()
elseif (USE_IPO)
  set(IPO_ENABLED True)
else ()
  set(IPO_ENABLED False)
endif ()

message(STATUS "IPO_ENABLED: ${IPO_ENABLED}")

project(iresearch)

# attach additional cmake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# load cmake utils
include(Utils)
include(PVS-Studio)

if (NOT MSVC)
  # put GCC version into GCC_VERSION variable
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
endif ()

################################################################################
### compile options
################################################################################

option(USE_PVS_STUDIO "Generate target for PVS-Studio check" OFF)
option(USE_CLANG_TIDY "Include clang-tidy checks" OFF)
option(USE_IWYU "Include IWYU checks" OFF)
option(USE_UTILS "Build utils" ON)
option(USE_TESTS "Build tests" OFF)
option(USE_MICROBENCH "Build micro-benchmark project" OFF)
option(USE_PYRESEARCH "Build iresearch python bridge" OFF)
option(USE_VALGRIND "Use workarounds to avoid false positives in valgrind" OFF)
option(USE_SIMDCOMP "Use architecture specific low-level optimizations" OFF)
option(USE_CCACHE "Use CCACHE if present" ON)
option(USE_URING "Build iresearch with uring support" OFF)
option(SUPPRESS_EXTERNAL_WARNINGS "Suppress warnings originating in 3rd party code" ON)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
  option(IRESEARCH_DEBUG "Enable IRS_ASSERT and other checks" ON)
else ()
  option(IRESEARCH_DEBUG "Enable IRS_ASSERT and other checks" OFF)
endif ()

if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  set(IRESEARCH_COVERAGE ON)
  set(CMAKE_BUILD_TYPE "Debug")
elseif (CMAKE_BUILD_TYPE MATCHES "Profile")
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(
    -g
    -fno-omit-frame-pointer
  # -fno-inline
  # -fno-optimize-sibling-calls
  )
endif ()

add_option_gprof(FALSE)

if (USE_VALGRIND)
  add_definitions(-DIRESEARCH_VALGRIND)
endif ()

if (MSVC)
  # FIXME TODO find a workaround or do not use alignas(...)
  # MSVC2017.1 - MSVC2018.7 does not correctly support alignas()
  # MSVC2017.8 requires the following define
  add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)

  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(/bigobj)

  # min/max macros interferes with our codebase
  add_definitions(-DNOMINMAX)

  add_definitions(-D_USE_MATH_DEFINES)

  # we don't want obvious warning
  add_compile_options(/wd4324)

  if (MSVC_BUILD_THREADS)
    set(CMAKE_C_FLAGS "/MP${MSVC_BUILD_THREADS} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "/MP${MSVC_BUILD_THREADS} ${CMAKE_CXX_FLAGS}")
  else ()
    set(CMAKE_C_FLAGS "/MP ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")
  endif ()

  if (${MSVC_VERSION} STRGREATER_EQUAL 1930)
    # see https://docs.microsoft.com/en-us/cpp/overview/cpp-conformance-improvements?view=msvc-170#error-on-a-non-dependent-static_assert
    set(CMAKE_C_FLAGS "/Zc:static_assert- ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "/Zc:static_assert- ${CMAKE_CXX_FLAGS}")
  endif ()
else ()
  # We want to force clang/gcc to use it, but only for C++ files
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fsized-deallocation>)
endif ()

# fix strange issue with utf_8_to_32_iterator failing
# to follow std::bidirectional_iterator concept
add_definitions(-DBOOST_TEXT_DISABLE_CONCEPTS)

################################################################################
### setup clang-tidy
################################################################################

unset(CLANG_TIDY_EXE CACHE)

if (USE_CLANG_TIDY)
  find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clang-tidy executable"
  )

  if (CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
  else ()
    message(STATUS "clang-tidy not found.")
  endif ()
endif ()

################################################################################
### setup IWYU
################################################################################

unset(IWYU_PATH)

if (USE_IWYU)
  find_program(IWYU_PATH NAMES include-what-you-use iwyu)

  if (NOT IWYU_PATH)
    message(FATAL_ERROR "Could not find the program include-what-you-use")
  endif ()
endif ()

################################################################################
### setup ccache
################################################################################

if (USE_CCACHE)
  find_program(CCACHE_FOUND ccache)

  if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  endif (CCACHE_FOUND)
endif ()

################################################################################
### setup platform dependent optimizations
################################################################################

include(OptimizeForArchitectureLocal)

################################################################################
### find 3rd party libraries
################################################################################

# find liburing
if (USE_URING AND NOT MSVC)
  find_package(Uring)

  if (uring_FOUND)
    add_definitions(-DIRESEARCH_URING)
    set(LIBURING uring::uring)
  endif ()
endif ()

# find Boost
set(IRESEARCH_EXTERNAL_INCLUDES
  ${EXTERNAL_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS} # ensure Boost paths take precedence over other system libraries as Boost may be defined elsewhere
  ${Lz4_INCLUDE_DIR}
  ${Fasttext_INCLUDE_DIR}
  ${FROZEN_INCLUDE_DIR})

include_directories(SYSTEM ${IRESEARCH_EXTERNAL_INCLUDES})

# set pthread library
if (NOT MSVC)
  set(PTHREAD_LIBRARY pthread)
endif ()

find_package(Lz4 REQUIRED)
find_package(ICU REQUIRED)
find_package(Snowball REQUIRED)

# Important note, now we include it before everything except C libraries
# TODO: I think we can try to include Sanitizers before C libraries, but now it may cause build issues
include(Sanitize)

# set external dirs
set(EXTERNAL_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/3rdParty
  CACHE INTERNAL
  ""
  )

set(IResearch_INCLUDE_DIR
  "${PROJECT_SOURCE_DIR}/core"
  CACHE INTERNAL
  ""
  )

# set output directories
set(EXECUTABLE_OUTPUT_PATH
  ${CMAKE_BINARY_DIR}/bin
  CACHE PATH
  "Executable output path"
  )

set(LIBRARY_OUTPUT_PATH
  ${CMAKE_BINARY_DIR}/bin
  CACHE PATH
  "Library output path"
  )

mark_as_advanced(
  EXECUTABLE_OUTPUT_PATH
  LIBRARY_OUTPUT_PATH
)

add_definitions(-DUNICODE -D_UNICODE)

# set test resource directory
set(IResearch_test_resource_dir
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/resources
  )

# generate tests_config.hpp here instead of inside 'tests' to make available externally
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/tests_config.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/tests/tests_config.hpp"
)

# copy pre-built files over to the build directory
set(IRESEARCH_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt)
file(GLOB_RECURSE IRESEARCH_PREBUILT_LIST RELATIVE "${IRESEARCH_PREBUILT_DIR}" FOLLOW_SYMLINKS "${IRESEARCH_PREBUILT_DIR}/*")
foreach (ELEMENT ${IRESEARCH_PREBUILT_LIST})
  configure_file("${IRESEARCH_PREBUILT_DIR}/${ELEMENT}" "${CMAKE_CURRENT_BINARY_DIR}/${ELEMENT}" COPYONLY)
endforeach ()

if (IRESEARCH_COVERAGE)
  include(Coverage)
elseif (IRESEARCH_DEBUG)
  add_definitions(-DIRESEARCH_DEBUG)
endif ()
if (USE_TESTS)
  add_definitions(-DIRESEARCH_TEST)
endif ()

add_subdirectory(core)

if (USE_UTILS)
  add_subdirectory(utils)
endif ()

if (USE_PYRESEARCH)
  add_subdirectory(python)
endif ()

if (USE_PVS_STUDIO)
  set(PVS_STUDIO_ANALYZE iresearch-static
    ${IResearchUtil_TARGET_NAME}
    ${IResearchBenchmarks_TARGET_NAME}
    ${IResearchPython_TARGET_NAME})

  pvs_studio_add_target(
    TARGET iresearch-pvs-check ALL
    OUTPUT FORMAT errorfile
    ANALYZE ${PVS_STUDIO_ANALYZE}
    MODE GA:1,2 OP
    LOG iresearch.err
    CONFIG "${CMAKE_SOURCE_DIR}/PVSIResearch.cfg"
  )

  pvs_studio_add_target(
    TARGET iresearch-pvs-check-html ALL
    FORMAT fullhtml
    ANALYZE ${PVS_STUDIO_ANALYZE}
    MODE GA:1,2 OP
    LOG iresearch-html.err
    CONFIG "${CMAKE_SOURCE_DIR}/PVSIResearch.cfg"
  )

  pvs_studio_add_target(
    TARGET iresearch-pvs-check-xml ALL
    FORMAT xml
    ANALYZE ${PVS_STUDIO_ANALYZE}
    MODE GA:1,2 OP
    LOG iresearch-xml.err
    CONFIG "${CMAKE_SOURCE_DIR}/PVSIResearch.cfg"
  )
endif ()

if (USE_MICROBENCH)
  add_subdirectory(microbench)
endif ()

if (USE_TESTS)
  add_subdirectory(tests)

  # testing
  enable_testing()
  add_test(
    iresearch-tests
    ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX}
  )

  # testing auto build not working
  # due to the following bug in cmake
  # http://public.kitware.com/Bug/view.php?id=8774
  # here is the workaround:
  add_custom_target(iresearch-check
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS iresearch-tests
    )

  # setup target for memory allocation profiling
  add_custom_target(iresearch-tests-malloc
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    COMMAND ms_print --x=500 --y=100 massif.out > massif.log
    DEPENDS iresearch-tests
    )

  # setup target for memory allocation profiling
  add_custom_target(iresearch-tests-malloc-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    COMMAND ms_print --x=500 --y=100 massif.out > massif.log
    DEPENDS iresearch-tests
    )

  # setup target for memory leak detection
  add_custom_target(iresearch-tests-memleak
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests
    )

  # setup target for memory leak detection
  add_custom_target(iresearch-tests-memleak-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests-static
    )

  # setup target for thread race detection
  add_custom_target(iresearch-tests-threadrace
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests
    )

  # setup target for thread race detection
  add_custom_target(iresearch-tests-threadrace-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests-static
    )

endif ()
