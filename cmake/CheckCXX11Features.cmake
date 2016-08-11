# - Check which parts of the C++11 standard the compiler supports
#
# When found it will set the following variables
#
#  CXX11_COMPILER_FLAGS         - the compiler flags needed to get C++11 features
#
#  HAS_CXX11_AUTO               - auto keyword
#  HAS_CXX11_AUTO_RET_TYPE      - function declaration with deduced return types
#  HAS_CXX11_CLASS_OVERRIDE     - override and final keywords for classes and methods
#  HAS_CXX11_CONSTEXPR          - constexpr keyword
#  HAS_CXX11_CSTDINT_H          - cstdint header
#  HAS_CXX11_DECLTYPE           - decltype keyword
#  HAS_CXX11_FUNC               - __func__ preprocessor constant
#  HAS_CXX11_INITIALIZER_LIST   - initializer list
#  HAS_CXX11_LAMBDA             - lambdas
#  HAS_CXX11_LIB_REGEX          - regex library
#  HAS_CXX11_LONG_LONG          - long long signed & unsigned types
#  HAS_CXX11_NULLPTR            - nullptr
#  HAS_CXX11_RVALUE_REFERENCES  - rvalue references
#  HAS_CXX11_SIZEOF_MEMBER      - sizeof() non-static members
#  HAS_CXX11_STATIC_ASSERT      - static_assert()
#  HAS_CXX11_VARIADIC_TEMPLATES - variadic templates
#  HAS_CXX11_SHARED_PTR         - Shared Pointer
#  HAS_CXX11_THREAD             - thread
#  HAS_CXX11_MUTEX              - mutex 
#  HAS_CXX11_NOEXCEPT           - noexcept 
#  HAS_CXX11_CONDITIONAL        - conditional type definitions

#=============================================================================
# Copyright 2011,2012 Rolf Eike Beer <eike@sf-mail.de>
# Copyright 2012 Andreas Weis
# Copyright 2014 Kaveh Vahedipour <kaveh@codeare.org>
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

#
# Each feature may have up to 3 checks, every one of them in it's own file
# FEATURE.cpp              - example that must build and return 0 when run
# FEATURE_fail.cpp         - example that must build, but may not return 0 when run
# FEATURE_fail_compile.cpp - example that must fail compilation
#
# The first one is mandatory, the latter 2 are optional and do not depend on
# each other (i.e. only one may be present).
#
# Modification for std::thread (Kaveh Vahdipour, Forschungszentrum Juelich)
#

IF (NOT CMAKE_CXX_COMPILER_LOADED)
    message(FATAL_ERROR "CheckCXX11Features modules only works if language CXX is enabled")
endif ()

cmake_minimum_required(VERSION 2.8.3)

#
### Check for needed compiler flags
#
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++11" _HAS_CXX11_FLAG)
if (NOT _HAS_CXX11_FLAG)
    check_cxx_compiler_flag("-std=c++0x" _HAS_CXX0X_FLAG)
endif ()

if (_HAS_CXX11_FLAG)
    set(CXX11_COMPILER_FLAGS "-std=c++11")
elseif (_HAS_CXX0X_FLAG)
    set(CXX11_COMPILER_FLAGS "-std=c++0x")
endif ()

