# Copyright 2018 Mike Dev
# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_move VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_move INTERFACE)
add_library(Boost::move ALIAS boost_move)

target_include_directories(boost_move INTERFACE include)

target_link_libraries(boost_move
  INTERFACE
    Boost::assert
    Boost::config
    Boost::core
    Boost::static_assert
)

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
