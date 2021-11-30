#
# Copyright (c) 2018 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#

# List headers in order: concepts, core, io, extensions
file(GLOB_RECURSE _hpp_concepts RELATIVE
  "${CMAKE_SOURCE_DIR}/include/boost/gil"
  "${CMAKE_SOURCE_DIR}/include/boost/gil/concepts/*.hpp")
list(APPEND _headers ${_hpp_concepts})

file(GLOB _hpp_core RELATIVE
  "${CMAKE_SOURCE_DIR}/include/boost/gil"
  "${CMAKE_SOURCE_DIR}/include/boost/gil/*.hpp")
list(APPEND _headers ${_hpp_core})

list(APPEND _ext_dirs extension/dynamic_image/)
if(BOOST_GIL_ENABLE_EXT_NUMERIC)
  list(APPEND _ext_dirs extension/numeric)
endif()
if(BOOST_GIL_ENABLE_EXT_TOOLBOX)
  list(APPEND _ext_dirs extension/toolbox)
endif()
if(BOOST_GIL_ENABLE_EXT_IO)
  list(APPEND _ext_dirs io)
  list(APPEND _ext_dirs extension/io)
endif()

foreach(_dir ${_ext_dirs})
  file(GLOB_RECURSE _hpp RELATIVE
    "${CMAKE_SOURCE_DIR}/include/boost/gil"
    "${CMAKE_SOURCE_DIR}/include/boost/gil/${_dir}/*.hpp")
  list(APPEND _headers ${_hpp})
endforeach()

if(NOT BOOST_GIL_ENABLE_EXT_IO_RAW)
  list(FILTER _headers EXCLUDE REGEX "\\/raw[\\.\\/]")
endif()

#-----------------------------------------------------------------------------
# Target: test_headers_self_contained
# Bundles all targets of self-contained header tests,
# functional equivalent to self-contained header tests defined in Jamfile.
#-----------------------------------------------------------------------------
message(STATUS "Boost.GIL: Configuring self-contained header tests for all headers")
add_custom_target(test_headers_self_contained)

file(READ ${CMAKE_CURRENT_LIST_DIR}/main.cpp _main_content)

foreach(_header ${_headers})
  string(REPLACE ".hpp" "" _target ${_header})
  string(REPLACE "/" "-" _target ${_target})
  set(_cpp ${CMAKE_BINARY_DIR}/test/headers/${_target}.cpp)
  set(_target test_header_${_target})

  string(REPLACE "BOOST_GIL_TEST_HEADER" "${_header}" _content "${_main_content}")
  file(WRITE ${_cpp} "${_content}")
  unset(_content)

  add_executable(${_target})

  target_sources(${_target}
    PRIVATE
      ${_cpp}
      ${CMAKE_SOURCE_DIR}/include/boost/gil/${_header})
  unset(_cpp)

  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)

  add_dependencies(test_headers_self_contained ${_target})

  unset(_target)
endforeach()

#-----------------------------------------------------------------------------
# Target: test_headers_all_in_one
# Verifies compilation of all headers included in one translation unit.
# An extra advantage is that such translation unit can be analysed with clang-tidy, etc.
#-----------------------------------------------------------------------------
message(STATUS "Boost.GIL: Configuring all-in-one headers test for all headers")

set(_cpp ${CMAKE_BINARY_DIR}/test/headers/test_headers_all_in_one.cpp)
file(WRITE ${_cpp} "// All headers included in one translation unit\n")
foreach(_header ${_headers})
  file(APPEND ${_cpp} "#include <boost/gil/${_header}>\n")
endforeach()
unset(_headers)
file(APPEND ${_cpp} "int main() { return 0; }\n")

add_executable(test_headers_all_in_one)

target_sources(test_headers_all_in_one PRIVATE ${_cpp})
unset(_cpp)

target_link_libraries(test_headers_all_in_one
  PRIVATE
    gil_compile_options
    gil_include_directories
    gil_dependencies)
