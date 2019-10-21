# Copyright 2018 Glen Joseph Fernandes
# (glenjofe@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# (http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

project(BoostCircularBuffer LANGUAGES CXX)

add_library(boost_circular_buffer INTERFACE)

add_library(Boost::circular_buffer ALIAS boost_circular_buffer)

target_include_directories(boost_circular_buffer INTERFACE include)

target_link_libraries(boost_circular_buffer INTERFACE
    Boost::assert
    Boost::concept_check
    Boost::config
    Boost::core
    Boost::move
    Boost::static_assert
    Boost::throw_exception
    Boost::type_traits)
