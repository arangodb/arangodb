# Copyright 2019 - 2021 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wsuggest-override _BOOST_NOWIDE_SUGGEST_OVERRIDE_SUPPORTED)

add_library(boost_nowide_file_test_helpers STATIC file_test_helpers.cpp)
target_link_libraries(boost_nowide_file_test_helpers PRIVATE PRIVATE Boost::nowide)
target_compile_definitions(boost_nowide_file_test_helpers PRIVATE BOOST_ALL_NO_LIB)

if(NOT TARGET tests)
  add_custom_target(tests)
endif()
  
function(boost_nowide_add_test name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "COMPILE_ONLY" "SRC" "LIBRARIES;DEFINITIONS;ARGS")
  if(NOT ARG_SRC)
    set(ARG_SRC ${name}.cpp)
  endif()
  set(name ${PROJECT_NAME}-${name})
  
  add_executable(${name} ${ARG_SRC})
  add_dependencies(tests ${name})
  target_link_libraries(${name} PRIVATE Boost::nowide ${ARG_LIBRARIES})
  boost_add_warnings(${name} pedantic ${Boost_NOWIDE_WERROR})
  if(_BOOST_NOWIDE_SUGGEST_OVERRIDE_SUPPORTED)
    target_compile_options(${name} PRIVATE -Wsuggest-override -Wno-error=suggest-override)
  endif()
  target_compile_definitions(${name} PRIVATE BOOST_ALL_NO_LIB ${ARG_DEFINITIONS})
  if(NOT ARG_COMPILE_ONLY)
    add_test(NAME ${name} COMMAND ${name} ${ARG_ARGS})
  endif()
endfunction()

boost_nowide_add_test(test_codecvt)
boost_nowide_add_test(test_convert)
boost_nowide_add_test(test_env)
boost_nowide_add_test(test_env_win SRC test_env.cpp DEFINITIONS BOOST_NOWIDE_TEST_INCLUDE_WINDOWS)
boost_nowide_add_test(test_filebuf LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_ifstream LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_ofstream LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_fstream LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_fstream_special LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_iostream LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_iostream_interactive COMPILE_ONLY SRC test_iostream.cpp DEFINITIONS BOOST_NOWIDE_TEST_INTERACTIVE LIBRARIES boost_nowide_file_test_helpers)
boost_nowide_add_test(test_stackstring)
boost_nowide_add_test(test_stat)
boost_nowide_add_test(test_stdio)
boost_nowide_add_test(test_system_n SRC test_system.cpp DEFINITIONS BOOST_NOWIDE_TEST_USE_NARROW=1)
if(WIN32)
  boost_nowide_add_test(test_system_w SRC test_system.cpp DEFINITIONS BOOST_NOWIDE_TEST_USE_NARROW=0)
else()
  foreach(test test_filebuf test_ifstream test_ofstream test_fstream test_fstream_special)
    boost_nowide_add_test(${test}_internal SRC ${test}.cpp DEFINITIONS BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT=1 LIBRARIES boost_nowide_file_test_helpers)
  endforeach()
endif()
boost_nowide_add_test(test_traits)

# Test that passthrough writes everything from stdin to stdout
# Needs to be done with CMake as the test driver to write any input to stdin and check output
add_test(
  NAME ${PROJECT_NAME}-test_iostream_passthrough
  COMMAND ${CMAKE_COMMAND}
    -D TEST_BINARY=$<TARGET_FILE:${PROJECT_NAME}-test_iostream>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_iostream_passthrough.cmake
)

if(NOT BOOST_SUPERPROJECT_SOURCE_DIR)
  find_package(Boost 1.56 REQUIRED COMPONENTS filesystem system)
endif()
boost_nowide_add_test(test_fs LIBRARIES Boost::filesystem)
boost_nowide_add_test(test_traits_fs SRC test_traits.cpp LIBRARIES Boost::filesystem DEFINITIONS BOOST_NOWIDE_TEST_BFS_PATH)
boost_nowide_add_test(benchmark_fstream COMPILE_ONLY DEFINITIONS BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT=1)
