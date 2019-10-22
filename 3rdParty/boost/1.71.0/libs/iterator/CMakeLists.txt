# Copyright 2018 Peter Dimov
# Copyright 2018 Andrey Semashev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet.

cmake_minimum_required(VERSION 3.5)

project(BoostIterator LANGUAGES CXX)

add_library(boost_iterator INTERFACE)
add_library(Boost::iterator ALIAS boost_iterator)

target_include_directories(boost_iterator INTERFACE include)

target_link_libraries(boost_iterator
    INTERFACE
        Boost::assert
        Boost::concept_check
        Boost::config
        Boost::conversion
        Boost::core
        Boost::detail
        Boost::function_types
        Boost::fusion
        Boost::mpl
        Boost::optional
        Boost::smart_ptr
        Boost::static_assert
        Boost::type_traits
        Boost::utility
)
