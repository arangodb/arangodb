# Copyright 2018 Peter Dimov
# Copyright 2018 Andrey Semashev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(boost_winapi VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_winapi INTERFACE)
add_library(Boost::winapi ALIAS boost_winapi)

target_include_directories(boost_winapi INTERFACE include)

target_link_libraries(boost_winapi
    INTERFACE
        Boost::config
        Boost::predef
)
