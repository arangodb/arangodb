# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5)
project(BoostTypeTraits)

add_library(boost_type_traits INTERFACE)
add_library(Boost::type_traits ALIAS boost_type_traits)

target_include_directories(boost_type_traits INTERFACE include)

target_link_libraries(boost_type_traits
    INTERFACE
        Boost::config
        Boost::static_assert
)

