#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring tests in test/extension/toolbox")

foreach(_name
  channel_type
  channel_view
  color_convert_cmyka
  color_convert_gray
  color_convert_gray_alpha
  color_convert_hsl
  color_convert_hsv
  color_convert_lab
  color_convert_luminance
  color_convert_xyz
  get_num_bits
  get_pixel_type
  is_bit_aligned
  is_homogeneous
  pixel_bit_size
  subchroma_image)
  set(_test t_ext_toolbox_${_name})
  set(_target test_ext_toolbox_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name})
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(NAME ${_test} COMMAND ${_target})

  unset(_name)
  unset(_target)
  unset(_test)
endforeach()

# FIXME: https://github.com/boostorg/gil/issues/235
foreach(_name
  indexed_image)
  set(_test t_ext_toolbox_${_name})
  set(_target test_ext_toolbox_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name})
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  add_test(NAME ${_test} COMMAND ${_target})

  unset(_name)
  unset(_target)
  unset(_test)
endforeach()
