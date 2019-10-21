# Ceres Solver - A fast non-linear least squares minimizer
# Copyright 2013 Google Inc. All rights reserved.
# http://code.google.com/p/ceres-solver/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Author: alexs.mac@gmail.com (Alex Stewart)
#

# FindEigen.cmake - Find Eigen library, version >= 3.
#
# This module defines the following variables:
#
# EIGEN_FOUND: TRUE iff Eigen is found.
# EIGEN_INCLUDE_DIRS: Include directories for Eigen.
#
# EIGEN_VERSION: Extracted from Eigen/src/Core/util/Macros.h
# EIGEN_WORLD_VERSION: Equal to 3 if EIGEN_VERSION = 3.2.0
# EIGEN_MAJOR_VERSION: Equal to 2 if EIGEN_VERSION = 3.2.0
# EIGEN_MINOR_VERSION: Equal to 0 if EIGEN_VERSION = 3.2.0
#
# The following variables control the behaviour of this module:
#
# EIGEN_INCLUDE_DIR_HINTS: List of additional directories in which to
#                          search for eigen includes, e.g: /timbuktu/eigen3.
#
# The following variables are also defined by this module, but in line with
# CMake recommended FindPackage() module style should NOT be referenced directly
# by callers (use the plural variables detailed above instead).  These variables
# do however affect the behaviour of the module via FIND_[PATH/LIBRARY]() which
# are NOT re-called (i.e. search for library is not repeated) if these variables
# are set with valid values _in the CMake cache_. This means that if these
# variables are set directly in the cache, either by the user in the CMake GUI,
# or by the user passing -DVAR=VALUE directives to CMake when called (which
# explicitly defines a cache variable), then they will be used verbatim,
# bypassing the HINTS variables and other hard-coded search locations.
#
# EIGEN_INCLUDE_DIR: Include directory for CXSparse, not including the
#                    include directory of any dependencies.

