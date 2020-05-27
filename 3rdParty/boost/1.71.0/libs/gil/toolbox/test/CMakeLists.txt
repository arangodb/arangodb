#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring Toolbox extension tests")

add_executable(test_ext_toolbox "")
target_sources(test_ext_toolbox
  PRIVATE
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
    # TODO: Add subchroma_image.cpp after fixing run-time failure,
    #       for details see https://github.com/boostorg/gil/pull/164
    #subchroma_image.cpp
    xyz_test.cpp)
target_link_libraries(test_ext_toolbox
  PRIVATE
    gil_compile_options
    gil_include_directories
    gil_dependencies)
add_test(test.ext.toolbox test_ext_toolbox)
