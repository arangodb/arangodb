# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Ratio is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostRatio LANGUAGES CXX)
option( BOOST_RATIO_INCLUDE_TESTS "Add boost ratio tests" OFF )

add_library( boost_ratio INTERFACE )
add_library( Boost::ratio ALIAS boost_ratio )

target_include_directories( boost_ratio INTERFACE include )

target_link_libraries( boost_ratio
    INTERFACE
        Boost::config
        Boost::core
        Boost::integer
        Boost::mpl
        Boost::static_assert
        Boost::type_traits

# NOTE: As of Boost 1.70, the dependency on rational is only
#       necessary, if BOOST_RATIO_EXTENSIONS is defined.
#       Maybe consuming libraries that do so should add
#       Boost::rational as a dependency themselves,
#       instead of doing it here for everyone?
        Boost::rational
)

if( BOOST_RATIO_INCLUDE_TESTS )
    enable_testing()
    add_subdirectory( test )
endif()
