# Copyright 2001, Raffi Enficiaud

# Use, modification, and distribution are subject to the
# Boost Software License, Version 1.0. (See accompanying file
# LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# See http://www.boost.org/libs/test for the library home page.

cmake_minimum_required(VERSION 2.8.11)
project(BoostTest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MACOSX_RPATH ON)

add_definitions(-DBOOST_TEST_NO_LIB)

# build type, by default to release (with optimisations) 
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT WITHOUT_TESTS)
  # ctest sets BUILD_TESTING automatically, but does not seem to serve its purpose.
  include(CTest)
  enable_testing()
endif()


include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)

if(NOT MSVC)
  # c++11 options
  check_cxx_compiler_flag(-std=c++11 HAS_CXX11_FLAG)
  check_cxx_compiler_flag(-std=c++0x HAS_CXX0X_FLAG)
  if(HAS_CXX11_FLAG)
    message(STATUS "Compiling with C++11 support")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  elseif(HAS_CXX0X_FLAG)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  endif()
endif()

if(MSVC)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
  set(MSVC_Additional_flags "/fp:fast /GF /Oy /GT /Ox /Ob2 /Oi /Os")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${MSVC_Additional_flags}")
endif()





# global path
set(BOOST_TEST_ROOT_DIR ${BoostTest_SOURCE_DIR}/..)
set(BOOST_ROOT_DIR ${BOOST_TEST_ROOT_DIR}/../..)
get_filename_component(BOOST_TEST_ROOT_DIR_ABS ${BoostTest_SOURCE_DIR}/.. ABSOLUTE)
get_filename_component(BOOST_ROOT_DIR_ABS ${BOOST_ROOT_DIR} ABSOLUTE)

# global include on boost
include_directories(${BOOST_ROOT_DIR_ABS}/)

