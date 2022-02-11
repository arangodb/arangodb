# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required( VERSION 3.5...3.20 )
project( boost_conversion VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX )

add_library( boost_conversion INTERFACE )
add_library( Boost::conversion ALIAS boost_conversion )

target_include_directories( boost_conversion INTERFACE include )

target_link_libraries( boost_conversion
    INTERFACE
        Boost::assert
        Boost::config
        Boost::core
        Boost::smart_ptr
        Boost::throw_exception
        Boost::type_traits
        Boost::typeof
)
