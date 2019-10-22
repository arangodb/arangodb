# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Pool is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required(VERSION 3.5)
project(BoostBind LANGUAGES CXX)

add_library(boost_pool INTERFACE)
add_library(Boost::pool ALIAS boost_pool)

target_include_directories(boost_pool INTERFACE include)

target_link_libraries(boost_pool
    INTERFACE
        Boost::assert
        Boost::config
        Boost::integer
        Boost::throw_exception
        Boost::type_traits
        Boost::winapi
)
