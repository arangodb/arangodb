# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_timer VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_timer
    src/auto_timers_construction.cpp
    src/cpu_timer.cpp
)

add_library(Boost::timer ALIAS boost_timer)

target_include_directories(boost_timer PUBLIC include)

target_compile_definitions(boost_timer
  PUBLIC
    BOOST_TIMER_NO_LIB
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_timer PUBLIC BOOST_TIMER_DYN_LINK)
else()
  target_compile_definitions(boost_timer PUBLIC BOOST_TIMER_STATIC_LINK)
endif()

target_link_libraries(boost_timer
  PUBLIC
    Boost::config
    Boost::core
    Boost::system
  PRIVATE
    Boost::chrono
    Boost::io
    Boost::predef
    Boost::throw_exception
)

if(BUILD_TESTING)

    add_subdirectory(test)

endif()
