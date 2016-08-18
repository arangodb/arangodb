# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# This file was adapted from FindMeta.cmake at https://github.com/ericniebler/meta.
#
#
# This CMake module finds the Meta include directory. This module sets the
# following CMake variables:
#
# Meta_FOUND
#   Set to 1 when the Meta include directory is found, 0 otherwise.
#
# Meta_INCLUDE_DIR
#   If the Meta include directory is found, this is set to the path of that
#   directory. Otherwise, this is not set.
#
#
# The following variables can be used to customize the behavior of the module:
#
# Meta_INCLUDE_DIR
#   The path to the Meta include directory. When set, this will be used as-is.
#
# Meta_CLONE_IF_MISSING
#   If the Meta include directory can't be found and this is set to true,
#   the Meta project will be cloned locally.

if (NOT EXISTS "${Meta_INCLUDE_DIR}")
    find_path(Meta_INCLUDE_DIR NAMES meta/meta.hpp
                               DOC "Meta library header files")
endif()

if (NOT EXISTS "${Meta_INCLUDE_DIR}" AND Meta_CLONE_IF_MISSING)
    include(ExternalProject)
    ExternalProject_Add(meta EXCLUDE_FROM_ALL 1
        GIT_REPOSITORY https://github.com/ericniebler/meta
        TIMEOUT 5
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
        BUILD_COMMAND ""   # Disable build step
        INSTALL_COMMAND "" # Disable install step
        TEST_COMMAND ""    # Disable test step
    )

    ExternalProject_Get_Property(meta SOURCE_DIR)
    set(Meta_INCLUDE_DIR "${SOURCE_DIR}/include")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Meta
    FOUND_VAR Meta_FOUND
    REQUIRED_VARS Meta_INCLUDE_DIR)
