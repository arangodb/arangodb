# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
#
# This CMake module finds the Hana library. It sets the following CMake
# variables:
#
# Hana_FOUND
#   Set to 1 when Hana is found, set to 0 otherwise.
#
# Hana_INCLUDE_DIRS
#   If Hana is found, this is a list containing Hana's include/ directory,
#   along with that of any dependency of Hana. Since Hana has no external
#   dependencies as a library, this will always contain only Hana's include/
#   directory. This is provided for consistency with normal CMake Find-modules.
#   If Hana is not found, this is not set.
#
#
# The following variables can be used to customize the behavior of the module:
#
# Hana_ROOT
#   The path to Hana's installation prefix. If this is specified, this prefix
#   will be searched first while looking for Hana. If Hana is not found in
#   this prefix, the usual system directories are searched.
#
# Hana_INSTALL_PREFIX
#   If Hana can't be found and this is set to something, Hana will be downloaded
#   and installed to that directory. This can be very useful for installing
#   Hana as a local dependency of the current project. A suggestion is to set
#   `Hana_INSTALL_PREFIX` to `project_root/ext/hana`, and to Git-ignore the
#   whole `ext/` subdirectory, to avoid tracking external dependencies in
#   source control. Note that when `Hana_INSTALL_PREFIX` is specified, the
#   version of Hana must be provided explicitly to `find_package`.
#
# Hana_ENABLE_TESTS
#   If Hana is installed as an external project and this is set to true, the
#   tests will be run whenever Hana is updated. By default, this is set to
#   false because the tests tend to be quite long.


##############################################################################
# Sanitize input variables
##############################################################################
if (DEFINED Hana_FIND_VERSION AND "${Hana_FIND_VERSION}" VERSION_LESS "0.6.0")
    message(FATAL_ERROR
        "This FindHana.cmake module may not be used with Hana < 0.6.0, "
        "because Hana did not carry version information before that.")
endif()

if (DEFINED Hana_INSTALL_PREFIX AND NOT DEFINED Hana_FIND_VERSION)
    message(FATAL_ERROR
        "The version of Hana must be specified when cloning locally. "
        "Otherwise, we wouldn't know which version to clone.")
endif()


##############################################################################
# First, try to find the library locally (using Hana_ROOT as a hint, if provided)
##############################################################################
find_path(Hana_INCLUDE_DIR
    NAMES boost/hana.hpp
    HINTS "${Hana_ROOT}/include"
    DOC "Include directory for the Hana library"
)

# Extract Hana's version from <boost/hana/version.hpp>, and set
# Hana_VERSION_XXX variables accordingly.
if (EXISTS "${Hana_INCLUDE_DIR}")
    foreach(level MAJOR MINOR PATCH)
        file(STRINGS
            ${Hana_INCLUDE_DIR}/boost/hana/version.hpp
            Hana_VERSION_${level}
            REGEX "#define BOOST_HANA_${level}_VERSION"
        )
        string(REGEX MATCH "([0-9]+)" Hana_VERSION_${level} "${Hana_VERSION_${level}}")
    endforeach()

    set(Hana_VERSION_STRING "${Hana_VERSION_MAJOR}.${Hana_VERSION_MINOR}.${Hana_VERSION_PATCH}")
endif()

# Temporarily override the quietness level to avoid producing a "not-found"
# message when we're going to install anyway.
if (Hana_INSTALL_PREFIX)
    set(_hana_quiet_level ${Hana_FIND_QUIETLY})
    set(Hana_FIND_QUIETLY 1)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hana
    REQUIRED_VARS Hana_INCLUDE_DIR
    VERSION_VAR Hana_VERSION_STRING
)
if (Hana_INSTALL_PREFIX)
    set(Hana_FIND_QUIETLY ${_hana_quiet_level})
endif()

if (Hana_FOUND)
    set(Hana_INCLUDE_DIRS "${Hana_INCLUDE_DIR}")
    return()
endif()


##############################################################################
# Otherwise, if Hana_INSTALL_PREFIX was specified, try to install the library.
##############################################################################
if (NOT Hana_FOUND AND DEFINED Hana_INSTALL_PREFIX)
    if (DEFINED Hana_ENABLE_TESTS)
        set(_hana_test_cmd ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/hana --target check)
    endif()

    include(ExternalProject)
    ExternalProject_Add(Hana EXCLUDE_FROM_ALL 1
        SOURCE_DIR      ${Hana_INSTALL_PREFIX}
        PREFIX          ${CMAKE_CURRENT_BINARY_DIR}/hana
        BINARY_DIR      ${CMAKE_CURRENT_BINARY_DIR}/hana
        DOWNLOAD_DIR    ${CMAKE_CURRENT_BINARY_DIR}/hana/_download
        STAMP_DIR       ${CMAKE_CURRENT_BINARY_DIR}/hana/_stamps
        TMP_DIR         ${CMAKE_CURRENT_BINARY_DIR}/hana/_tmp

        # Download step
        DOWNLOAD_NAME "hana-v${Hana_FIND_VERSION}.tar.gz"
        URL "https://github.com/boostorg/hana/tarball/v${Hana_FIND_VERSION}"
        TIMEOUT 20

        # Configure step
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}

        # No build step because Hana is header-only
        BUILD_COMMAND ""

        # No install step
        INSTALL_COMMAND ""

        # Test step
        TEST_COMMAND ${_hana_test_cmd}
    )

    if ("${Hana_FIND_VERSION}" MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(Hana_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(Hana_VERSION_MINOR ${CMAKE_MATCH_2})
        set(Hana_VERSION_PATCH ${CMAKE_MATCH_3})
        set(Hana_VERSION_STRING "${Hana_VERSION_MAJOR}.${Hana_VERSION_MINOR}.${Hana_VERSION_PATCH}")
    endif()

    set(Hana_INCLUDE_DIR "${Hana_INSTALL_PREFIX}/include")
    find_package_handle_standard_args(Hana
        REQUIRED_VARS Hana_INCLUDE_DIR
        VERSION_VAR Hana_VERSION_STRING
    )

    if (Hana_FOUND)
        set(Hana_INCLUDE_DIRS "${Hana_INCLUDE_DIR}")
    endif()
endif()
