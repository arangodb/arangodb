# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

# Setup custom functions, master targets and file lists for the unit tests
add_custom_target(experimental COMMENT "Build all the experimental code.")
add_dependencies(check experimental)

# Add all the experimental targets
file(GLOB_RECURSE BOOST_HANA_EXPERIMENTAL_SOURCES "*.cpp")

# GCC fails to link strong_datatypes.cpp due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70095
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    list(APPEND EXCLUDED_EXPERIMENTAL "strong_datatypes.cpp")
endif()

file(GLOB_RECURSE EXCLUDED_EXPERIMENTAL ${EXCLUDED_EXPERIMENTAL})
list(REMOVE_ITEM BOOST_HANA_EXPERIMENTAL_SOURCES "" ${EXCLUDED_EXPERIMENTAL})

foreach(file IN LISTS BOOST_HANA_EXPERIMENTAL_SOURCES)
    boost_hana_target_name_for(target "${file}")
    add_executable(${target} EXCLUDE_FROM_ALL ${file})
    boost_hana_add_test(${target} ${CMAKE_CURRENT_BINARY_DIR}/${target})
    add_dependencies(experimental ${target})
endforeach()
