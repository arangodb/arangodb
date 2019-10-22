# CMake build control file for safe numerics Library Examples

cmake_minimum_required(VERSION 3.8.6)

project("SafeIntegers")
enable_language(CXX)

########################################################
# Create interface library
#
add_library(boost_safe_numerics INTERFACE)
add_library(Boost::safe_numerics ALIAS boost_safe_numerics)

target_include_directories(boost_safe_numerics INTERFACE "${Boost_INCLUDE_DIRS}")
target_compile_features(boost_safe_numerics INTERFACE cxx_std_14)

########################################################
# Compiler settings - special settings for known compilers
#

message(STATUS "compiler is ${CMAKE_CXX_COMPILER_ID}" )

if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
  add_definitions( -ftemplate-depth=300 )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
  add_definitions( /wd4996 )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
  add_definitions( -ftemplate-depth=300 )
  add_compile_options(-Wnon-virtual-dtor -ansi -Wcast-align -Wchar-subscripts -Wall -W -Wshadow -Wsign-compare )
endif()

#
# Project settings
#

find_package(Boost )

if(Boost_FOUND)
  if( CMAKE_HOST_APPLE )
    set(Boost_ADDRESS_MODEL 64 CACHE STRING "32/64 bits")
  endif()
  message(STATUS "Boost directories found at ${Boost_INCLUDE_DIRS}")
  message(STATUS "Boost version found is ${Boost_VERSION}")
  include_directories("${Boost_INCLUDE_DIRS}")  # note: sets header search path?
elseif()
    message("Boost NOT Found!")
endif()

message(STATUS "include directories are:" )
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "   ${dir}")
endforeach()

#use folders in organization of the IDE
set(USE_FOLDERS TRUE)

###########################
# testing and submitting test results to the test dashboard

if(0)

## Create a file named CTestConfig.cmake adjacent to the current file.
## This new file should contain the following:

set(CTEST_PROJECT_NAME "Safe Numerics")
set(CTEST_NIGHTLY_START_TIME "01:00:00 UTC")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
# set(CTEST_DROP_LOCATION "/cdash/submit.php?project=MyProject")
set(CTEST_DROP_LOCATION "/index.php?project=Safe+Numerics")
set(CTEST_DROP_SITE_CDASH TRUE)

endif()

function(test_run_pass base_name )
  message(STATUS ${base_name})
  add_executable(${base_name} ${base_name}.cpp)
  add_test(NAME ${base_name} COMMAND ${base_name})
endfunction(test_run_pass)

function(test_compile_pass base_name )
  message(STATUS ${base_name} - compile only)
  add_library(${base_name} OBJECT ${base_name})
endfunction(test_compile_pass)

function(test_compile_fail base_name )
  message(STATUS ${base_name})
  # Add failing-to-compile targets
  add_executable(${base_name} ${base_name}.cpp)

  # Avoid building these targets normally
  set_target_properties(${base_name} PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
  )

  # Add the tests.  These invoke "cmake --build ..." which is a
  # cross-platform way of building the given target.
  add_test(NAME ${base_name}
    COMMAND ${CMAKE_COMMAND} --build . --target ${base_name} --config $<CONFIGURATION>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  # Expect these tests to fail
  # (i.e. cmake --build should return a non-zero value)
  set_tests_properties(${base_name} PROPERTIES WILL_FAIL TRUE)
endfunction( test_compile_fail base_name )

########################################################
# End Compiler settings
#

enable_testing()

add_subdirectory("include/boost/safe_numerics")
add_subdirectory("example")
add_subdirectory("test")

