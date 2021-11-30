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
message(STATUS "Boost.GIL: Configuring tests in test/legacy")

foreach(_name
  channel
  pixel
  pixel_iterator)
  set(_test t_legacy_${_name})
  set(_target test_legacy_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE ${_name}.cpp error_if.cpp)
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

# Add extra source files accompanying image.cpp
foreach(_name
  image)
  set(_test t_legacy_${_name})
  set(_target test_legacy_${_name})

  add_executable(${_target} "")
  target_sources(${_target} PRIVATE
    ${_name}.cpp
    error_if.cpp
    sample_image.cpp)

  target_link_libraries(${_target}
    PRIVATE
      gil_compile_options
      gil_include_directories
      gil_dependencies)
  target_compile_definitions(${_target} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK)
  add_test(NAME ${_test} COMMAND ${_target}
    ${CMAKE_CURRENT_SOURCE_DIR}/gil_reference_checksums.txt)

  unset(_name)
  unset(_target)
  unset(_test)
endforeach()
