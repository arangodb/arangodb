# Copyright 2019 Sam Day
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.dynamic_bitset is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required(VERSION 3.5)
project(BoostDynamicBitset LANGUAGES CXX)

add_library(boost_dynamic_bitset INTERFACE)
add_library(Boost::dynamic_bitset ALIAS boost_dynamic_bitset)

target_include_directories(boost_dynamic_bitset INTERFACE include)

target_link_libraries(boost_dynamic_bitset
        INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::integer
        Boost::move
        Boost::static_assert
        Boost::throw_exception
        )
