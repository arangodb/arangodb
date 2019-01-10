#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: configuring core tests")

foreach(name channel pixel pixel_iterator promote_integral)
  add_executable(gil_test_${name} "")
  target_sources(gil_test_${name}
    PRIVATE
      ${_boost_gil_headers}
      ${_boost_gil_test_headers}
      ${name}.cpp
      error_if.cpp)
  target_link_libraries(gil_test_${name}
    PRIVATE
      Boost::disable_autolinking
      Boost::filesystem
      Boost::unit_test_framework)
  add_test(gil.tests.core.${name} gil_test_${name})
endforeach()

add_executable(gil_test_image "")
target_sources(gil_test_image
  PRIVATE
    ${_boost_gil_headers}
    ${_boost_gil_test_headers}
    error_if.cpp
    image.cpp
    sample_image.cpp)
target_link_libraries(gil_test_image
  PRIVATE
    Boost::disable_autolinking
    Boost::filesystem
    Boost::unit_test_framework)
add_test(gil.tests.core.image
  gil_test_image
  ${CMAKE_CURRENT_SOURCE_DIR}/gil_reference_checksums.txt)

add_subdirectory(algorithm)
add_subdirectory(channel)
add_subdirectory(image_view)
add_subdirectory(point)
