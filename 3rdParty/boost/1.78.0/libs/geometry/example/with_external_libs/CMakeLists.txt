# Boost.Geometry
# Example CMakeLists.txt building the Boost.Geometry with wxWidget example
#
# Copyright (c) 2021-2021 Barend Gehrels, Amsterdam, the Netherlands.

# Use, modification and distribution is subject to the Boost Software License,
# Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.10)

project(with_external_libs)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set BOOST_ROOT in your environment (this is cmake default)
find_package(Boost)

# Set WX_ROOT, similarly, also in your environment
set(WX_ROOT $ENV{WX_ROOT})
message(STATUS "Using wxWidgets from this folder: " $ENV{WX_ROOT})

# WX Widgets
link_directories(${WX_ROOT}/lib)
add_executable(wx x04_wxwidgets_world_mapper.cpp)
target_include_directories(wx PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(wx PRIVATE ${WX_ROOT}/include)
target_include_directories(wx PRIVATE ${WX_ROOT}/include/wx-3.1)

# WX configuration (get the values using wx-config --cxxflags and wx-config --libs)
target_compile_definitions(wx PRIVATE WXUSINGDLL __WXGTK2__ __WXGTK__)
target_link_libraries(wx PRIVATE wx_gtk2u_html-3.1 wx_gtk2u_core-3.1 wx_baseu_net-3.1 wx_baseu-3.1)

