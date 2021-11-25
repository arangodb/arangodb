# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Ratio is currently experimental at best
#       and this file runs only a subset of the unit tests
#       (in particular none of the fail tests)

# Attach all our tests to the `tests` target, to enable
# `cmake --build . --target tests`
if(NOT TARGET tests)
  add_custom_target(tests)
endif()

## unit tests

# list of tests that contain a main function
set( exec_test_files ratio_ext_pass;ratio_io_pass;ratio_pass )
file( GLOB_RECURSE test_files *_pass.cpp )
foreach( file IN LISTS test_files )

    get_filename_component( core_name ${file} NAME_WE )
    set( test_name boost_ratio-test-${core_name} )

    if( ${core_name} IN_LIST exec_test_files )
        add_executable( ${test_name} EXCLUDE_FROM_ALL ${file} )
        add_test( NAME ${test_name} COMMAND ${test_name} )
    else()
        # most tests are compile only, so we just need to create an object file
        # in order to see, if it compiles
        add_library( ${test_name} STATIC EXCLUDE_FROM_ALL ${file})
    endif()

    add_dependencies(tests ${test_name})

    target_link_libraries( ${test_name} PUBLIC
        Boost::ratio
    )

endforeach()

## examples
file( GLOB_RECURSE test_files ../example/*.cpp )
foreach( file IN LISTS test_files )

    get_filename_component( core_name ${file} NAME_WE )
    set( test_name boost_ratio-example-${core_name} )

    add_executable( ${test_name} EXCLUDE_FROM_ALL ${file} )

    target_link_libraries( ${test_name} PUBLIC
        Boost::ratio Boost::chrono
    )

    add_dependencies(tests ${test_name})

    add_test( NAME ${test_name} COMMAND ${test_name} )

endforeach()
