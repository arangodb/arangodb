# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

project(CmakeConfigPythonTest LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../BoostVersion.cmake)

find_package(PythonLibs 2.7 REQUIRED)

set(component python)

if(USE_BOOST_PACKAGE)

  # test the versioned pythonXY component for backward compatibility

  string(REPLACE "." ";" PYTHONLIBS_VERSION_LIST ${PYTHONLIBS_VERSION_STRING})

  list(GET PYTHONLIBS_VERSION_LIST 0 PYTHONLIBS_VERSION_MAJOR)
  list(GET PYTHONLIBS_VERSION_LIST 1 PYTHONLIBS_VERSION_MINOR)

  set(component python${PYTHONLIBS_VERSION_MAJOR}${PYTHONLIBS_VERSION_MINOR})

  find_package(Boost ${BOOST_VERSION} EXACT REQUIRED COMPONENTS ${component})

else()

  # use the found Python version in case more than one Boost.Python is installed
  set(Boost_PYTHON_VERSION ${PYTHONLIBS_VERSION_STRING})

  find_package(boost_python ${BOOST_VERSION} EXACT CONFIG REQUIRED)

endif()

add_executable(main quick.cpp)
target_link_libraries(main Boost::${component} ${PYTHON_LIBRARIES})
target_include_directories(main PRIVATE ${PYTHON_INCLUDE_DIRS})

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

add_test(main main)
