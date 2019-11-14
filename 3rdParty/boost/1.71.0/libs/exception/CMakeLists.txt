# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Exception is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostException LANGUAGES CXX )

# We treat Boost.Exception as header only for now.
# See https://github.com/boostorg/exception/pull/17
# for more information.

add_library( boost_exception INTERFACE )
add_library( Boost::exception ALIAS boost_exception )

target_include_directories( boost_exception INTERFACE include )

target_link_libraries( boost_exception
    INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::smart_ptr
        Boost::throw_exception
        Boost::tuple
        Boost::type_traits
)

