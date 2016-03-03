# Macros for use with the Vc library. Vc can be found at http://code.compeng.uni-frankfurt.de/projects/vc
#
# The following macros are provided:
# vc_determine_compiler
# vc_set_preferred_compiler_flags
#
#=============================================================================
# Copyright 2009-2013   Matthias Kretz <kretz@kde.org>
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

cmake_minimum_required(VERSION 2.8.3)

get_filename_component(_currentDir "${CMAKE_CURRENT_LIST_FILE}" PATH)
include ("${_currentDir}/UserWarning.cmake")
include ("${_currentDir}/AddCompilerFlag.cmake")
include ("${_currentDir}/OptimizeForArchitecture.cmake")

macro(vc_determine_compiler)
   if(NOT DEFINED Vc_COMPILER_IS_INTEL)
      execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "--version" OUTPUT_VARIABLE _cxx_compiler_version ERROR_VARIABLE _cxx_compiler_version)
      set(Vc_COMPILER_IS_INTEL false)
      set(Vc_COMPILER_IS_OPEN64 false)
      set(Vc_COMPILER_IS_CLANG false)
      set(Vc_COMPILER_IS_MSVC false)
      set(Vc_COMPILER_IS_GCC false)
      if(CMAKE_CXX_COMPILER MATCHES "/(icpc|icc)$")
         set(Vc_COMPILER_IS_INTEL true)
         exec_program(${CMAKE_C_COMPILER} ARGS -dumpversion OUTPUT_VARIABLE Vc_ICC_VERSION)
         message(STATUS "Detected Compiler: Intel ${Vc_ICC_VERSION}")
      elseif(CMAKE_CXX_COMPILER MATCHES "(opencc|openCC)$")
         set(Vc_COMPILER_IS_OPEN64 true)
         message(STATUS "Detected Compiler: Open64")
      elseif(CMAKE_CXX_COMPILER MATCHES "clang\\+\\+$" OR "${_cxx_compiler_version}" MATCHES "clang")
         set(Vc_COMPILER_IS_CLANG true)
         exec_program(${CMAKE_CXX_COMPILER} ARGS --version OUTPUT_VARIABLE Vc_CLANG_VERSION)
         string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" Vc_CLANG_VERSION "${Vc_CLANG_VERSION}")
         message(STATUS "Detected Compiler: Clang ${Vc_CLANG_VERSION}")
      elseif(MSVC)
         set(Vc_COMPILER_IS_MSVC true)
         message(STATUS "Detected Compiler: MSVC ${MSVC_VERSION}")
      elseif(CMAKE_COMPILER_IS_GNUCXX)
         set(Vc_COMPILER_IS_GCC true)
         exec_program(${CMAKE_C_COMPILER} ARGS -dumpversion OUTPUT_VARIABLE Vc_GCC_VERSION)
         message(STATUS "Detected Compiler: GCC ${Vc_GCC_VERSION}")

         # some distributions patch their GCC to return nothing or only major and minor version on -dumpversion.
         # In that case we must extract the version number from --version.
         if(NOT Vc_GCC_VERSION OR Vc_GCC_VERSION MATCHES "^[0-9]\\.[0-9]+$")
            exec_program(${CMAKE_C_COMPILER} ARGS --version OUTPUT_VARIABLE Vc_GCC_VERSION)
            string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" Vc_GCC_VERSION "${Vc_GCC_VERSION}")
            message(STATUS "GCC Version from --version: ${Vc_GCC_VERSION}")
         endif()

         # some distributions patch their GCC to be API incompatible to what the FSF released. In
         # those cases we require a macro to identify the distribution version
         find_program(_lsb_release lsb_release)
         mark_as_advanced(_lsb_release)
         if(_lsb_release)
            execute_process(COMMAND ${_lsb_release} -is OUTPUT_VARIABLE _distributor_id OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(COMMAND ${_lsb_release} -rs OUTPUT_VARIABLE _distributor_release OUTPUT_STRIP_TRAILING_WHITESPACE)
            string(TOUPPER "${_distributor_id}" _distributor_id)
            if(_distributor_id STREQUAL "UBUNTU")
               execute_process(COMMAND ${CMAKE_C_COMPILER} --version OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE _gcc_version)
               string(REGEX MATCH "\\(.* ${Vc_GCC_VERSION}-([0-9]+).*\\)" _tmp "${_gcc_version}")
               if(_tmp)
                  set(_patch ${CMAKE_MATCH_1})
                  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)$" _tmp "${_distributor_release}")
                  execute_process(COMMAND printf 0x%x%02x%02x ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${_patch} OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE _tmp)
                  set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -D__GNUC_UBUNTU_VERSION__=${_tmp}")
               endif()
            endif()
         endif()
      else()
         message(WARNING "Untested/-supported Compiler (${CMAKE_CXX_COMPILER}) for use with Vc.\nPlease fill out the missing parts in the CMake scripts and submit a patch to http://code.compeng.uni-frankfurt.de/projects/vc")
      endif()
   endif()
