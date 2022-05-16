# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)
project(boost_fusion VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_fusion INTERFACE)
add_library(Boost::fusion ALIAS boost_fusion)

target_include_directories(boost_fusion INTERFACE include)

target_link_libraries(boost_fusion
    INTERFACE
        Boost::config
        Boost::container_hash
        Boost::core
        Boost::function_types
        Boost::mpl
        Boost::preprocessor
        Boost::static_assert
        Boost::tuple
        Boost::type_traits
        Boost::typeof
        Boost::utility
)
