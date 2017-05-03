# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

##############################################################################
# Setup custom functions and master targets for the unit tests
##############################################################################
add_custom_target(tests COMMENT "Build all the unit tests.")
add_dependencies(check tests)

add_custom_target(tests.quick COMMENT "Build a subset of all the unit tests to finish faster.")

#   boost_hana_add_unit_test(<name> ...)
#
# Equivalent to `boost_hana_add_test`, except the test is also added as a
# dependency of the `tests` target.
function(boost_hana_add_unit_test name)
    boost_hana_add_test(${ARGV})
    add_dependencies(tests ${name})
    if (NOT "${name}" MATCHES "\\.ext\\.|_mcd|\\.auto\\.")
        add_dependencies(tests.quick ${name})
    endif()
endfunction()


##############################################################################
# Include additional support directories
##############################################################################
include_directories(${CMAKE_CURRENT_LIST_DIR}/_include)


##############################################################################
# Caveats: Take note of public headers and tests that are not supported.
##############################################################################
if (NOT Boost_FOUND)
    list(APPEND EXCLUDED_UNIT_TESTS
        "ext/boost/*.cpp"
        "experimental/printable/*.cpp")

    list(APPEND EXCLUDED_PUBLIC_HEADERS
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/boost/*.hpp"
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/boost.hpp"
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/fwd/ext/boost/*.hpp"
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/fwd/ext/boost.hpp"
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/experimental/printable.hpp")
endif()

# The std::tuple adapter is not supported with Clang < 3.7.0
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND
    "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "3.7.0")

    list(APPEND EXCLUDED_UNIT_TESTS "ext/std/tuple.cpp")

    list(APPEND EXCLUDED_PUBLIC_HEADERS
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/fwd/ext/std/tuple.hpp"
        "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/std/tuple.hpp")
endif()

# On Windows, Clang-cl emulates a MSVC bug that causes EBO not to be applied
# properly. We disable the tests that check for EBO.
if (MSVC AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    list(APPEND EXCLUDED_UNIT_TESTS
        "tuple/empty_member.cpp"
        "issues/github_202.cpp"
    )
endif()


##############################################################################
# Generate tests that include each public header.
# The headers that were excluded above due to caveats are ignored here.
##############################################################################
file(GLOB_RECURSE PUBLIC_HEADERS
    RELATIVE "${Boost.Hana_SOURCE_DIR}/include"
    "${Boost.Hana_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE EXCLUDED_PUBLIC_HEADERS
    RELATIVE "${Boost.Hana_SOURCE_DIR}/include"
    ${EXCLUDED_PUBLIC_HEADERS}
    "${Boost.Hana_SOURCE_DIR}/include/boost/hana/detail/*.hpp"
    "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/boost/fusion/detail/*.hpp"
)

list(REMOVE_ITEM PUBLIC_HEADERS ${EXCLUDED_PUBLIC_HEADERS})
include(TestHeaders)
add_dependencies(tests.quick test.headers)
add_dependencies(tests test.headers)


##############################################################################
# Check for ODR violations when linking several translation units
# (GitHub issue 75)
##############################################################################
list(APPEND EXCLUDED_UNIT_TESTS "issues/github_75/*.cpp")
boost_hana_target_name_for(github_75 "${CMAKE_CURRENT_LIST_DIR}/issues/github_75")
add_executable(${github_75} EXCLUDE_FROM_ALL "issues/github_75/tu1.cpp" "issues/github_75/tu2.cpp")
boost_hana_add_unit_test(${github_75} ${CMAKE_CURRENT_BINARY_DIR}/${github_75})


##############################################################################
# Add all the remaining unit tests
##############################################################################
file(GLOB_RECURSE UNIT_TESTS "*.cpp")
file(GLOB_RECURSE EXCLUDED_UNIT_TESTS ${EXCLUDED_UNIT_TESTS})
list(REMOVE_ITEM UNIT_TESTS ${EXCLUDED_UNIT_TESTS})

foreach(_file IN LISTS UNIT_TESTS)
    boost_hana_target_name_for(_target "${_file}")
    file(STRINGS "${_file}" _subparts REGEX "BOOST_HANA_TEST_PART == ([0-9]+)")

    # If the file contains the special BOOST_HANA_TEST_PART marker, we split
    # it into several subparts. This is done to avoid long compile-times and
    # other inconveniences.
    if (_subparts)
        string(REGEX MATCHALL "BOOST_HANA_TEST_PART == ([0-9]+)" _subparts "${_subparts}")
        string(REGEX REPLACE "BOOST_HANA_TEST_PART == ([0-9]+)" "\\1" _subparts "${_subparts}")
        list(REMOVE_DUPLICATES _subparts)
        add_custom_target(${_target})

        foreach(n IN LISTS _subparts)
            add_executable("${_target}.part${n}" EXCLUDE_FROM_ALL "${_file}")
            set_target_properties("${_target}.part${n}" PROPERTIES
                COMPILE_DEFINITIONS "BOOST_HANA_TEST_PART=${n}")
            add_dependencies(${_target} "${_target}.part${n}")

            boost_hana_add_unit_test("${_target}.part${n}"
                ${CMAKE_CURRENT_BINARY_DIR}/${_target}.part${n})
        endforeach()
    else()
        add_executable(${_target} EXCLUDE_FROM_ALL "${_file}")
        boost_hana_add_unit_test(${_target} ${CMAKE_CURRENT_BINARY_DIR}/${_target})
    endif()
endforeach()
