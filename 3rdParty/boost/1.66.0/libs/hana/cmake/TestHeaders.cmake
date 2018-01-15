# Copyright Louis Dionne 2013-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module provides a function generating unit tests to make sure
# that every public header can be included on its own.
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

# Generates header-inclusion unit tests for all the specified headers.
#
# For each specified header with path `xxx/yyy/zzz.hpp`, a target named
# `test.header.xxx.yyy.zzz` is created. This target builds the unit test
# including `xxx/yyy/zzz.hpp`.
#
# Parameters
# ----------
# HEADERS headers:
#   A list of header files to generate the inclusion tests for. All headers
#   in this list must be represented as relative paths from the root of the
#   include directory added to the compiler's header search path. In other
#   words, it should be possible to include all headers in this list as
#
#       #include <${header}>
#
#   For example, for a library with the following structure:
#
#       project/
#           doc/
#           test/
#           ...
#           include/
#               boost/
#                   hana.hpp
#                   hana/
#                       transform.hpp
#                       tuple.hpp
#                       pair.hpp
#                       ...
#
#   When building the unit tests for that library, we'll add `-I project/include'
#   to the compiler's arguments. The list of public headers should therefore contain
#
#       boost/hana.hpp
#       boost/hana/transform.hpp
#       boost/hana/tuple.hpp
#       boost/hana/pair.hpp
#       ...
#
#   Usually, all the 'public' header files of a library should be tested for
#   standalone inclusion. A header is considered 'public' if a client should
#   be able to include that header on its own.
#
# [EXCLUDE excludes]:
#   An optional list of headers or regexes for which no unit test should be
#   generated. Basically, any header in the list specified by the `HEADERS`
#   argument that matches anything in `EXCLUDE` will be skipped.
#
# [MASTER_TARGET target]:
#   An optional target name that will be made a dependent of all the generated
#   targets. This can be used to create a target that will build all the
#   header-inclusion tests.
#
# [LINK_LIBRARIES libraries]:
#   An optional list of libraries that should be linked into each generated
#   executable. The libraries are linked into the target using the usual
#   `target_link_libraries`.
#
# [EXCLUDE_FROM_ALL]:
#   If provided, the generated targets are excluded from the 'all' target.
#
function(generate_standalone_header_tests)
    cmake_parse_arguments(ARGS "EXCLUDE_FROM_ALL"               # options
                               "MASTER_TARGET"                  # 1 value args
                               "HEADERS;EXCLUDE;LINK_LIBRARIES" # multivalued args
                               ${ARGN})

    if (NOT ARGS_HEADERS)
        message(FATAL_ERROR "The `HEADERS` argument must be provided.")
    endif()

    if (ARGS_EXCLUDE_FROM_ALL)
        set(ARGS_EXCLUDE_FROM_ALL "EXCLUDE_FROM_ALL")
    else()
        set(ARGS_EXCLUDE_FROM_ALL "")
    endif()

    foreach(header ${ARGS_HEADERS})
        set(skip FALSE)
        foreach(exclude ${ARGS_EXCLUDE})
            if (${header} MATCHES ${exclude})
                set(skip TRUE)
                break()
            endif()
        endforeach()
        if (skip)
            continue()
        endif()

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
            ${ARGS_EXCLUDE_FROM_ALL}
            "${CMAKE_CURRENT_BINARY_DIR}/headers/${directory}/${filename}.cpp"
        )
        if (ARGS_LINK_LIBRARIES)
            target_link_libraries(test.header.${target} ${ARGS_LINK_LIBRARIES})
        endif()

        if (ARGS_MASTER_TARGET)
            add_dependencies(${ARGS_MASTER_TARGET} test.header.${target})
        endif()
    endforeach()
endfunction()
