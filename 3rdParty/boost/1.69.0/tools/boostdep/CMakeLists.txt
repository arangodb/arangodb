# Copyright 2018 Mike Dev
# Copyright 2018 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required( VERSION 3.1 )

project( boostdep LANGUAGES CXX )

add_executable( boostdep src/boostdep.cpp )

find_package( Boost COMPONENTS filesystem REQUIRED )
target_link_libraries( boostdep Boost::filesystem )

install( TARGETS boostdep RUNTIME DESTINATION bin )
