#.rst:
# FindPythonInterp
# ----------------
#
# Find python interpreter
#
# This module finds if Python interpreter is installed and determines
# where the executables are.  This code sets the following variables:
#
# ::
#
#   PYTHONINTERP_FOUND         - Was the Python executable found
#   PYTHON_EXECUTABLE          - path to the Python interpreter
#
#
#
# ::
#
#   PYTHON_VERSION_STRING      - Python version found e.g. 2.5.2
#   PYTHON_VERSION_MAJOR       - Python major version found e.g. 2
#   PYTHON_VERSION_MINOR       - Python minor version found e.g. 5
#   PYTHON_VERSION_PATCH       - Python patch version found e.g. 2
#
#
#
# The Python_ADDITIONAL_VERSIONS variable can be used to specify a list
# of version numbers that should be taken into account when searching
# for Python.  You need to set this variable before calling
# find_package(PythonInterp).
#
# If calling both ``find_package(PythonInterp)`` and
# ``find_package(PythonLibs)``, call ``find_package(PythonInterp)`` first to
# get the currently active Python version by default with a consistent version
# of PYTHON_LIBRARIES.

#=============================================================================
# Copyright 2005-2010 Kitware, Inc.
# Copyright 2011 Bjoern Ricks <bjoern.ricks@gmail.com>
# Copyright 2012 Rolf Eike Beer <eike@sf-mail.de>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

unset(_Python_NAMES)

set(_PYTHON1_VERSIONS 1.6 1.5)
set(_PYTHON2_VERSIONS 2.7 2.6 2.5 2.4 2.3 2.2 2.1 2.0)
set(_PYTHON3_VERSIONS 3.6 3.5 3.4 3.3 3.2 3.1 3.0)

if(PythonInterp_FIND_VERSION)
    if(PythonInterp_FIND_VERSION_COUNT GREATER 1)
        set(_PYTHON_FIND_MAJ_MIN "${PythonInterp_FIND_VERSION_MAJOR}.${PythonInterp_FIND_VERSION_MINOR}")
        list(APPEND _Python_NAMES
             python${_PYTHON_FIND_MAJ_MIN}
             python${PythonInterp_FIND_VERSION_MAJOR}
             python)
        unset(_PYTHON_FIND_OTHER_VERSIONS)
        if(NOT PythonInterp_FIND_VERSION_EXACT)
            foreach(_PYTHON_V ${_PYTHON${PythonInterp_FIND_VERSION_MAJOR}_VERSIONS})
                if(NOT _PYTHON_V VERSION_LESS _PYTHON_FIND_MAJ_MIN)
                    list(APPEND _PYTHON_FIND_OTHER_VERSIONS ${_PYTHON_V})
                endif()
             endforeach()
        endif()
        unset(_PYTHON_FIND_MAJ_MIN)
    else()
        list(APPEND _Python_NAMES python${PythonInterp_FIND_VERSION_MAJOR} python)
        set(_PYTHON_FIND_OTHER_VERSIONS ${_PYTHON${PythonInterp_FIND_VERSION_MAJOR}_VERSIONS})
    endif()
else()
    set(_PYTHON_FIND_OTHER_VERSIONS ${_PYTHON3_VERSIONS} ${_PYTHON2_VERSIONS} ${_PYTHON1_VERSIONS})
endif()
set(PATHLIST $ENV{PATH})
LIST(FILTER PATHLIST INCLUDE REGEX ".*on3.*")
find_program(PYTHON_EXECUTABLE NAMES ${_Python_NAMES} HINTS ${PATHLIST} CMAKE_FIND_ROOT_PATH_BOTH)

# Set up the versions we know about, in the order we will search. Always add
# the user supplied additional versions to the front.
set(_Python_VERSIONS ${Python_ADDITIONAL_VERSIONS})
# If FindPythonInterp has already found the major and minor version,
# insert that version next to get consistent versions of the interpreter and
# library.
if(DEFINED PYTHONLIBS_VERSION_STRING)
  string(REPLACE "." ";" _PYTHONLIBS_VERSION "${PYTHONLIBS_VERSION_STRING}")
  list(GET _PYTHONLIBS_VERSION 0 _PYTHONLIBS_VERSION_MAJOR)
  list(GET _PYTHONLIBS_VERSION 1 _PYTHONLIBS_VERSION_MINOR)
  list(APPEND _Python_VERSIONS ${_PYTHONLIBS_VERSION_MAJOR}.${_PYTHONLIBS_VERSION_MINOR})
