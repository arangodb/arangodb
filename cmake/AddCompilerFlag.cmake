# - Add a given compiler flag to flags variables.
# AddCompilerFlag(<flag> [<var>])
# or
# AddCompilerFlag(<flag> [C_FLAGS <var>] [CXX_FLAGS <var>] [C_RESULT <var>]
#                        [CXX_RESULT <var>])

#=============================================================================
# Copyright 2010-2013 Matthias Kretz <kretz@kde.org>
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

get_filename_component(_currentDir "${CMAKE_CURRENT_LIST_FILE}" PATH)
include("${_currentDir}/CheckCCompilerFlag.cmake")
include("${_currentDir}/CheckCXXCompilerFlag.cmake")
include("${_currentDir}/CheckMicCCompilerFlag.cmake")
include("${_currentDir}/CheckMicCXXCompilerFlag.cmake")

macro(AddCompilerFlag _flag)
   string(REGEX REPLACE "[-.+/:= ]" "_" _flag_esc "${_flag}")

   set(_c_flags "CMAKE_C_FLAGS")
   set(_cxx_flags "CMAKE_CXX_FLAGS")
   set(_mic_c_flags "CMAKE_MIC_C_FLAGS")
   set(_mic_cxx_flags "CMAKE_MIC_CXX_FLAGS")
   set(_c_result tmp)
   set(_cxx_result tmp)
   set(_mic_c_result)
   set(_mic_cxx_result)
   if(${ARGC} EQUAL 2)
      message(WARNING "Deprecated use of the AddCompilerFlag macro.")
      unset(_c_result)
      set(_cxx_result ${ARGV1})
   elseif(${ARGC} GREATER 2)
      set(state 0)
      unset(_c_flags)
      unset(_cxx_flags)
      unset(_mic_c_flags)
      unset(_mic_cxx_flags)
      unset(_c_result)
      unset(_cxx_result)
      unset(_mic_c_result)
      unset(_mic_cxx_result)
      foreach(_arg ${ARGN})
         if(_arg STREQUAL "C_FLAGS")
            set(state 1)
            if(NOT DEFINED _c_result)
               set(_c_result tmp0)
            endif()
         elseif(_arg STREQUAL "CXX_FLAGS")
            set(state 2)
            if(NOT DEFINED _cxx_result)
               set(_cxx_result tmp1)
            endif()
         elseif(_arg STREQUAL "C_RESULT")
            set(state 3)
         elseif(_arg STREQUAL "CXX_RESULT")
            set(state 4)
         elseif(_arg STREQUAL "MIC_C_RESULT")
            set(state 5)
         elseif(_arg STREQUAL "MIC_CXX_RESULT")
            set(state 6)
         elseif(_arg STREQUAL "MIC_C_FLAGS")
            if(NOT DEFINED _mic_c_result)
               set(_mic_c_result tmp2)
            endif()
            set(state 7)
         elseif(_arg STREQUAL "MIC_CXX_FLAGS")
            if(NOT DEFINED _mic_cxx_result)
               set(_mic_cxx_result tmp3)
            endif()
            set(state 8)
         elseif(state EQUAL 1)
            set(_c_flags "${_arg}")
         elseif(state EQUAL 2)
            set(_cxx_flags "${_arg}")
         elseif(state EQUAL 3)
            set(_c_result "${_arg}")
         elseif(state EQUAL 4)
            set(_cxx_result "${_arg}")
         elseif(state EQUAL 5)
            set(_mic_c_result "${_arg}")
         elseif(state EQUAL 6)
            set(_mic_cxx_result "${_arg}")
         elseif(state EQUAL 7)
            set(_mic_c_flags "${_arg}")
         elseif(state EQUAL 8)
            set(_mic_cxx_flags "${_arg}")
         else()
            message(FATAL_ERROR "Syntax error for AddCompilerFlag")
         endif()
      endforeach()
   endif()

   if("${_flag}" STREQUAL "-mfma")
      # Compiling with FMA3 support may fail only at the assembler level.
      # In that case we need to have such an instruction in the test code
      set(_code "#include <immintrin.h>
      __m128 foo(__m128 x) { return _mm_fmadd_ps(x, x, x); }
      int main() { return 0; }")
   elseif("${_flag}" STREQUAL "-stdlib=libc++")
      # Compiling with libc++ not only requires a compiler that understands it, but also
      # the libc++ headers itself
      set(_code "#include <iostream>
      int main() { return 0; }")
   else()
      set(_code "int main() { return 0; }")
   endif()

   if(DEFINED _c_result)
      check_c_compiler_flag("${_flag}" check_c_compiler_flag_${_flag_esc} "${_code}")
      set(${_c_result} ${check_c_compiler_flag_${_flag_esc}})
   endif()
   if(DEFINED _cxx_result)
      check_cxx_compiler_flag("${_flag}" check_cxx_compiler_flag_${_flag_esc} "${_code}")
      set(${_cxx_result} ${check_cxx_compiler_flag_${_flag_esc}})
   endif()

   if(check_c_compiler_flag_${_flag_esc} AND DEFINED _c_flags)
      if(${_c_flags})
         set(${_c_flags} "${${_c_flags}} ${_flag}")
      else()
         set(${_c_flags} "${_flag}")
      endif()
   endif()
   if(check_cxx_compiler_flag_${_flag_esc} AND DEFINED _cxx_flags)
      if(${_cxx_flags})
         set(${_cxx_flags} "${${_cxx_flags}} ${_flag}")
      else()
         set(${_cxx_flags} "${_flag}")
      endif()
   endif()

   if(MIC_NATIVE_FOUND)
      if(DEFINED _mic_c_result)
         check_mic_c_compiler_flag("${_flag}" check_mic_c_compiler_flag_${_flag_esc} "${_code}")
         set(${_mic_c_result} ${check_mic_c_compiler_flag_${_flag_esc}})
      endif()
      if(DEFINED _mic_cxx_result)
         check_mic_cxx_compiler_flag("${_flag}" check_mic_cxx_compiler_flag_${_flag_esc} "${_code}")
         set(${_mic_cxx_result} ${check_mic_cxx_compiler_flag_${_flag_esc}})
      endif()

      if(check_mic_c_compiler_flag_${_flag_esc} AND DEFINED _mic_c_flags)
         set(${_mic_c_flags} "${${_mic_c_flags}} ${_flag}")
      endif()
      if(check_mic_cxx_compiler_flag_${_flag_esc} AND DEFINED _mic_cxx_flags)
         set(${_mic_cxx_flags} "${${_mic_cxx_flags}} ${_flag}")
      endif()
   endif()
endmacro(AddCompilerFlag)
