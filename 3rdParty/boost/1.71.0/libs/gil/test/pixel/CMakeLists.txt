#
# Copyright (c) 2019 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
foreach(_name
  concepts
  bit_aligned_pixel_reference
  is_pixel
  is_planar
  num_channels
  packed_pixel
  pixel_reference_is_mutable
  pixels_are_compatible
  test_fixture)
  set(_target test_core_pixel_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp)
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(test.core.pixel.${_name} ${_target})

  unset(_name)
  unset(_target)
endforeach()
