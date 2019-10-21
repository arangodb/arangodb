# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Ratio is currently experimental at best
#       and this file runs only a subset of the unit tests
#       (in particular none of the fail tests)


## unit tests

# list of tests that contain a main function
set( exec_test_files ratio_ext_pass;ratio_io_pass;ratio_pass )
file( GLOB_RECURSE test_files *_pass.cpp )
foreach( file IN LISTS test_files )

    get_filename_component( core_name ${file} NAME_WE )
    set( test_name test_boost_ratio_${core_name} )

    if( ${core_name} IN_LIST exec_test_files )
        add_executable( ${test_name} ${file} )
        add_test( NAME ${test_name} COMMAND ${test_name} )
    else()
        # most tests are compile only, so we just need to create an object file
        # in order to see, if it compiles
        add_library( ${test_name} STATIC ${file})
    endif()

    target_link_libraries( ${test_name} PUBLIC
        Boost::ratio
    )

endforeach()

## examples
file( GLOB_RECURSE test_files ../example/*.cpp )
foreach( file IN LISTS test_files )

    get_filename_component( core_name ${file} NAME_WE )
    set( test_name test_boost_ratio_${core_name} )

    add_executable( ${test_name} ${file} )
    target_link_libraries( ${test_name} PUBLIC
        Boost::ratio
    )
    add_test( NAME ${test_name} COMMAND ${test_name} )

endforeach()

