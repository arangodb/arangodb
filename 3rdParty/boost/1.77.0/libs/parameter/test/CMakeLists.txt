# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Parameter is currently experimental at best
#       and the interface is likely to change in the future

# TODO: Also process literate tests
file(GLOB test_files *.cpp)

# remove some test for which the dependencies are not yet available or have
# special requirements
# TODO: enable more tests
list(FILTER test_files EXCLUDE REGEX
    efficiency|deduced_unmatched_arg|python_test|duplicates)

# Attach all our tests to the `tests` target, to enable
# `cmake --build . --target tests`
if(NOT TARGET tests)
  add_custom_target(tests)
endif()

foreach(file IN LISTS test_files)

    get_filename_component(core_name ${file} NAME_WE)
    set(test_name boost_parameter-test-${core_name})

    add_executable(${test_name} EXCLUDE_FROM_ALL ${file})
    add_dependencies(tests ${test_name})

    # add Boost.Parameter and any libraries that are only needed by the tests
    # (none at the moment)
    target_link_libraries(${test_name} Boost::parameter)

    add_test(NAME ${test_name} COMMAND ${test_name})

endforeach()