# Called if we failed to find Eigen or any of it's required dependencies,
# unsets all public (designed to be used externally) variables and reports
# error message at priority depending upon [REQUIRED/QUIET/<NONE>] argument.
MACRO(EIGEN_REPORT_NOT_FOUND REASON_MSG)
  UNSET(EIGEN_FOUND)
  UNSET(EIGEN_INCLUDE_DIRS)
  # Make results of search visible in the CMake GUI if Eigen has not
  # been found so that user does not have to toggle to advanced view.
  MARK_AS_ADVANCED(CLEAR EIGEN_INCLUDE_DIR)
  # Note <package>_FIND_[REQUIRED/QUIETLY] variables defined by FindPackage()
  # use the camelcase library name, not uppercase.
  IF (Eigen_FIND_QUIETLY)
    MESSAGE(STATUS "Failed to find Eigen - " ${REASON_MSG} ${ARGN})
  ELSEIF (Eigen_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Failed to find Eigen - " ${REASON_MSG} ${ARGN})
  ELSE()
    # Neither QUIETLY nor REQUIRED, use no priority which emits a message
    # but continues configuration and allows generation.
    MESSAGE("-- Failed to find Eigen - " ${REASON_MSG} ${ARGN})
  ENDIF ()
ENDMACRO(EIGEN_REPORT_NOT_FOUND)

# Search user-installed locations first, so that we prefer user installs
# to system installs where both exist.
#
# TODO: Add standard Windows search locations for Eigen.
LIST(APPEND EIGEN_CHECK_INCLUDE_DIRS
  /usr/local/include/eigen3
  /usr/local/homebrew/include/eigen3 # Mac OS X
  /opt/local/var/macports/software/eigen3 # Mac OS X.
  /opt/local/include/eigen3
  /usr/include/eigen3)

# Search supplied hint directories first if supplied.
FIND_PATH(EIGEN_INCLUDE_DIR
  NAMES Eigen/Core
  PATHS ${EIGEN_INCLUDE_DIR_HINTS}
  ${EIGEN_CHECK_INCLUDE_DIRS})
IF (NOT EIGEN_INCLUDE_DIR OR
    NOT EXISTS ${EIGEN_INCLUDE_DIR})
  EIGEN_REPORT_NOT_FOUND(
    "Could not find eigen3 include directory, set EIGEN_INCLUDE_DIR to "
    "path to eigen3 include directory, e.g. /usr/local/include/eigen3.")
ENDIF (NOT EIGEN_INCLUDE_DIR OR
       NOT EXISTS ${EIGEN_INCLUDE_DIR})

# Mark internally as found, then verify. EIGEN_REPORT_NOT_FOUND() unsets
# if called.
SET(EIGEN_FOUND TRUE)

# Extract Eigen version from Eigen/src/Core/util/Macros.h
IF (EIGEN_INCLUDE_DIR)
  SET(EIGEN_VERSION_FILE ${EIGEN_INCLUDE_DIR}/Eigen/src/Core/util/Macros.h)
  IF (NOT EXISTS ${EIGEN_VERSION_FILE})
    EIGEN_REPORT_NOT_FOUND(
      "Could not find file: ${EIGEN_VERSION_FILE} "
      "containing version information in Eigen install located at: "
      "${EIGEN_INCLUDE_DIR}.")
  ELSE (NOT EXISTS ${EIGEN_VERSION_FILE})
    FILE(READ ${EIGEN_VERSION_FILE} EIGEN_VERSION_FILE_CONTENTS)

    STRING(REGEX MATCH "#define EIGEN_WORLD_VERSION [0-9]+"
      EIGEN_WORLD_VERSION "${EIGEN_VERSION_FILE_CONTENTS}")
    STRING(REGEX REPLACE "#define EIGEN_WORLD_VERSION ([0-9]+)" "\\1"
      EIGEN_WORLD_VERSION "${EIGEN_WORLD_VERSION}")

    STRING(REGEX MATCH "#define EIGEN_MAJOR_VERSION [0-9]+"
      EIGEN_MAJOR_VERSION "${EIGEN_VERSION_FILE_CONTENTS}")
    STRING(REGEX REPLACE "#define EIGEN_MAJOR_VERSION ([0-9]+)" "\\1"
      EIGEN_MAJOR_VERSION "${EIGEN_MAJOR_VERSION}")

    STRING(REGEX MATCH "#define EIGEN_MINOR_VERSION [0-9]+"
      EIGEN_MINOR_VERSION "${EIGEN_VERSION_FILE_CONTENTS}")
    STRING(REGEX REPLACE "#define EIGEN_MINOR_VERSION ([0-9]+)" "\\1"
      EIGEN_MINOR_VERSION "${EIGEN_MINOR_VERSION}")

    # This is on a single line s/t CMake does not interpret it as a list of
    # elements and insert ';' separators which would result in 3.;2.;0 nonsense.
    SET(EIGEN_VERSION "${EIGEN_WORLD_VERSION}.${EIGEN_MAJOR_VERSION}.${EIGEN_MINOR_VERSION}")
  ENDIF (NOT EXISTS ${EIGEN_VERSION_FILE})
ENDIF (EIGEN_INCLUDE_DIR)

# Set standard CMake FindPackage variables if found.
IF (EIGEN_FOUND)
  SET(EIGEN_INCLUDE_DIRS ${EIGEN_INCLUDE_DIR})
ENDIF (EIGEN_FOUND)

# Handle REQUIRED / QUIET optional arguments and version.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Eigen
  REQUIRED_VARS EIGEN_INCLUDE_DIRS
  VERSION_VAR EIGEN_VERSION)

# Only mark internal variables as advanced if we found Eigen, otherwise
# leave it visible in the standard GUI for the user to set manually.
IF (EIGEN_FOUND)
  MARK_AS_ADVANCED(FORCE EIGEN_INCLUDE_DIR)
ENDIF (EIGEN_FOUND)
