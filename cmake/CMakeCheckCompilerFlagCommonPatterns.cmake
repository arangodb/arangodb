
#=============================================================================
# Copyright 2006-2011 Kitware, Inc.
# Copyright 2006 Alexander Neundorf <neundorf@kde.org>
# Copyright 2011 Matthias Kretz <kretz@kde.org>
# Copyright 2013 Rolf Eike Beer <eike@sf-mail.de>
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

# Do NOT include this module directly into any of your code. It is meant as
# a library for Check*CompilerFlag.cmake modules. It's content may change in
# any way between releases.

macro (CHECK_COMPILER_FLAG_COMMON_PATTERNS _VAR)
   set(${_VAR}
     FAIL_REGEX "unrecognized .*option"                     # GNU
     FAIL_REGEX "unknown .*option"                          # Clang
     FAIL_REGEX "ignoring unknown option"                   # MSVC
     FAIL_REGEX "warning D9002"                             # MSVC, any lang
     FAIL_REGEX "option.*not supported"                     # Intel
     FAIL_REGEX "invalid argument .*option"                 # Intel
     FAIL_REGEX "ignoring option .*argument required"       # Intel
     FAIL_REGEX "ignoring option .*argument is of wrong type" # Intel
     FAIL_REGEX "[Uu]nknown option"                         # HP
     FAIL_REGEX "[Ww]arning: [Oo]ption"                     # SunPro
     FAIL_REGEX "command option .* is not recognized"       # XL
     FAIL_REGEX "command option .* contains an incorrect subargument" # XL
     FAIL_REGEX "not supported in this configuration. ignored"       # AIX
     FAIL_REGEX "File with unknown suffix passed to linker" # PGI
     FAIL_REGEX "WARNING: unknown flag:"                    # Open64
     FAIL_REGEX "Incorrect command line option:"            # Borland
     FAIL_REGEX "Warning: illegal option"                   # SunStudio 12
     FAIL_REGEX "[Ww]arning: Invalid suboption"             # Fujitsu
   )
endmacro ()
