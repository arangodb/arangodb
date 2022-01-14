#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring tests in test/extension/numeric")

foreach(_name
  channel_numeric_operations
  convolve
  convolve_2d
  convolve_cols
  convolve_rows
  extend_boundary
  kernel
  kernel_fixed
  matrix3x2
  pixel_numeric_operations
  pixel_numeric_operations_float
  resample)
  set(_test t_ext_numeric_${_name})
  set(_target test_ext_numeric_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp)
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
