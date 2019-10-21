# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Rational is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostRational LANGUAGES CXX)
option( BOOST_RATIONAL_INCLUDE_TESTS "Add boost rational tests" OFF )

add_library( boost_rational INTERFACE )
add_library( Boost::rational ALIAS boost_rational )

target_include_directories( boost_rational INTERFACE include )

target_link_libraries( boost_rational
    INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::integer
        Boost::static_assert
        Boost::throw_exception
        Boost::type_traits
        Boost::utility
)

if( BOOST_RATIONAL_INCLUDE_TESTS )
    enable_testing()
    add_subdirectory( test )
endif()

