# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.TypeIndex is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required(VERSION 3.5)
project(Boost.TypeIndex)

add_library(boost_type_index INTERFACE)
add_library(Boost::type_index ALIAS boost_type_index)

target_include_directories(boost_type_index INTERFACE include)

target_link_libraries(boost_type_index
    INTERFACE
        Boost::config
        Boost::container_hash
        Boost::core
        Boost::preprocessor
        Boost::smart_ptr
        Boost::static_assert
        Boost::throw_exception
        Boost::type_traits
)
