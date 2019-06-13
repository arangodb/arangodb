# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5)
project(BoostPreprocessor LANGUAGES CXX)

add_library(boost_preprocessor INTERFACE)
add_library(Boost::preprocessor ALIAS boost_preprocessor)

target_include_directories(boost_preprocessor INTERFACE include)

