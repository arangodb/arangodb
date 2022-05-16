# Copyright 2018 Peter Dimov
# Copyright 2018 Andrey Semashev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(boost_detail VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_detail INTERFACE)
add_library(Boost::detail ALIAS boost_detail)

target_include_directories(boost_detail INTERFACE include)

target_link_libraries(boost_detail
    INTERFACE
        Boost::config
        Boost::core
        Boost::preprocessor
        Boost::static_assert
        Boost::type_traits
)
