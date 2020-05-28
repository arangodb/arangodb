# Copyright 2019 Sam Day
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.lexical_cast is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required(VERSION 3.5)
project(BoostLexicalCast LANGUAGES CXX)

add_library(boost_lexical_cast INTERFACE)
add_library(Boost::lexical_cast ALIAS boost_lexical_cast)

target_include_directories(boost_lexical_cast INTERFACE include)

target_link_libraries(boost_lexical_cast
        INTERFACE
        Boost::array
        Boost::assert
        Boost::config
        Boost::container
        Boost::core
        Boost::detail
        Boost::integer
        Boost::math
        Boost::numeric_conversion
        Boost::range
        Boost::static_assert
        Boost::throw_exception
        Boost::type_traits
        )
