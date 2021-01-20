
cmake_minimum_required(VERSION 3.5.1)
cmake_policy(SET CMP0048 NEW) # enable project VERSION
cmake_policy(SET CMP0056 NEW) # honor link flags in try_compile()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(immer VERSION 0.6.2)

if (NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-extended-offsetof -Wno-c++17-extensions -Wno-c++1z-extensions -Wno-unknown-warning-option")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_EXTENSIONS off)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments")
endif()

include(ImmerUtils)

#  Options
#  =======

option(ENABLE_SANITIZE "compile with sanitizers enabled")
option(ENABLE_COVERAGE "compile with test coverage support")
option(DISABLE_WERROR "enable --werror")
option(DISABLE_FREE_LIST "disables the free list heap")
option(DISABLE_THREAD_SAFETY "disables thread safety by default")

option(ENABLE_PYTHON "enable building python module" off)
option(ENABLE_GUILE "enable building guile module" off)
option(ENABLE_BOOST_COROUTINE "run benchmarks with boost coroutine" off)

set(CXX_STANDARD 14 CACHE STRING "c++ standard number")

set(CMAKE_CXX_STANDARD ${CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED on)

if (ENABLE_SANITIZE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak")
endif()
if (NOT MSVC AND NOT DISABLE_WERROR)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

#  Dependencies
#  ============

if (ENABLE_BOOST_COROUTINE)
  set(immer_boost_components coroutine)
endif()

find_package(Threads)
find_package(BoehmGC)
find_package(Boost 1.56 COMPONENTS ${immer_boost_components})

find_program(CCACHE ccache)
if (CCACHE)
  message(STATUS "Using ccache: ${CCACHE}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
else()
  message(STATUS "Could not find ccache")
endif()

if (NOT BOEHM_GC_FOUND)
  set(BOEHM_GC_LIBRARIES "")
endif()

#  Targets
#  =======

# the library
add_library(immer INTERFACE)
target_include_directories(immer INTERFACE
  $<BUILD_INTERFACE:${immer_BINARY_DIR}/>
  $<BUILD_INTERFACE:${immer_SOURCE_DIR}/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS immer EXPORT ImmerConfig)
install(EXPORT ImmerConfig DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Immer")
install(DIRECTORY immer DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# development target to be used in tests, examples, benchmarks...
immer_canonicalize_cmake_booleans(
  DISABLE_FREE_LIST
  DISABLE_THREAD_SAFETY
  CHECK_SLOW_TESTS)
add_library(immer-dev INTERFACE)
target_include_directories(immer-dev SYSTEM INTERFACE
  ${Boost_INCLUDE_DIR}
  ${BOEHM_GC_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/tools/include)
target_link_libraries(immer-dev INTERFACE
  immer
  ${BOEHM_GC_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})
target_compile_definitions(immer-dev INTERFACE
  -DIMMER_CXX_STANDARD=${CXX_STANDARD}
  -DIMMER_HAS_LIBGC=1
  -DIMMER_NO_FREE_LIST=${DISABLE_FREE_LIST}
  -DIMMER_NO_THREAD_SAFETY=${DISABLE_THREAD_SAFETY}
  -DIMMER_SLOW_TESTS=${CHECK_SLOW_TESTS})
if (ENABLE_COVERAGE)
  target_compile_options(immer-dev INTERFACE "--coverage")
  target_link_libraries(immer-dev INTERFACE "--coverage")
endif()

#  Testing
#  =======

enable_testing()

add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Build and run all the tests and examples.")

add_subdirectory(test)
add_subdirectory(benchmark)
# [2020-05-07, Tobias Gödderz] Commented out so the custom target "examples"
# doesn't clash with ours
# add_subdirectory(example)
add_subdirectory(doc)
add_subdirectory(extra/fuzzer)
add_subdirectory(extra/python)
add_subdirectory(extra/guile)
