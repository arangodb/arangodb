# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_contract VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_contract
  src/contract.cpp
)

add_library(Boost::contract ALIAS boost_contract)

target_include_directories(boost_contract PUBLIC include)

target_link_libraries(boost_contract
  PUBLIC
    Boost::any
    Boost::assert
    Boost::config
    Boost::core
    Boost::exception
    Boost::function
    Boost::function_types
    Boost::mpl
    Boost::optional
    Boost::preprocessor
    Boost::smart_ptr
    Boost::static_assert
    Boost::thread
    Boost::type_traits
    Boost::typeof
    Boost::utility
)

target_compile_definitions(boost_contract
  PUBLIC BOOST_CONTRACT_NO_LIB
  # Source files already define BOOST_CONTRACT_SOURCE
  # PRIVATE BOOST_CONTRACT_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_contract PUBLIC BOOST_CONTRACT_DYN_LINK)
else()
  target_compile_definitions(boost_contract PUBLIC BOOST_CONTRACT_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