# include globs
file(GLOB_RECURSE
     BOOST_UTF_HEADERS
     ${BOOST_TEST_ROOT_DIR}/include/*.hpp
     ${BOOST_TEST_ROOT_DIR}/include/*.ipp)

# organize files
foreach(_h IN LISTS BOOST_UTF_HEADERS)
  get_filename_component(_hh ${_h} ABSOLUTE)
  file(RELATIVE_PATH _v ${BOOST_TEST_ROOT_DIR_ABS}/include/boost/test ${_hh})
  get_filename_component(_v "${_v}" DIRECTORY)
  string(REPLACE "/" "\\" _v "${_v}")
  source_group(${_v} FILES ${_h})
endforeach()

set(BOOST_UTF_SRC
    ${BOOST_TEST_ROOT_DIR}/src/compiler_log_formatter.cpp

    ${BOOST_TEST_ROOT_DIR}/src/debug.cpp
    ${BOOST_TEST_ROOT_DIR}/src/decorator.cpp
    ${BOOST_TEST_ROOT_DIR}/src/execution_monitor.cpp
    ${BOOST_TEST_ROOT_DIR}/src/framework.cpp
    ${BOOST_TEST_ROOT_DIR}/src/junit_log_formatter.cpp
    ${BOOST_TEST_ROOT_DIR}/src/plain_report_formatter.cpp
    ${BOOST_TEST_ROOT_DIR}/src/progress_monitor.cpp
    ${BOOST_TEST_ROOT_DIR}/src/results_collector.cpp
    ${BOOST_TEST_ROOT_DIR}/src/results_reporter.cpp
    ${BOOST_TEST_ROOT_DIR}/src/test_tools.cpp
    ${BOOST_TEST_ROOT_DIR}/src/test_tree.cpp
    ${BOOST_TEST_ROOT_DIR}/src/unit_test_log.cpp
    ${BOOST_TEST_ROOT_DIR}/src/unit_test_main.cpp
    ${BOOST_TEST_ROOT_DIR}/src/unit_test_monitor.cpp
    ${BOOST_TEST_ROOT_DIR}/src/unit_test_parameters.cpp
    ${BOOST_TEST_ROOT_DIR}/src/xml_log_formatter.cpp
    ${BOOST_TEST_ROOT_DIR}/src/xml_report_formatter.cpp
    )

add_library(boost_test_framework
            STATIC
            ${BOOST_UTF_HEADERS}
            ${BOOST_UTF_SRC})
target_compile_definitions(boost_test_framework PUBLIC "-DBOOST_TEST_DYN_LINK=0")
target_include_directories(boost_test_framework PUBLIC ${BOOST_TEST_ROOT_DIR}/include/)
set_target_properties(boost_test_framework PROPERTIES FOLDER "UTF")


####
# Documentation files (files only, no target)
file(GLOB_RECURSE
     BOOST_UTF_DOC_FILES
     ${BOOST_TEST_ROOT_DIR}/doc/*.qbk)
add_custom_target(
  quickbook
  SOURCES ${BOOST_UTF_DOC_FILES})
set_property(TARGET quickbook PROPERTY FOLDER "Documentation/")


####
# Unit tests

# documentation tests
file(GLOB_RECURSE
     BOOST_UTF_DOC_EXAMPLES
     ${BOOST_TEST_ROOT_DIR}/doc/examples/*.cpp)

foreach(_h IN LISTS BOOST_UTF_DOC_EXAMPLES)
  get_filename_component(_hh ${_h} NAME_WE)
  add_executable(${_hh} ${_h} ${BOOST_TEST_ROOT_DIR}/doc/examples/${_hh}.output)
  target_include_directories(${_hh} PUBLIC ${BOOST_TEST_ROOT_DIR}/include/)
  set_target_properties(${_hh} PROPERTIES FOLDER "Doc examples")

  add_test(NAME doc-${_hh}
           COMMAND ${_hh})
  get_filename_component(_ext ${_h} EXT)
  string(FIND ${_ext} "fail" _index_fail)
  if(${_index_fail} GREATER -1)
    message(STATUS "test ${_hh}.${_ext} = ${_index_fail}")
    set_tests_properties(doc-${_hh} PROPERTIES WILL_FAIL TRUE)
  endif()
endforeach()


# unit tests folder
set(BOOST_TEST_UNITTESTS_FOLDER ${BOOST_TEST_ROOT_DIR}/test)

# datasets
file(GLOB
     BOOST_TEST_UNITTESTS_DATASET
     ${BOOST_TEST_UNITTESTS_FOLDER}/test-organization-ts/datasets-test/*.cpp
     ${BOOST_TEST_UNITTESTS_FOLDER}/test-organization-ts/datasets-test/*.hpp)
add_executable(boost_test_datasets ${BOOST_TEST_UNITTESTS_DATASET})
target_include_directories(boost_test_datasets PUBLIC ${BOOST_TEST_ROOT_DIR}/include/)
target_link_libraries(boost_test_datasets boost_test_framework)
target_compile_definitions(boost_test_datasets PUBLIC "BOOST_TEST_DYN_LINK=1")
set_target_properties(boost_test_datasets PROPERTIES FOLDER "Unit tests")
add_test(NAME bt-unittest-dataset
         COMMAND boost_test_datasets)


####
# TS writing-test-ts

set(BOOST_UTF_TESTS_IND_FILES
    writing-test-ts
    execution_monitor-ts
    framework-ts
    usage-variants-ts
    utils-ts
    test-organization-ts)
    

foreach(_ts IN LISTS BOOST_UTF_TESTS_IND_FILES)

  message("parsing test suite ${_ts}")
  file(GLOB
       _boost_utf_current_tsuite
       ${BOOST_TEST_UNITTESTS_FOLDER}/${_ts}/*.cpp)


  foreach(_h IN LISTS _boost_utf_current_tsuite)
    get_filename_component(_hh ${_h} ABSOLUTE)
    get_filename_component(_name ${_h} NAME_WE)
    file(RELATIVE_PATH _v ${BOOST_TEST_UNITTESTS_FOLDER} ${_hh})
    #get_filename_component(_v "${_v}" DIRECTORY)
    message("adding ${_ts}/${_name}")
    add_executable(${_name} ${_hh})
    target_link_libraries(${_name} PRIVATE boost_test_framework)
    set_target_properties(${_name} PROPERTIES FOLDER "Unit tests/${_ts}")
    add_test(NAME bt-unittest-${_name}
             COMMAND ${_name})
  endforeach()

  unset(_boost_utf_current_tsuite)

endforeach() # test suite

