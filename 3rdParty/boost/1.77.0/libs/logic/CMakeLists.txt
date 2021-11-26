# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required( VERSION 3.5...3.20 )
project( boost_logic VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX )

add_library( boost_logic INTERFACE )
add_library( Boost::logic ALIAS boost_logic )

target_include_directories( boost_logic INTERFACE include )

target_link_libraries( boost_logic
    INTERFACE
        Boost::config
        Boost::core
)
