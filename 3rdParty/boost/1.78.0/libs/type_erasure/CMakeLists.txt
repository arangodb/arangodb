# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_type_erasure VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_type_erasure
  src/dynamic_binding.cpp
)

add_library(Boost::type_erasure ALIAS boost_type_erasure)

target_include_directories(boost_type_erasure PUBLIC include)

target_link_libraries(boost_type_erasure
  PUBLIC
    Boost::assert
    Boost::config
    Boost::core
    Boost::fusion
    Boost::iterator
    Boost::mp11
    Boost::mpl
    Boost::preprocessor
    Boost::smart_ptr
    Boost::throw_exception
    Boost::type_traits
    Boost::typeof
    Boost::vmd
  PRIVATE
    Boost::thread
)

target_compile_definitions(boost_type_erasure
  PUBLIC BOOST_TYPE_ERASURE_NO_LIB
  # Source files already define BOOST_TYPE_ERASURE_SOURCE
  # PRIVATE BOOST_TYPE_ERASURE_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_type_erasure PUBLIC BOOST_TYPE_ERASURE_DYN_LINK)
else()
  target_compile_definitions(boost_type_erasure PUBLIC BOOST_TYPE_ERASURE_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()

