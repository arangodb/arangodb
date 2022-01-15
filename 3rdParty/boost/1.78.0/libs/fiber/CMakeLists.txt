# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...3.20)

project(boost_fiber VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

if(WIN32 AND NOT CMAKE_CXX_PLATFORM_ID MATCHES "Cygwin")
  set(_default_target windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(_default_target linux)
else()
  set(_default_target none)
endif()

set(BOOST_FIBER_NUMA_TARGET_OS "${_default_target}" CACHE STRING "Boost.Fiber target OS (aix, freebsd, hpux, linux, solaris, windows, none)")
set_property(CACHE BOOST_FIBER_NUMA_TARGET_OS PROPERTY STRINGS aix freebsd hpux linux solaris windows none)

unset(_default_target)

message(STATUS "Boost.Fiber: NUMA target OS is ${BOOST_FIBER_NUMA_TARGET_OS}")

# boost_fiber

add_library(boost_fiber
  src/algo/algorithm.cpp
  src/algo/round_robin.cpp
  src/algo/shared_work.cpp
  src/algo/work_stealing.cpp
  src/barrier.cpp
  src/condition_variable.cpp
  src/context.cpp
  src/fiber.cpp
  src/future.cpp
  src/mutex.cpp
  src/properties.cpp
  src/recursive_mutex.cpp
  src/recursive_timed_mutex.cpp
  src/scheduler.cpp
  src/timed_mutex.cpp
  src/waker.cpp
)

add_library(Boost::fiber ALIAS boost_fiber)

target_include_directories(boost_fiber PUBLIC include)

target_link_libraries(boost_fiber
  PUBLIC
    Boost::assert
    Boost::config
    Boost::context
    Boost::core
    Boost::intrusive
    Boost::predef
    Boost::smart_ptr
)

target_compile_features(boost_fiber PUBLIC cxx_std_11)

target_compile_definitions(boost_fiber
  PUBLIC BOOST_FIBER_NO_LIB
  PRIVATE BOOST_FIBER_SOURCE BOOST_FIBERS_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_fiber PUBLIC BOOST_FIBER_DYN_LINK BOOST_FIBERS_DYN_LINK)
else()
  target_compile_definitions(boost_fiber PUBLIC BOOST_FIBER_STATIC_LINK)
endif()

# boost_fiber_numa

if(BOOST_FIBER_NUMA_TARGET_OS STREQUAL none)
  set(NUMA_SOURCES
    src/numa/pin_thread.cpp
    src/numa/topology.cpp
  )
else()
  set(NUMA_SOURCES
    src/numa/${BOOST_FIBER_NUMA_TARGET_OS}/pin_thread.cpp
    src/numa/${BOOST_FIBER_NUMA_TARGET_OS}/topology.cpp
  )
endif()

add_library(boost_fiber_numa
  ${NUMA_SOURCES}
  src/numa/algo/work_stealing.cpp
)

add_library(Boost::fiber_numa ALIAS boost_fiber_numa)

target_include_directories(boost_fiber_numa PUBLIC include)

target_link_libraries(boost_fiber_numa
  PUBLIC
    Boost::assert
    Boost::config
    Boost::context
    Boost::fiber
    Boost::smart_ptr
  PRIVATE
    Boost::algorithm
    Boost::filesystem
    Boost::format
)

target_compile_definitions(boost_fiber_numa
  PUBLIC BOOST_FIBER_NO_LIB
  PRIVATE BOOST_FIBER_SOURCE BOOST_FIBERS_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_fiber_numa PUBLIC BOOST_FIBER_DYN_LINK BOOST_FIBERS_DYN_LINK)
else()
  target_compile_definitions(boost_fiber_numa PUBLIC BOOST_FIBER_STATIC_LINK)
endif()

# Install

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(TARGETS boost_fiber boost_fiber_numa VERSION ${BOOST_SUPERPROJECT_VERSION} HEADER_DIRECTORY include)
endif()

# Test

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
