#
# Copyright (c) 2018 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
foreach(_name
  collection
  concepts
  derived_view_type
  dynamic_step
  is_planar
  view_is_basic
  view_is_mutable
  view_is_step
  view_type
  view_type_from_pixel)
  set(_target test_core_image_view_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp)
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(test.core.image_view.${_name} ${_target})

  unset(_name)
  unset(_target)
endforeach()
