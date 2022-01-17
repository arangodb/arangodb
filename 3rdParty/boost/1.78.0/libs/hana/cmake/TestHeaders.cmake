# Copyright Louis Dionne 2013-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module provides a function generating a unit test to make sure
# that every public header can be included on its own.
#
# When a C++ library or application has many header files, it can happen that
# a header does not include all the other headers it depends on. When this is
# the case, it can happen that including that header file on its own will
# break the compilation. This CMake module generates a dummy executable
# comprised of many .cpp files, each of which includes a header file that
# is part of the public API. In other words, the executable is comprised
# of .cpp files of the form:
#
#   #include <the/public/header.hpp>
#
# and then exactly one `main` function. If this succeeds to compile, it means
# that the header can be included on its own, which is what clients expect.
# Otherwise, you have a problem. Since writing these dumb unit tests by hand
# is tedious and repetitive, you can use this CMake module to automate this
# task.

#   add_header_test(<target> [EXCLUDE_FROM_ALL] [EXCLUDE excludes...] HEADERS headers...)
#
# Generates header-inclusion unit tests for all the specified headers.
#
# This function creates a target which builds a dummy executable including
# each specified header file individually. If this target builds successfully,
# it means that all the specified header files can be included individually.
#
# Parameters
# ----------
# <target>:
#   The name of the target to generate.
#
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
# [EXCLUDE_FROM_ALL]:
#   If provided, the generated target is excluded from the 'all' target.
#
function(add_header_test target)
    cmake_parse_arguments(ARGS "EXCLUDE_FROM_ALL"             # options
                               ""                             # 1 value args
                               "HEADERS;EXCLUDE"              # multivalued args
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

        set(source "${CMAKE_CURRENT_BINARY_DIR}/headers/${directory}/${filename}.cpp")
        if (NOT EXISTS "${source}")
            file(WRITE "${source}" "#include <${header}>")
        endif()
        list(APPEND sources "${source}")
    endforeach()

    set(standalone_main "${CMAKE_CURRENT_BINARY_DIR}/headers/_standalone_main.cpp")
    if (NOT EXISTS "${standalone_main}")
        file(WRITE "${standalone_main}" "int main() { }")
    endif()
    add_executable(${target}
        ${ARGS_EXCLUDE_FROM_ALL}
        ${sources}
        "${CMAKE_CURRENT_BINARY_DIR}/headers/_standalone_main.cpp"
    )
endfunction()
