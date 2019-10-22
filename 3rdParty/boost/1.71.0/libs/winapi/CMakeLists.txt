# Copyright 2018 Peter Dimov
# Copyright 2018 Andrey Semashev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet.

cmake_minimum_required(VERSION 3.5)

project(BoostWinAPI LANGUAGES CXX)

add_library(boost_winapi INTERFACE)
add_library(Boost::winapi ALIAS boost_winapi)

target_include_directories(boost_winapi INTERFACE include)

target_link_libraries(boost_winapi
    INTERFACE
        Boost::config
        Boost::predef
)
