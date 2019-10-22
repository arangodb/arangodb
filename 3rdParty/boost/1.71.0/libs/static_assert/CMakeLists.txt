# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5)
project(BoostStaticAssert LANGUAGES CXX)

add_library(boost_static_assert INTERFACE)
add_library(Boost::static_assert ALIAS boost_static_assert)

target_include_directories(boost_static_assert INTERFACE include)

target_link_libraries(boost_static_assert
    INTERFACE
        Boost::config
)
