# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# http://www.boost.org/LICENSE_1_0.txt

# Partial (add_subdirectory only) and experimental CMake support
# Subject to change; please do not rely on the contents of this file yet

cmake_minimum_required(VERSION 3.5)

project(BoostVariant2 LANGUAGES CXX)

add_library(boost_variant2 INTERFACE)
add_library(Boost::variant2 ALIAS boost_variant2)

target_include_directories(boost_variant2 INTERFACE include)

target_link_libraries(boost_variant2
    INTERFACE
        Boost::config
        Boost::mp11
)
