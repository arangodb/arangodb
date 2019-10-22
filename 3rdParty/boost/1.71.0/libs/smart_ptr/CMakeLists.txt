# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet

cmake_minimum_required(VERSION 3.5)
project(BoostSmartPtr LANGUAGES CXX)

add_library(boost_smart_ptr INTERFACE)
add_library(Boost::smart_ptr ALIAS boost_smart_ptr)

target_include_directories(boost_smart_ptr INTERFACE include)

target_link_libraries(boost_smart_ptr
    INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::move
        Boost::predef
        Boost::static_assert
        Boost::throw_exception
        Boost::type_traits
)