endmacro()

macro(vc_set_gnu_buildtype_flags)
   set(CMAKE_CXX_FLAGS_DEBUG          "-g3"          CACHE STRING "Flags used by the compiler during debug builds." FORCE)
   set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG" CACHE STRING "Flags used by the compiler during release minsize builds." FORCE)
   set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG" CACHE STRING "Flags used by the compiler during release builds (/MD /Ob1 /Oi /Ot /Oy /Gs will produce slightly less optimized but smaller files)." FORCE)
   set(CMAKE_CXX_FLAGS_RELWITHDEBUG   "-O3"          CACHE STRING "Flags used by the compiler during release builds containing runtime checks." FORCE)
   set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBUG} -g" CACHE STRING "Flags used by the compiler during Release with Debug Info builds." FORCE)
   set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}"          CACHE STRING "Flags used by the compiler during debug builds." FORCE)
   set(CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}"     CACHE STRING "Flags used by the compiler during release minsize builds." FORCE)
   set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}"        CACHE STRING "Flags used by the compiler during release builds (/MD /Ob1 /Oi /Ot /Oy /Gs will produce slightly less optimized but smaller files)." FORCE)
   set(CMAKE_C_FLAGS_RELWITHDEBUG   "${CMAKE_CXX_FLAGS_RELWITHDEBUG}"   CACHE STRING "Flags used by the compiler during release builds containing runtime checks." FORCE)
   set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "Flags used by the compiler during Release with Debug Info builds." FORCE)
   if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebug")
      set(ENABLE_STRICT_ALIASING true CACHE BOOL "Enables strict aliasing rules for more aggressive optimizations")
      if(NOT ENABLE_STRICT_ALIASING)
         AddCompilerFlag(-fno-strict-aliasing)
      endif(NOT ENABLE_STRICT_ALIASING)
   endif()
   mark_as_advanced(CMAKE_CXX_FLAGS_RELWITHDEBUG CMAKE_C_FLAGS_RELWITHDEBUG)
endmacro()

macro(vc_add_compiler_flag VAR _flag)
   AddCompilerFlag("${_flag}" CXX_FLAGS ${VAR})
endmacro()

macro(vc_check_assembler)
   if(APPLE)
      if(NOT Vc_COMPILER_IS_CLANG)
         message(WARNING "Apple does not provide an assembler with AVX support. AVX will not be available. Please use Clang if you want to use AVX.")
         set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -DVC_NO_XGETBV")
         set(Vc_AVX_INTRINSICS_BROKEN true)
         set(Vc_AVX2_INTRINSICS_BROKEN true)
      endif()
   else(APPLE)
      if(${ARGC} EQUAL 1)
         set(_as "${ARGV1}")
      else()
         exec_program(${CMAKE_CXX_COMPILER} ARGS -print-prog-name=as OUTPUT_VARIABLE _as)
         mark_as_advanced(_as)
      endif()
      if(NOT _as)
         message(WARNING "Could not find 'as', the assembler used by GCC. Hoping everything will work out...")
      else()
         exec_program(${_as} ARGS --version OUTPUT_VARIABLE _as_version)
         string(REGEX REPLACE "\\([^\\)]*\\)" "" _as_version "${_as_version}")
         string(REGEX MATCH "[1-9]\\.[0-9]+(\\.[0-9]+)?" _as_version "${_as_version}")
         if(_as_version VERSION_LESS "2.18.93")
            UserWarning("Your binutils is too old (${_as_version}). Some optimizations of Vc will be disabled.")
            add_definitions(-DVC_NO_XGETBV) # old assembler doesn't know the xgetbv instruction
            set(Vc_AVX_INTRINSICS_BROKEN true)
            set(Vc_XOP_INTRINSICS_BROKEN true)
            set(Vc_FMA4_INTRINSICS_BROKEN true)
         elseif(_as_version VERSION_LESS "2.21.0")
            UserWarning("Your binutils is too old (${_as_version}) for XOP instructions. They will therefore not be provided in libVc.")
            set(Vc_XOP_INTRINSICS_BROKEN true)
         endif()
      endif()
   endif(APPLE)
