#
# Copyright (c) 2017-2019 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring core tests")

foreach(_name
  promote_integral)
  set(_target utility_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp)
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(test.utility.${_name} ${_target})

  unset(_target)
  unset(_name)
endforeach()

if(GIL_BUILD_HEADERS_TESTS)
  add_subdirectory(headers)
endif()

add_subdirectory(point)
add_subdirectory(channel)
add_subdirectory(color)
add_subdirectory(pixel)
add_subdirectory(iterator)
add_subdirectory(locator)
add_subdirectory(image)
add_subdirectory(image_view)
add_subdirectory(algorithm)
add_subdirectory(legacy)
