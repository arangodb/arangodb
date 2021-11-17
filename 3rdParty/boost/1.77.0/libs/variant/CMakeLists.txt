# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Variant is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostVariant LANGUAGES CXX )

add_library( boost_variant INTERFACE )
add_library( Boost::variant ALIAS boost_variant )

target_include_directories( boost_variant INTERFACE include )

target_link_libraries( boost_variant
    INTERFACE
        Boost::assert
        Boost::bind
        Boost::config
        Boost::container_hash
        Boost::core
        Boost::detail
        Boost::integer
        Boost::move
        Boost::mpl
        Boost::preprocessor
        Boost::static_assert
        Boost::throw_exception
        Boost::type_index
        Boost::type_traits
        Boost::utility
)
