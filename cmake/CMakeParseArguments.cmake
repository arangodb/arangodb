#
# CMakeParseArguments
# -------------------
#
# The cmake_parse_arguments() command is now a CMake built-in since CMake 3.5.
# This file is kept only for backwards compatibility with third-party code that
# does include(CMakeParseArguments).  It intentionally defines nothing so that
# the built-in command is not shadowed.
#
# CMake 4.x changed internal modules (e.g. CheckTypeSize) to use the
# cmake_parse_arguments(PARSE_ARGV ...) form.  The old function definition
# in this file did not understand that form, which caused those modules to
# receive too few arguments and fail.  Removing the definition here lets the
# built-in handle all callers correctly.
