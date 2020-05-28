# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Mpl is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostMpl LANGUAGES CXX )

add_library( boost_mpl INTERFACE )
add_library( Boost::mpl ALIAS boost_mpl )

target_include_directories( boost_mpl INTERFACE include )

target_link_libraries( boost_mpl
    INTERFACE
        Boost::config
        Boost::core
        Boost::predef
        Boost::preprocessor
        Boost::static_assert
        Boost::type_traits
        Boost::utility
)

