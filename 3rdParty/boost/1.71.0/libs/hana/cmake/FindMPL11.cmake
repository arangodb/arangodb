# Copyright Louis Dionne 2013-2017
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module finds the MPL11 include directory. This module sets the
# following CMake variables:
#
# MPL11_FOUND
#   Set to 1 when the MPL11 include directory is found, 0 otherwise.
#
# MPL11_INCLUDE_DIR
#   If the MPL11 include directory is found, this is set to the path of that
#   directory. Otherwise, this is not set.
#
#
# The following variables can be used to customize the behavior of the module:
#
# MPL11_INCLUDE_DIR
#   The path to the MPL11 include directory. When set, this will be used as-is.
#
# MPL11_CLONE_IF_MISSING
#   If the MPL11 include directory can't be found and this is set to true,
#   the MPL11 project will be cloned locally.

if (NOT EXISTS "${MPL11_INCLUDE_DIR}")
    find_path(MPL11_INCLUDE_DIR NAMES boost/mpl11/mpl11.hpp
                                DOC "MPL11 library header files")
endif()

if (NOT EXISTS "${MPL11_INCLUDE_DIR}" AND MPL11_CLONE_IF_MISSING)
    include(ExternalProject)
    ExternalProject_Add(MPL11 EXCLUDE_FROM_ALL 1
        GIT_REPOSITORY https://github.com/ldionne/mpl11
        TIMEOUT 5
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
        BUILD_COMMAND ""   # Disable build step
        INSTALL_COMMAND "" # Disable install step
        TEST_COMMAND ""    # Disable test step
    )

    ExternalProject_Get_Property(MPL11 SOURCE_DIR)
    set(MPL11_INCLUDE_DIR "${SOURCE_DIR}/include")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPL11 DEFAULT_MSG MPL11_INCLUDE_DIR)
