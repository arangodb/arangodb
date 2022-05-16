# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)
project(boost_pool VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

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
