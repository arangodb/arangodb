# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required( VERSION 3.5...3.20 )
project( boost_variant VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX )

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
