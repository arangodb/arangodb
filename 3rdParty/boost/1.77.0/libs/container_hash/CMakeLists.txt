# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)
project(boost_container_hash VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_container_hash INTERFACE)
add_library(Boost::container_hash ALIAS boost_container_hash)

target_include_directories(boost_container_hash INTERFACE include)

target_link_libraries(boost_container_hash
    INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::detail
        Boost::integer
        Boost::static_assert
        Boost::type_traits
)
