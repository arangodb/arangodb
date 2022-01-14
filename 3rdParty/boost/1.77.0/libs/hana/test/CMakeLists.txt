# Copyright Louis Dionne 2013-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

add_custom_target(tests COMMENT "Build all the unit tests.")
add_dependencies(hana_check tests)


##############################################################################
# Take note of files that depend on Boost
##############################################################################
file(GLOB_RECURSE TESTS_REQUIRING_BOOST "ext/boost/*.cpp"
                                        "experimental/printable/*.cpp")

file(GLOB_RECURSE PUBLIC_HEADERS_REQUIRING_BOOST
    RELATIVE "${Boost.Hana_SOURCE_DIR}/include"
    "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/boost/*.hpp"
    "${Boost.Hana_SOURCE_DIR}/include/boost/hana/ext/boost.hpp"
    "${Boost.Hana_SOURCE_DIR}/include/boost/hana/experimental/printable.hpp"
)


##############################################################################
# Caveats: Take note of public headers and tests that are not supported.
##############################################################################
if (NOT Boost_FOUND)
    list(APPEND EXCLUDED_UNIT_TESTS ${TESTS_REQUIRING_BOOST})
    list(APPEND EXCLUDED_PUBLIC_HEADERS ${PUBLIC_HEADERS_REQUIRING_BOOST})
endif()

# The experimental::type_name test is only supported on Clang and AppleClang >= 7.0
if (NOT (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"
        OR (${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" AND
            NOT ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 7)))
    list(APPEND EXCLUDED_PUBLIC_HEADERS
        "boost/hana/experimental/type_name.hpp")
    list(APPEND EXCLUDED_UNIT_TESTS "experimental/type_name.cpp")
endif()

# On Windows, Clang-cl emulates a MSVC bug that causes EBO not to be applied
# properly. We disable the tests that check for EBO.
if (MSVC AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    list(APPEND EXCLUDED_UNIT_TESTS
        "detail/ebo.cpp"
        "issues/github_202.cpp"
        "pair/empty_storage.cpp"
        "tuple/empty_member.cpp"
    )
endif()


##############################################################################
# Generate tests that include each public header.
# The headers that were excluded above due to caveats are ignored here.
##############################################################################
add_custom_target(test.headers COMMENT "Build all the header-inclusion unit tests.")
add_dependencies(tests test.headers)

file(GLOB_RECURSE PUBLIC_HEADERS
    RELATIVE "${Boost.Hana_SOURCE_DIR}/include"
    "${Boost.Hana_SOURCE_DIR}/include/*.hpp"
)
list(REMOVE_ITEM PUBLIC_HEADERS ${PUBLIC_HEADERS_REQUIRING_BOOST})

include(TestHeaders)
add_header_test(test.headers.standalone EXCLUDE_FROM_ALL
                                        HEADERS ${PUBLIC_HEADERS}
                                        EXCLUDE ${EXCLUDED_PUBLIC_HEADERS})
target_link_libraries(test.headers.standalone PRIVATE hana)
add_dependencies(test.headers test.headers.standalone)

if (Boost_FOUND)
    add_header_test(test.headers.boost EXCLUDE_FROM_ALL
                                       HEADERS ${PUBLIC_HEADERS_REQUIRING_BOOST}
                                       EXCLUDE ${EXCLUDED_PUBLIC_HEADERS})
    target_link_libraries(test.headers.boost PRIVATE hana Boost::boost)
    add_dependencies(test.headers test.headers.boost)
endif()


##############################################################################
# Check for ODR violations when linking several translation units
# (GitHub issue 75)
##############################################################################
list(APPEND EXCLUDED_UNIT_TESTS "issues/github_75/*.cpp")
boost_hana_target_name_for(github_75 "${CMAKE_CURRENT_LIST_DIR}/issues/github_75")
add_executable(${github_75} EXCLUDE_FROM_ALL "issues/github_75/tu1.cpp" "issues/github_75/tu2.cpp")
boost_hana_set_test_properties(${github_75})
add_test(${github_75} "${CMAKE_CURRENT_BINARY_DIR}/${github_75}")
add_dependencies(tests ${github_75})


##############################################################################
# Add all the remaining unit tests
##############################################################################
file(GLOB_RECURSE UNIT_TESTS "*.cpp")
file(GLOB_RECURSE EXCLUDED_UNIT_TESTS ${EXCLUDED_UNIT_TESTS})
list(REMOVE_ITEM UNIT_TESTS ${EXCLUDED_UNIT_TESTS})

foreach(_file IN LISTS UNIT_TESTS)
    boost_hana_target_name_for(_target "${_file}")
    add_executable(${_target} EXCLUDE_FROM_ALL "${_file}")
    boost_hana_set_test_properties(${_target})
    if (_file IN_LIST TESTS_REQUIRING_BOOST)
        target_link_libraries(${_target} PRIVATE Boost::boost)
    endif()
    target_include_directories(${_target} PRIVATE _include)
    add_test(${_target} "${CMAKE_CURRENT_BINARY_DIR}/${_target}")
    add_dependencies(tests ${_target})
endforeach()


##############################################################################
# Add the deployment test, which checks that we can indeed install `hana` and
# then use the provided `HanaConfig.cmake` config file to use `hana` from an
# external project.
##############################################################################
include(ExternalProject)
set(HANA_FAKE_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/deploy/fakeroot")
ExternalProject_Add(test.deploy.fakeroot
  SOURCE_DIR "${PROJECT_SOURCE_DIR}"
  EXCLUDE_FROM_ALL TRUE
  BUILD_ALWAYS TRUE
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${HANA_FAKE_INSTALL_DIR}
             -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
             -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
  TEST_COMMAND ""      # Disable test step
  UPDATE_COMMAND ""    # Disable source work-tree update
)

add_custom_target(test.deploy
  DEPENDS test.deploy.fakeroot
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/deploy/build"
  COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}/deploy/build"
            ${CMAKE_COMMAND} "${CMAKE_CURRENT_SOURCE_DIR}/deploy"
                             -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
                             -DCMAKE_PREFIX_PATH="${HANA_FAKE_INSTALL_DIR}"
                             -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
                             -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}"
  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}/deploy/build"
  USES_TERMINAL
)
add_dependencies(hana_check test.deploy)
