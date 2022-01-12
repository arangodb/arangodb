# Copyright 2021 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_subdir_test LANGUAGES CXX)

# Those 2 should work the same
# while using find_package for the installed Boost avoids the need to manually specify dependencies
if(DEFINED BOOST_CI_INSTALL_TEST AND BOOST_CI_INSTALL_TEST)
    find_package(boost_nowide 1.78 REQUIRED)
elseif(DEFINED BOOST_NOWIDE_INSTALL_TEST AND BOOST_NOWIDE_INSTALL_TEST)
    find_package(boost_nowide 11 REQUIRED)
else()
    add_subdirectory(../../../config boostorg/config)
    add_subdirectory(../.. boostorg/nowide)
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::nowide)

enable_testing()
add_test(NAME main COMMAND main)