endif()
# Search for the current active python version first
list(APPEND _Python_VERSIONS ";")
list(APPEND _Python_VERSIONS ${_PYTHON_FIND_OTHER_VERSIONS})

unset(_PYTHON_FIND_OTHER_VERSIONS)
unset(_PYTHON1_VERSIONS)
unset(_PYTHON2_VERSIONS)
unset(_PYTHON3_VERSIONS)

# Search for newest python version if python executable isn't found
if(NOT PYTHON_EXECUTABLE)
    foreach(_CURRENT_VERSION IN LISTS _Python_VERSIONS)
      set(_Python_NAMES python${_CURRENT_VERSION})
      if(WIN32)
        list(APPEND _Python_NAMES python)
      endif()
      find_program(PYTHON_EXECUTABLE
        NAMES ${_Python_NAMES}
        PATHS [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\${_CURRENT_VERSION}\\InstallPath]
        )
    endforeach()
endif()

# determine python version string
if(PYTHON_EXECUTABLE)
    execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c
                            "import sys; sys.stdout.write(';'.join([str(x) for x in sys.version_info[:3]]))"
                    OUTPUT_VARIABLE _VERSION
                    RESULT_VARIABLE _PYTHON_VERSION_RESULT
                    ERROR_QUIET)
    if(NOT _PYTHON_VERSION_RESULT)
        string(REPLACE ";" "." PYTHON_VERSION_STRING "${_VERSION}")
        list(GET _VERSION 0 PYTHON_VERSION_MAJOR)
        list(GET _VERSION 1 PYTHON_VERSION_MINOR)
        list(GET _VERSION 2 PYTHON_VERSION_PATCH)
        if(PYTHON_VERSION_PATCH EQUAL 0)
            # it's called "Python 2.7", not "2.7.0"
            string(REGEX REPLACE "\\.0$" "" PYTHON_VERSION_STRING "${PYTHON_VERSION_STRING}")
        endif()
    else()
        # sys.version predates sys.version_info, so use that
        execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c "import sys; sys.stdout.write(sys.version)"
                        OUTPUT_VARIABLE _VERSION
                        RESULT_VARIABLE _PYTHON_VERSION_RESULT
                        ERROR_QUIET)
        if(NOT _PYTHON_VERSION_RESULT)
            string(REGEX REPLACE " .*" "" PYTHON_VERSION_STRING "${_VERSION}")
            string(REGEX REPLACE "^([0-9]+)\\.[0-9]+.*" "\\1" PYTHON_VERSION_MAJOR "${PYTHON_VERSION_STRING}")
            string(REGEX REPLACE "^[0-9]+\\.([0-9])+.*" "\\1" PYTHON_VERSION_MINOR "${PYTHON_VERSION_STRING}")
            if(PYTHON_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.([0-9]+)")
                set(PYTHON_VERSION_PATCH "${CMAKE_MATCH_1}")
            else()
                set(PYTHON_VERSION_PATCH "0")
            endif()
        else()
            # sys.version was first documented for Python 1.5, so assume
            # this is older.
            set(PYTHON_VERSION_STRING "1.4")
            set(PYTHON_VERSION_MAJOR "1")
            set(PYTHON_VERSION_MINOR "4")
            set(PYTHON_VERSION_PATCH "0")
        endif()
    endif()
    unset(_PYTHON_VERSION_RESULT)
    unset(_VERSION)
endif()

# handle the QUIETLY and REQUIRED arguments and set PYTHONINTERP_FOUND to TRUE if
# all listed variables are TRUE
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonInterp REQUIRED_VARS PYTHON_EXECUTABLE VERSION_VAR PYTHON_VERSION_STRING)

mark_as_advanced(PYTHON_EXECUTABLE)
