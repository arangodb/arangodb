# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module finds the Valgrind executable. This module sets the
# following CMake variables:
#
# Valgrind_FOUND
#   Set to 1 when Valgrind is found, 0 otherwise.
#
# Valgrind_EXECUTABLE
#   If the Valgrind executable is found, this is set to its full path.
#   Otherwise this is not set.
#
#
# The following variables can be used to give hints about search locations:
#
# Valgrind_EXECUTABLE
#   The path to the Valgrind executable.

if (NOT EXISTS "${Valgrind_EXECUTABLE}")
    find_program(Valgrind_EXECUTABLE NAMES valgrind
        DOC "Full path to the Valgrind executable.")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Valgrind
    FOUND_VAR Valgrind_FOUND
    REQUIRED_VARS Valgrind_EXECUTABLE)
