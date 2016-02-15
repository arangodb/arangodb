#.rst:
# CheckCXXSymbolExists
# --------------------
#
# Check if a symbol exists as a function, variable, or macro in C++
#
# CHECK_CXX_SYMBOL_EXISTS(<symbol> <files> <variable>)
#
# Check that the <symbol> is available after including given header
# <files> and store the result in a <variable>.  Specify the list of
# files in one argument as a semicolon-separated list.
# CHECK_CXX_SYMBOL_EXISTS() can be used to check in C++ files, as
# opposed to CHECK_SYMBOL_EXISTS(), which works only for C.
#
# If the header files define the symbol as a macro it is considered
# available and assumed to work.  If the header files declare the symbol
# as a function or variable then the symbol must also be available for
# linking.  If the symbol is a type or enum value it will not be
# recognized (consider using CheckTypeSize or CheckCSourceCompiles).
#
# The following variables may be set before calling this macro to modify
# the way the check is run:
#
# ::
#
#   CMAKE_REQUIRED_FLAGS = string of compile command line flags
#   CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#   CMAKE_REQUIRED_INCLUDES = list of include directories
#   CMAKE_REQUIRED_LIBRARIES = list of libraries to link
#   CMAKE_REQUIRED_QUIET = execute quietly without messages

#=============================================================================
# Copyright 2003-2011 Kitware, Inc.
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

include(CheckSymbolExists)

macro(CHECK_CXX_SYMBOL_EXISTS SYMBOL FILES VARIABLE)
  _CHECK_SYMBOL_EXISTS("${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckSymbolExists.cxx" "${SYMBOL}" "${FILES}" "${VARIABLE}" )
endmacro()
