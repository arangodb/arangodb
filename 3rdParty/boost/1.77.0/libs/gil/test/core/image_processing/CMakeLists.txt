# Boost.GIL (Generic Image Library) - tests
#
# Copyright 2019 Miral Shah <miralshah2211@gmail.com>
#
# Use, modification and distribution are subject to the Boost Software License,
# Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
foreach(_name
    threshold_binary
    threshold_truncate
    threshold_otsu)
    set(_test t_core_image_processing_${_name})
    set(_target test_core_image_processing_${_name})

    add_executable(${_target} "")
    target_sources(${_target} PRIVATE ${_name}.cpp)
    target_link_libraries(${_target}
            PRIVATE
            gil_compile_options
            gil_include_directories
            gil_dependencies)
    target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
    add_test(NAME ${_test} COMMAND ${_target})

    unset(_name)
    unset(_target)
endforeach()

foreach(_name
  lanczos_scaling
  simple_kernels
  harris
  hessian
  box_filter
  median_filter
  sobel_scharr)
  set(_test t_core_image_processing_${_name})
  set(_target test_core_image_processing_${_name})

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
