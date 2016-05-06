#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Tries to find Snappy headers and libraries.
#
# Usage of this module as follows:
#
#  find_package(Snappy)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Snappy_HOME - When set, this path is inspected instead of standard library
#                locations as the root of the Snappy installation.
#                The environment variable SNAPPY_HOME overrides this veriable.
#
# This module defines
#  SNAPPY_INCLUDE_DIR, directory containing headers
#  SNAPPY_LIBS, directory containing snappy libraries
#  SNAPPY_STATIC_LIB, path to libsnappy.a
#  SNAPPY_SHARED_LIB, path to libsnappy's shared library
#  SNAPPY_FOUND, whether snappy has been found

if( NOT "$ENV{SNAPPY_HOME}" STREQUAL "")
    file( TO_CMAKE_PATH "$ENV{SNAPPY_HOME}" _native_path )
    list( APPEND _snappy_roots ${_native_path} )
elseif ( Snappy_HOME )
    list( APPEND _snappy_roots ${Snappy_HOME} )
endif()

# Try the parameterized roots, if they exist
if ( _snappy_roots )
    find_path( SNAPPY_INCLUDE_DIR NAMES snappy.h
        PATHS ${_snappy_roots} NO_DEFAULT_PATH
        PATH_SUFFIXES "include" )
    find_library( SNAPPY_LIBRARIES NAMES snappy
        PATHS ${_snappy_roots} NO_DEFAULT_PATH
        PATH_SUFFIXES "lib" )
else ()
    find_path( SNAPPY_INCLUDE_DIR NAMES snappy.h )
    find_library( SNAPPY_LIBRARIES NAMES snappy )
endif ()


if (SNAPPY_INCLUDE_DIR AND SNAPPY_LIBRARIES)
  set(SNAPPY_FOUND TRUE)
  get_filename_component( SNAPPY_LIBS ${SNAPPY_LIBRARIES} PATH )
  set(SNAPPY_LIB_NAME libsnappy)
  set(SNAPPY_STATIC_LIB ${SNAPPY_LIBS}/${SNAPPY_LIB_NAME}.a)
  set(SNAPPY_SHARED_LIB ${SNAPPY_LIBS}/${SNAPPY_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
else ()
  set(SNAPPY_FOUND FALSE)
endif ()

if (SNAPPY_FOUND)
  if (NOT Snappy_FIND_QUIETLY)
    message(STATUS "Found the Snappy library: ${SNAPPY_LIBRARIES}")
  endif ()
else ()
  if (NOT Snappy_FIND_QUIETLY)
    set(SNAPPY_ERR_MSG "Could not find the Snappy library. Looked in ")
    if ( _snappy_roots )
      set(SNAPPY_ERR_MSG "${SNAPPY_ERR_MSG} in ${_snappy_roots}.")
    else ()
      set(SNAPPY_ERR_MSG "${SNAPPY_ERR_MSG} system search paths.")
    endif ()
    if (Snappy_FIND_REQUIRED)
      message(FATAL_ERROR "${SNAPPY_ERR_MSG}")
    else (Snappy_FIND_REQUIRED)
      message(STATUS "${SNAPPY_ERR_MSG}")
    endif (Snappy_FIND_REQUIRED)
  endif ()
endif ()

mark_as_advanced(
  SNAPPY_INCLUDE_DIR
  SNAPPY_LIBS
  SNAPPY_LIBRARIES
  SNAPPY_STATIC_LIB
  SNAPPY_SHARED_LIB
)
