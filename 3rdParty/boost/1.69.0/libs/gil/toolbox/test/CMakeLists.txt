#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: configuring Toolbox extension tests")

file(GLOB_RECURSE _headers
  ${PROJECT_SOURCE_DIR}/include/boost/gil/extension/toolbox/*.hpp)
list(APPEND _headers ${_boost_gil_headers})

add_executable(gil_test_ext_toolbox "")
target_sources(gil_test_ext_toolbox
  PRIVATE
  ${_headers}
  test.cpp
  channel_type.cpp
  channel_view.cpp
  cmyka.cpp
  get_num_bits.cpp
  get_pixel_type.cpp
  gray_alpha.cpp
  gray_to_rgba.cpp
  hsl_hsv_test.cpp
  indexed_image_test.cpp
  is_bit_aligned.cpp
  is_homogeneous.cpp
  is_similar.cpp
  lab_test.cpp
  pixel_bit_size.cpp
  rgb_to_luminance.cpp
  xyz_test.cpp)
target_link_libraries(gil_test_ext_toolbox
  PRIVATE
  Boost::disable_autolinking
  Boost::unit_test_framework)
add_test(gil.tests.ext.toolbox gil_test_ext_toolbox)
