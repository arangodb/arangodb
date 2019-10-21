#
# Copyright (c) 2019 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# *** IMPORTANT MAINTENANCE RULES ***
# These are GIL's original, comprehensive, all-in-one test suites.
#   * Keep as reference.
#   * Do NOT extend.
#   * Do NOT refactor.
#   * Modify only if absolutely necessary (a bug found in the tests).
# See the accompanying README.md
#
message(STATUS "Boost.GIL: Configuring core tests")

foreach(_name
  channel
  image
  pixel
  pixel_iterator)
  set(_target legacy_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp error_if.cpp)
  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(test.legacy.${_name} ${_target})

  unset(_target)
  unset(_name)
endforeach()

# Add extra source files accompanying image.cpp
target_sources(legacy_image
  PRIVATE
    error_if.cpp
    sample_image.cpp)
add_test(test.legacy.image
  legacy_image
  ${CMAKE_CURRENT_SOURCE_DIR}/gil_reference_checksums.txt)
