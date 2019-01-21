# - Check whether the MIC CXX compiler supports a given flag.
# CHECK_MIC_CXX_COMPILER_FLAG(<flag> <var>)
#  <flag> - the compiler flag
#  <var>  - variable to store the result
# This internally calls the check_cxx_source_compiles macro.  See help
# for CheckCXXSourceCompiles for a listing of variables that can
# modify the build.

#=============================================================================
# Copyright 2006-2009 Kitware, Inc.
# Copyright 2006 Alexander Neundorf <neundorf@kde.org>
# Copyright 2011-2013 Matthias Kretz <kretz@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
#  * The names of Kitware, Inc., the Insight Consortium, or the names of
#    any consortium members, or of any contributors, may not be used to
#    endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

macro(check_mic_cxx_compiler_flag _FLAG _RESULT)
   if("${_RESULT}" MATCHES "^${_RESULT}$")
      set(_tmpdir "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp")
      if(${ARGC} GREATER 2)
         file(WRITE "${_tmpdir}/src.cpp" "${ARGV2}")
      else()
         file(WRITE "${_tmpdir}/src.cpp" "int main() { return 0; }")
      endif()

      execute_process(
         COMMAND "${MIC_CXX}" -mmic -c -o "${_tmpdir}/src.o"
         "${_FLAG}" "${_tmpdir}/src.cpp"
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         RESULT_VARIABLE ${_RESULT}
         OUTPUT_VARIABLE OUTPUT
         ERROR_VARIABLE OUTPUT
         )

      if(${_RESULT} EQUAL 0)
         foreach(_fail_regex
               "error: bad value (.*) for .* switch"       # GNU
               "argument unused during compilation"        # clang
               "is valid for .* but not for C\\\\+\\\\+"   # GNU
               "unrecognized .*option"                     # GNU
               "ignored for target"                        # GNU
               "ignoring unknown option"                   # MSVC
               "[Uu]nknown option"                         # HP
               "[Ww]arning: [Oo]ption"                     # SunPro
               "command option .* is not recognized"       # XL
               "WARNING: unknown flag:"                    # Open64
               "command line error"                        # ICC
               "command line warning"                      # ICC
               "#10236:"                                   # ICC: File not found
               )
            if("${OUTPUT}" MATCHES "${_fail_regex}")
               set(${_RESULT} FALSE)
            endif()
         endforeach()
      endif()

      if(${_RESULT} EQUAL 0)
         set(${_RESULT} 1 CACHE INTERNAL "Test ${_FLAG}")
         message(STATUS "Performing Test Check MIC C++ Compiler flag ${_FLAG} - Success")
         file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
            "Performing MIC C++ Compiler Flag Test ${_FLAG} succeded with the following output:\n"
            "${OUTPUT}\n"
            "COMMAND: ${MIC_CXX} -mmic -c -o ${_tmpdir}/src.o ${_FLAG} ${_tmpdir}/src.cpp\n"
            )
      else()
         message(STATUS "Performing Test Check MIC C++ Compiler flag ${_FLAG} - Failed")
         set(${_RESULT} "" CACHE INTERNAL "Test ${_FLAG}")
         file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
            "Performing MIC C++ Compiler Flag Test ${_FLAG} failed with the following output:\n"
            "${OUTPUT}\n"
            "COMMAND: ${MIC_CXX} -mmic -c -o ${_tmpdir}/src.o ${_FLAG} ${_tmpdir}/src.cpp\n"
            )
      endif()
   endif()
endmacro()

