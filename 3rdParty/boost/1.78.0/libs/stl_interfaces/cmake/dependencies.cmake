# Copyright Louis Dionne 2016
# Copyright Zach Laine 2016-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

###############################################################################
# Boost
###############################################################################
set(Boost_USE_STATIC_LIBS ON)
if (NOT BOOST_BRANCH)
  set(BOOST_BRANCH master)
endif()
if (NOT EXISTS ${CMAKE_BINARY_DIR}/boost_root)
  add_custom_target(
    boost_root_clone
    git clone --depth 100 -b ${BOOST_BRANCH}
      https://github.com/boostorg/boost.git boost_root
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  if (MSVC)
    set(bootstrap_cmd ./bootstrap.bat)
  else()
    set(bootstrap_cmd ./bootstrap.sh)
  endif()
  add_custom_target(
    boost_clone
    COMMAND git submodule init libs/assert
    COMMAND git submodule init libs/config
    COMMAND git submodule init libs/core
    COMMAND git submodule init tools/build
    COMMAND git submodule init libs/headers
    COMMAND git submodule init tools/boost_install
    COMMAND git submodule update --jobs 3
    COMMAND ${bootstrap_cmd}
    COMMAND ./b2 headers
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/boost_root
    DEPENDS boost_root_clone)
endif ()
add_library(boost INTERFACE)
add_dependencies(boost boost_clone)
target_include_directories(boost INTERFACE ${CMAKE_BINARY_DIR}/boost_root)
