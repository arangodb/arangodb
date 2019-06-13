#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: configuring Numeric extension tests")

file(GLOB _headers
  ${PROJECT_SOURCE_DIR}/include/boost/gil/extension/numeric/*.hpp)
list(APPEND _headers ${_boost_gil_headers})

add_executable(gil_test_ext_numeric "")
target_sources(gil_test_ext_numeric
  PRIVATE
  ${_headers}
  numeric.cpp)
target_link_libraries(gil_test_ext_numeric
  PRIVATE
  Boost::disable_autolinking
  Boost::unit_test_framework)
add_test(gil.tests.ext.numeric gil_test_ext_numeric)
