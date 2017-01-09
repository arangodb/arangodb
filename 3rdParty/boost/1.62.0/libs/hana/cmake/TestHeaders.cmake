# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module generates unit tests to make sure that every public header
# can be included on its own.
#
# When a C++ library or application has many header files, it can happen that
# a header does not include all the other headers it depends on. When this is
# the case, it can happen that including that header file on its own will
# break the compilation. This CMake module generates a dummy unit test for
# each header file considered public: this unit test is just a program of
# the form
#
#   #include <the/public/header.hpp>
#   int main() { }
#
# If this succeeds to compile, it means that the header can be included on
# its own, which is what clients expect. Otherwise, you have a problem.
# Since writing these dumb unit tests by hand is tedious and repetitive,
# you can use this CMake module to automate this task.
#
#
# This CMake module uses the following variables to do its job:
#
#   PUBLIC_HEADERS
#       A list of all the header files that are considered to be 'public'.
#       A header is considered 'public' if a client should be able to
#       include that header on its own. The headers in this list should
#       be represented as relative paths from the root of the `include`
#       directory, or whatever directory is added to the header search
#       of the compiler.
#
#       For example, for a library with the following structure:
#
#          project-root/
#              doc/
#              test/
#              ...
#              include/
#                  boost/
#                      hana.hpp
#                      hana/
#                          transform.hpp
#                          tuple.hpp
#                          pair.hpp
#                          ...
#
#       When building the unit tests for that library, we'll add
#       `-I project-root/include' to the compiler's arguments.
#       The list of public headers should therefore contain
#
#           boost/hana.hpp
#           boost/hana/transform.hpp
#           boost/hana/tuple.hpp
#           boost/hana/pair.hpp
#           ...
#
# This CMake module creates the following targets:
#
#   test.headers
#       Builds all the header-inclusion unit tests. This target is excluded
#       from the 'all' target, so it will only run when asked explicitly.
#
#   test.header.<path.to.header>
#       For each `path/to/header`, a target named `test.header.path.to.header`
#       is created. This target builds the unit test including `path/to/header`.
#       These targets are also excluded from the 'all' target. When the
#       `test.headers` target is built, all the `test.header.xxx` targets
#       are built.
#

add_custom_target(test.headers
    COMMENT "Build all the header-inclusion unit tests.")

foreach(header IN LISTS PUBLIC_HEADERS)
    get_filename_component(filename "${header}" NAME_WE)
    get_filename_component(directory "${header}" DIRECTORY)

    if (NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/headers/${directory}/${filename}.cpp")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/headers/${directory}/${filename}.cpp" "
/* THIS FILE WAS AUTOMATICALLY GENERATED: DO NOT EDIT! */
#include <${header}>
int main() { }
        ")
    endif()

    string(REGEX REPLACE "/" "." target "${header}")
    add_executable(test.header.${target}
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_BINARY_DIR}/headers/${directory}/${filename}.cpp"
    )

    add_dependencies(test.headers test.header.${target})
endforeach()
