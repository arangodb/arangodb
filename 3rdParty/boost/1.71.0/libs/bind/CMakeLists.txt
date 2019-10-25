# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet

cmake_minimum_required(VERSION 3.5)
project(BoostBind LANGUAGES CXX)

add_library(boost_bind INTERFACE)
add_library(Boost::bind ALIAS boost_bind)

target_include_directories(boost_bind INTERFACE include)

target_link_libraries(boost_bind
    INTERFACE
        Boost::config
        Boost::core
)
