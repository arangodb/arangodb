# Copyright 2018 Glen Joseph Fernandes
# (glenjofe@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# (http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

project(BoostPolygon LANGUAGES CXX)

add_library(boost_polygon INTERFACE)

add_library(Boost::polygon ALIAS boost_polygon)

target_include_directories(boost_polygon INTERFACE include)

target_link_libraries(boost_polygon INTERFACE Boost::config)
