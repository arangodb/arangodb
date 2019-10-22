# Copyright 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet

cmake_minimum_required(VERSION 3.5)
project(BoostEndian LANGUAGES CXX)

add_library(boost_endian INTERFACE)
add_library(Boost::endian ALIAS boost_endian)

target_include_directories(boost_endian INTERFACE include)

target_link_libraries(boost_endian
    INTERFACE
        Boost::config
        Boost::core
        Boost::predef
        Boost::static_assert
        Boost::type_traits
)
