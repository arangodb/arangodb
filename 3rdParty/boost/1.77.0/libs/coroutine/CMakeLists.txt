# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_coroutine VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

if(WIN32 AND NOT CMAKE_CXX_PLATFORM_ID MATCHES "Cygwin")
  set(STACK_TRAITS_SOURCES
    src/windows/stack_traits.cpp
  )
else()
  set(STACK_TRAITS_SOURCES
    src/posix/stack_traits.cpp
  )
endif()

add_library(boost_coroutine
  src/detail/coroutine_context.cpp
  src/exceptions.cpp
  ${STACK_TRAITS_SOURCES}
)

add_library(Boost::coroutine ALIAS boost_coroutine)

target_include_directories(boost_coroutine PUBLIC include)

target_link_libraries(boost_coroutine
  PUBLIC
    Boost::assert
    Boost::config
    Boost::context
    Boost::core
    Boost::exception
    Boost::move
    Boost::system
    Boost::throw_exception
    Boost::type_traits
    Boost::utility
  PRIVATE
    Boost::thread
)

target_compile_definitions(boost_coroutine
  PUBLIC BOOST_COROUTINE_NO_LIB
  PRIVATE BOOST_COROUTINE_SOURCE BOOST_COROUTINES_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_coroutine PUBLIC BOOST_COROUTINE_DYN_LINK BOOST_COROUTINES_DYN_LINK)
else()
  target_compile_definitions(boost_coroutine PUBLIC BOOST_COROUTINE_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