function(cxx11_check_feature FEATURE_NAME RESULT_VAR)
    if (NOT DEFINED ${RESULT_VAR})
        set(_bindir "${CMAKE_CURRENT_BINARY_DIR}/cxx11/${FEATURE_NAME}")

        set(_SRCFILE_BASE ${CMAKE_CURRENT_LIST_DIR}/CheckCXX11Features/cxx11-test-${FEATURE_NAME})
        set(_LOG_NAME "\"${FEATURE_NAME}\"")
        message(STATUS "Checking C++11 support for ${_LOG_NAME}")

        set(_SRCFILE "${_SRCFILE_BASE}.cpp")
        set(_SRCFILE_FAIL "${_SRCFILE_BASE}_fail.cpp")
        set(_SRCFILE_FAIL_COMPILE "${_SRCFILE_BASE}_fail_compile.cpp")

        if (CROSS_COMPILING)
            try_compile(${RESULT_VAR} "${_bindir}" "${_SRCFILE}"
                        COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
            if (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
                try_compile(${RESULT_VAR} "${_bindir}_fail" "${_SRCFILE_FAIL}"
                            COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
            endif (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
        else (CROSS_COMPILING)
            try_run(_RUN_RESULT_VAR _COMPILE_RESULT_VAR
                    "${_bindir}" "${_SRCFILE}"
                    COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
            if (_COMPILE_RESULT_VAR AND NOT _RUN_RESULT_VAR)
                set(${RESULT_VAR} TRUE)
            else (_COMPILE_RESULT_VAR AND NOT _RUN_RESULT_VAR)
                set(${RESULT_VAR} FALSE)
            endif (_COMPILE_RESULT_VAR AND NOT _RUN_RESULT_VAR)
            if (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
                try_run(_RUN_RESULT_VAR _COMPILE_RESULT_VAR
                        "${_bindir}_fail" "${_SRCFILE_FAIL}"
                         COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
                if (_COMPILE_RESULT_VAR AND _RUN_RESULT_VAR)
                    set(${RESULT_VAR} TRUE)
                else (_COMPILE_RESULT_VAR AND _RUN_RESULT_VAR)
                    set(${RESULT_VAR} FALSE)
                endif (_COMPILE_RESULT_VAR AND _RUN_RESULT_VAR)
            endif (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
        endif (CROSS_COMPILING)
        if (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL_COMPILE})
            try_compile(_TMP_RESULT "${_bindir}_fail_compile" "${_SRCFILE_FAIL_COMPILE}"
                        COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
            if (_TMP_RESULT)
                set(${RESULT_VAR} FALSE)
            else (_TMP_RESULT)
                set(${RESULT_VAR} TRUE)
            endif (_TMP_RESULT)
        endif (${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL_COMPILE})

        if (${RESULT_VAR})
            message(STATUS "Checking C++11 support for ${_LOG_NAME}: works")
        else (${RESULT_VAR})
            message(FATAL_ERROR "Checking C++11 support for ${_LOG_NAME}: not supported")
        endif (${RESULT_VAR})
        set(${RESULT_VAR} ${${RESULT_VAR}} CACHE INTERNAL "C++11 support for ${_LOG_NAME}")
    endif (NOT DEFINED ${RESULT_VAR})
endfunction(cxx11_check_feature)

cxx11_check_feature("__func__" HAS_CXX11_FUNC)
cxx11_check_feature("auto" HAS_CXX11_AUTO)
cxx11_check_feature("auto_ret_type" HAS_CXX11_AUTO_RET_TYPE)
#cxx11_check_feature("atomic_uint_fast16_t" HAS_CXX11_ATOMIC_UINT_FAST16_T)
cxx11_check_feature("class_override_final" HAS_CXX11_CLASS_OVERRIDE)
cxx11_check_feature("constexpr" HAS_CXX11_CONSTEXPR)
cxx11_check_feature("conditional" HAS_CXX11_CONDITIONAL)
#cxx11_check_feature("cstdint" HAS_CXX11_CSTDINT_H)
cxx11_check_feature("decltype" HAS_CXX11_DECLTYPE)
cxx11_check_feature("initializer_list" HAS_CXX11_INITIALIZER_LIST)
cxx11_check_feature("lambda" HAS_CXX11_LAMBDA)
cxx11_check_feature("range_based_for_loop" HAS_CXX11_RANGE_BASED_FOR_LOOP)
#cxx11_check_feature("long_long" HAS_CXX11_LONG_LONG)
cxx11_check_feature("nullptr" HAS_CXX11_NULLPTR)
cxx11_check_feature("tuple" HAS_CXX11_TUPLE)
cxx11_check_feature("regex" HAS_CXX11_LIB_REGEX)
cxx11_check_feature("rvalue-references" HAS_CXX11_RVALUE_REFERENCES)
cxx11_check_feature("sizeof_member" HAS_CXX11_SIZEOF_MEMBER)
cxx11_check_feature("static_assert" HAS_CXX11_STATIC_ASSERT)
cxx11_check_feature("variadic_templates" HAS_CXX11_VARIADIC_TEMPLATES)
cxx11_check_feature("shared_ptr" HAS_CXX11_SHARED_PTR)
cxx11_check_feature("unique_ptr" HAS_CXX11_UNIQUE_PTR)
cxx11_check_feature("weak_ptr" HAS_CXX11_WEAK_PTR)
cxx11_check_feature("thread" HAS_CXX11_THREAD)
cxx11_check_feature("mutex" HAS_CXX11_MUTEX)
cxx11_check_feature("noexcept" HAS_CXX11_NOEXCEPT)
