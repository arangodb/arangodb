# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

project(CmakeConfigIostreamsTest LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../BoostVersion.cmake)

if(USE_BOOST_PACKAGE)

  find_package(Boost ${BOOST_VERSION} EXACT REQUIRED COMPONENTS iostreams)

else()

  find_package(boost_iostreams ${BOOST_VERSION} EXACT CONFIG REQUIRED)

endif()

add_executable(test_gzip test_gzip.cpp)
target_link_libraries(test_gzip Boost::iostreams)

add_executable(test_bzip2 test_bzip2.cpp)
target_link_libraries(test_bzip2 Boost::iostreams)

if(NOT WIN32)

  find_package(ZLIB)
  target_link_libraries(test_gzip ZLIB::ZLIB)

  find_package(BZip2)
  target_link_libraries(test_bzip2 BZip2::BZip2)

endif()

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

add_test(NAME test_gzip COMMAND test_gzip WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_bzip2 COMMAND test_bzip2 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
