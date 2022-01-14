#
# Copyright (c) 2019 Mateusz Loskot <mateusz at loskot dot net>
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#

foreach(_name
  test_utility_output_stream)
  set(_test t_utility_${_name})
  set(_target test_utility_${_name})

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
  unset(_test)
endforeach()

add_subdirectory(core)

add_subdirectory(legacy)

add_subdirectory(extension)

# TODO: Split headers tests into core and extensions, see Jamfile-s
if(BOOST_GIL_BUILD_HEADER_TESTS)
  add_subdirectory(header)
endif()