endmacro()

macro(vc_check_fpmath)
   # if compiling for 32 bit x86 we need to use the -mfpmath=sse since the x87 is broken by design
   include (CheckCXXSourceRuns)
   check_cxx_source_runs("int main() { return sizeof(void*) != 8; }" Vc_VOID_PTR_IS_64BIT)
   if(NOT Vc_VOID_PTR_IS_64BIT)
      exec_program(${CMAKE_C_COMPILER} ARGS -dumpmachine OUTPUT_VARIABLE _gcc_machine)
      if(_gcc_machine MATCHES "[x34567]86" OR _gcc_machine STREQUAL "mingw32")
         vc_add_compiler_flag(Vc_DEFINITIONS "-mfpmath=sse")
      endif()
   endif()
endmacro()

macro(vc_set_preferred_compiler_flags)
   vc_determine_compiler()

   set(_add_warning_flags false)
   set(_add_buildtype_flags false)
   foreach(_arg ${ARGN})
      if(_arg STREQUAL "WARNING_FLAGS")
         set(_add_warning_flags true)
      elseif(_arg STREQUAL "BUILDTYPE_FLAGS")
         set(_add_buildtype_flags true)
      endif()
   endforeach()

   set(Vc_SSE_INTRINSICS_BROKEN false)
   set(Vc_AVX_INTRINSICS_BROKEN false)
   set(Vc_AVX2_INTRINSICS_BROKEN false)
   set(Vc_XOP_INTRINSICS_BROKEN false)
   set(Vc_FMA4_INTRINSICS_BROKEN false)

   if(Vc_COMPILER_IS_OPEN64)
      ##################################################################################################
      #                                             Open64                                             #
      ##################################################################################################
      if(_add_warning_flags)
         AddCompilerFlag("-W")
         AddCompilerFlag("-Wall")
         AddCompilerFlag("-Wimplicit")
         AddCompilerFlag("-Wswitch")
         AddCompilerFlag("-Wformat")
         AddCompilerFlag("-Wchar-subscripts")
         AddCompilerFlag("-Wparentheses")
         AddCompilerFlag("-Wmultichar")
         AddCompilerFlag("-Wtrigraphs")
         AddCompilerFlag("-Wpointer-arith")
         AddCompilerFlag("-Wcast-align")
         AddCompilerFlag("-Wreturn-type")
         AddCompilerFlag("-Wno-unused-function")
         AddCompilerFlag("-pedantic")
         AddCompilerFlag("-Wno-long-long")
         AddCompilerFlag("-Wshadow")
         AddCompilerFlag("-Wold-style-cast")
         AddCompilerFlag("-Wno-variadic-macros")
      endif()
      if(_add_buildtype_flags)
         vc_set_gnu_buildtype_flags()
      endif()

      vc_check_assembler()

      # Open64 4.5.1 still doesn't ship immintrin.h
      set(Vc_AVX_INTRINSICS_BROKEN true)
      set(Vc_AVX2_INTRINSICS_BROKEN true)
   elseif(Vc_COMPILER_IS_GCC)
      ##################################################################################################
      #                                              GCC                                               #
      ##################################################################################################
      if(_add_warning_flags)
         foreach(_f -W -Wall -Wswitch -Wformat -Wchar-subscripts -Wparentheses -Wmultichar -Wtrigraphs -Wpointer-arith -Wcast-align -Wreturn-type -Wno-unused-function -pedantic -Wshadow -Wundef -Wold-style-cast -Wno-variadic-macros)
            AddCompilerFlag("${_f}")
         endforeach()
         if(Vc_GCC_VERSION VERSION_GREATER "4.5.2" AND Vc_GCC_VERSION VERSION_LESS "4.6.4")
            # GCC gives bogus "array subscript is above array bounds" warnings in math.cpp
            AddCompilerFlag("-Wno-array-bounds")
         endif()
         if(Vc_GCC_VERSION VERSION_GREATER "4.7.99")
            # GCC 4.8 warns about stuff we don't care about
            # Some older GCC versions have problems to note that they don't support the flag
            AddCompilerFlag("-Wno-unused-local-typedefs")
         endif()
      endif()
      vc_add_compiler_flag(Vc_DEFINITIONS "-Wabi")
      vc_add_compiler_flag(Vc_DEFINITIONS "-fabi-version=0") # ABI version 4 is required to make __m128 and __m256 appear as different types. 0 should give us the latest version.

      if(_add_buildtype_flags)
         vc_set_gnu_buildtype_flags()
      endif()

      # GCC 4.5.[01] fail at inlining some functions, creating functions with a single instructions,
      # thus creating a large overhead.
      if(Vc_GCC_VERSION VERSION_LESS "4.5.2" AND NOT Vc_GCC_VERSION VERSION_LESS "4.5.0")
         UserWarning("GCC 4.5.0 and 4.5.1 have problems with inlining correctly. Setting early-inlining-insns=12 as workaround.")
         AddCompilerFlag("--param early-inlining-insns=12")
      endif()

      if(Vc_GCC_VERSION VERSION_LESS "4.1.99")
         UserWarning("Your GCC is ancient and crashes on some important optimizations.  The full set of SSE2 intrinsics is not supported.  Vc will fall back to the scalar implementation.  Use of the may_alias and always_inline attributes will be disabled.  In turn all code using Vc must be compiled with -fno-strict-aliasing")
         vc_add_compiler_flag(Vc_DEFINITIONS "-fno-strict-aliasing")
         set(Vc_AVX_INTRINSICS_BROKEN true)
         set(Vc_AVX2_INTRINSICS_BROKEN true)
         set(Vc_SSE_INTRINSICS_BROKEN true)
      elseif(Vc_GCC_VERSION VERSION_LESS "4.4.6")
         UserWarning("Your GCC is older than 4.4.6. This is known to cause problems/bugs. Please update to the latest GCC if you can.")
         set(Vc_AVX_INTRINSICS_BROKEN true)
         set(Vc_AVX2_INTRINSICS_BROKEN true)
         if(Vc_GCC_VERSION VERSION_LESS "4.3.0")
            UserWarning("Your GCC is older than 4.3.0. It is unable to handle the full set of SSE2 intrinsics. All SSE code will be disabled. Please update to the latest GCC if you can.")
            set(Vc_SSE_INTRINSICS_BROKEN true)
         endif()
      endif()

      if(Vc_GCC_VERSION VERSION_LESS 4.5.0)
         UserWarning("GCC 4.4.x shows false positives for -Wparentheses, thus we rather disable the warning.")
         string(REPLACE " -Wparentheses " " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
         string(REPLACE " -Wparentheses " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
         set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -Wno-parentheses")

         UserWarning("GCC 4.4.x shows false positives for -Wstrict-aliasing, thus we rather disable the warning. Use a newer GCC for better warnings.")
         AddCompilerFlag("-Wno-strict-aliasing")

         UserWarning("GCC 4.4.x shows false positives for -Wuninitialized, thus we rather disable the warning. Use a newer GCC for better warnings.")
         AddCompilerFlag("-Wno-uninitialized")
      elseif(Vc_GCC_VERSION VERSION_EQUAL 4.6.0)
         UserWarning("GCC 4.6.0 miscompiles AVX loads/stores, leading to spurious segfaults. Disabling AVX per default.")
         set(Vc_AVX_INTRINSICS_BROKEN true)
         set(Vc_AVX2_INTRINSICS_BROKEN true)
      elseif(Vc_GCC_VERSION VERSION_EQUAL 4.7.0)
         UserWarning("GCC 4.7.0 miscompiles at -O3, adding -fno-predictive-commoning to the compiler flags as workaround")
         set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -fno-predictive-commoning")
      endif()

      vc_check_fpmath()
      vc_check_assembler()
   elseif(Vc_COMPILER_IS_INTEL)
      ##################################################################################################
      #                                          Intel Compiler                                        #
      ##################################################################################################

      if(_add_buildtype_flags)
         set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -O3")
         set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DNDEBUG -O3")
         set(CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE} -O3")
         set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -DNDEBUG -O3")

         set(ALIAS_FLAGS "-no-ansi-alias")
         if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            # default ICC to -no-ansi-alias because otherwise tests/utils_sse fails. So far I suspect a miscompilation...
            set(ENABLE_STRICT_ALIASING true CACHE BOOL "Enables strict aliasing rules for more aggressive optimizations")
            if(ENABLE_STRICT_ALIASING)
               set(ALIAS_FLAGS "-ansi-alias")
            endif(ENABLE_STRICT_ALIASING)
         endif()
         set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${ALIAS_FLAGS}")
         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALIAS_FLAGS}")
      endif()
      vc_add_compiler_flag(Vc_DEFINITIONS "-diag-disable 913")
      # Disable warning #13211 "Immediate parameter to intrinsic call too large". (sse/vector.tcc rotated(int))
      vc_add_compiler_flag(Vc_DEFINITIONS "-diag-disable 13211")

      if(NOT "$ENV{DASHBOARD_TEST_FROM_CTEST}" STREQUAL "")
         # disable warning #2928: the __GXX_EXPERIMENTAL_CXX0X__ macro is disabled when using GNU version 4.6 with the c++0x option
         # this warning just adds noise about problems in the compiler - but I'm only interested in seeing problems in Vc
         vc_add_compiler_flag(Vc_DEFINITIONS "-diag-disable 2928")
      endif()

      # Intel doesn't implement the XOP or FMA4 intrinsics
      set(Vc_XOP_INTRINSICS_BROKEN true)
      set(Vc_FMA4_INTRINSICS_BROKEN true)
   elseif(Vc_COMPILER_IS_MSVC)
      ##################################################################################################
      #                                      Microsoft Visual Studio                                   #
      ##################################################################################################

      if(_add_warning_flags)
         AddCompilerFlag("/wd4800") # Disable warning "forcing value to bool"
         AddCompilerFlag("/wd4996") # Disable warning about strdup vs. _strdup
         AddCompilerFlag("/wd4244") # Disable warning "conversion from 'unsigned int' to 'float', possible loss of data"
         AddCompilerFlag("/wd4146") # Disable warning "unary minus operator applied to unsigned type, result still unsigned"
         AddCompilerFlag("/wd4227") # Disable warning "anachronism used : qualifiers on reference are ignored" (this is about 'restrict' usage on references, stupid MSVC)
         AddCompilerFlag("/wd4722") # Disable warning "destructor never returns, potential memory leak" (warns about ~_UnitTest_Global_Object which we don't care about)
         AddCompilerFlag("/wd4748") # Disable warning "/GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function" (I don't get it)
         add_definitions(-D_CRT_SECURE_NO_WARNINGS)
      endif()

      # MSVC does not support inline assembly on 64 bit! :(
      # searching the help for xgetbv doesn't turn up anything. So just fall back to not supporting AVX on Windows :(
      # TODO: apparently MSVC 2010 SP1 added _xgetbv
      set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -DVC_NO_XGETBV")

      # get rid of the min/max macros
      set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -DNOMINMAX")

      # MSVC doesn't implement the XOP or FMA4 intrinsics
      set(Vc_XOP_INTRINSICS_BROKEN true)
      set(Vc_FMA4_INTRINSICS_BROKEN true)

      if(MSVC_VERSION LESS 1700)
         UserWarning("MSVC before 2012 has a broken std::vector::resize implementation. STL + Vc code will probably not compile.")
      endif()
   elseif(Vc_COMPILER_IS_CLANG)
      ##################################################################################################
      #                                              Clang                                             #
      ##################################################################################################

      # for now I don't know of any arguments I want to pass. -march and stuff is tried by OptimizeForArchitecture...
      if(Vc_CLANG_VERSION VERSION_EQUAL "3.0")
         UserWarning("Clang 3.0 has serious issues to compile Vc code and will most likely crash when trying to do so.\nPlease update to a recent clang version.")
      elseif(Vc_CLANG_VERSION VERSION_LESS "3.3")
         # the LLVM assembler gets FMAs wrong (bug 15040)
         vc_add_compiler_flag(Vc_DEFINITIONS "-no-integrated-as")
      else()
         vc_add_compiler_flag(Vc_DEFINITIONS "-integrated-as")
      endif()

      # disable these warnings because clang shows them for function overloads that were discarded via SFINAE
      vc_add_compiler_flag(Vc_DEFINITIONS "-Wno-local-type-template-args")
      vc_add_compiler_flag(Vc_DEFINITIONS "-Wno-unnamed-type-template-args")

      if(XCODE)
         # set_target_properties(${_target} PROPERTIES XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
      else()
         AddCompilerFlag(-stdlib=libc++)
      endif()
   endif()

   if(NOT Vc_COMPILER_IS_MSVC)
      vc_add_compiler_flag(Vc_DEFINITIONS "-ffp-contract=fast")
   endif()

   OptimizeForArchitecture()
   set(Vc_DEFINITIONS "${Vc_ARCHITECTURE_FLAGS} ${Vc_DEFINITIONS}")

   set(VC_IMPL "auto" CACHE STRING "Force the Vc implementation globally to the selected instruction set. \"auto\" lets Vc use the best available instructions.")
   if(NOT VC_IMPL STREQUAL "auto")
      set(Vc_DEFINITIONS "${Vc_DEFINITIONS} -DVC_IMPL=${VC_IMPL}")
      if(NOT VC_IMPL STREQUAL "Scalar")
         set(_use_var "USE_${VC_IMPL}")
         if(VC_IMPL STREQUAL "SSE")
            set(_use_var "USE_SSE2")
         endif()
         if(NOT ${_use_var})
            message(WARNING "The selected value for VC_IMPL (${VC_IMPL}) will not work because the relevant instructions are not enabled via compiler flags.")
         endif()
      endif()
   endif()
endmacro()

# helper macro for vc_compile_for_all_implementations
macro(_vc_compile_one_implementation _srcs _impl)
   list(FIND _disabled_targets "${_impl}" _disabled_index)
   list(FIND _only_targets "${_impl}" _only_index)
   if(${_disabled_index} EQUAL -1 AND (NOT _only_targets OR ${_only_index} GREATER -1))
      set(_extra_flags)
      set(_ok FALSE)
      foreach(_flags ${ARGN})
         if(_flags STREQUAL "NO_FLAG")
            set(_ok TRUE)
            break()
         endif()
         string(REPLACE " " ";" _flag_list "${_flags}")
         foreach(_f ${_flag_list})
            AddCompilerFlag(${_f} CXX_RESULT _ok)
            if(NOT _ok)
               break()
            endif()
         endforeach()
         if(_ok)
            set(_extra_flags ${_flags})
            break()
         endif()
      endforeach()

      if(Vc_COMPILER_IS_MSVC)
         # MSVC for 64bit does not recognize /arch:SSE2 anymore. Therefore we set override _ok if _impl
         # says SSE
         if("${_impl}" MATCHES "SSE")
            set(_ok TRUE)
         endif()
      endif()

      if(_ok)
         get_filename_component(_out "${_vc_compile_src}" NAME_WE)
         get_filename_component(_ext "${_vc_compile_src}" EXT)
         set(_out "${CMAKE_CURRENT_BINARY_DIR}/${_out}_${_impl}${_ext}")
         add_custom_command(OUTPUT "${_out}"
            COMMAND ${CMAKE_COMMAND} -E copy "${_vc_compile_src}" "${_out}"
            MAIN_DEPENDENCY "${_vc_compile_src}"
            COMMENT "Copy to ${_out}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM)
         set_source_files_properties( "${_out}" PROPERTIES
            COMPILE_DEFINITIONS "VC_IMPL=${_impl}"
            COMPILE_FLAGS "${_flags} ${_extra_flags}"
         )
         list(APPEND ${_srcs} "${_out}")
      endif()
   endif()
endmacro()

# Generate compile rules for the given C++ source file for all available implementations and return
# the resulting list of object files in _obj
# all remaining arguments are additional flags
# Example:
#   vc_compile_for_all_implementations(_objs src/trigonometric.cpp FLAGS -DCOMPILE_BLAH EXCLUDE Scalar)
#   add_executable(executable main.cpp ${_objs})
macro(vc_compile_for_all_implementations _srcs _src)
   set(_flags)
   unset(_disabled_targets)
   unset(_only_targets)
   set(_state 0)
   foreach(_arg ${ARGN})
      if(_arg STREQUAL "FLAGS")
         set(_state 1)
      elseif(_arg STREQUAL "EXCLUDE")
         set(_state 2)
      elseif(_arg STREQUAL "ONLY")
         set(_state 3)
      elseif(_state EQUAL 1)
         set(_flags "${_flags} ${_arg}")
      elseif(_state EQUAL 2)
         list(APPEND _disabled_targets "${_arg}")
      elseif(_state EQUAL 3)
         list(APPEND _only_targets "${_arg}")
      else()
         message(FATAL_ERROR "incorrect argument to vc_compile_for_all_implementations")
      endif()
   endforeach()

   set(_vc_compile_src "${_src}")

   _vc_compile_one_implementation(${_srcs} Scalar NO_FLAG)
   if(NOT Vc_SSE_INTRINSICS_BROKEN)
      _vc_compile_one_implementation(${_srcs} SSE2   "-xSSE2"   "-msse2"   "/arch:SSE2")
      _vc_compile_one_implementation(${_srcs} SSE3   "-xSSE3"   "-msse3"   "/arch:SSE2")
      _vc_compile_one_implementation(${_srcs} SSSE3  "-xSSSE3"  "-mssse3"  "/arch:SSE2")
      _vc_compile_one_implementation(${_srcs} SSE4_1 "-xSSE4.1" "-msse4.1" "/arch:SSE2")
      _vc_compile_one_implementation(${_srcs} SSE4_2 "-xSSE4.2" "-msse4.2" "/arch:SSE2")
      _vc_compile_one_implementation(${_srcs} SSE3+SSE4a  "-msse4a")
   endif()
   if(NOT Vc_AVX_INTRINSICS_BROKEN)
      _vc_compile_one_implementation(${_srcs} AVX      "-xAVX"    "-mavx"    "/arch:AVX")
      if(NOT Vc_XOP_INTRINSICS_BROKEN)
         if(NOT Vc_FMA4_INTRINSICS_BROKEN)
            _vc_compile_one_implementation(${_srcs} SSE+XOP+FMA4 "-mxop -mfma4"        ""    "")
            _vc_compile_one_implementation(${_srcs} AVX+XOP+FMA4 "-mavx -mxop -mfma4"  ""    "")
         endif()
         _vc_compile_one_implementation(${_srcs} SSE+XOP+FMA "-mxop -mfma"        ""    "")
         _vc_compile_one_implementation(${_srcs} AVX+XOP+FMA "-mavx -mxop -mfma"  ""    "")
      endif()
      _vc_compile_one_implementation(${_srcs} AVX+FMA "-mavx -mfma"  ""    "")
   endif()
   if(NOT Vc_AVX2_INTRINSICS_BROKEN)
      _vc_compile_one_implementation(${_srcs} AVX2 "-xCORE-AVX2"  "-mavx2"    "/arch:AVX2")
   endif()
endmacro()
