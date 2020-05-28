# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Parameter is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostParameter LANGUAGES CXX)

option(BOOST_PARAMETER_INCLUDE_TESTS OFF "Include Boost.Parameter tests")

add_library( boost_parameter INTERFACE )
add_library( Boost::parameter ALIAS boost_parameter )

target_include_directories( boost_parameter INTERFACE include )

target_link_libraries( boost_parameter
    INTERFACE
        Boost::config
        Boost::core
        Boost::function
        Boost::fusion
        # TODO: we only need mp11 or mpl
        Boost::mp11
        Boost::mpl
        Boost::optional
        Boost::preprocessor
        Boost::type_traits
        Boost::utility
)

if(BOOST_PARAMETER_INCLUDE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
