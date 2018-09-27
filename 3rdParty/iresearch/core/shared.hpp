////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_SHARED_H
#define IRESEARCH_SHARED_H

#include <cfloat>
#include <cstdlib>
#include <iostream>
#include <cstddef> //need this for wchar_t, size_t, NULL
#include <stdint.h> //need this for int32_t, etc
#include <math.h> //required for float_t
#include <string> //need to include this really early...

//////////////////////////////////////////////////////////
/// Export/Import definitions
//////////////////////////////////////////////////////////

// Generic helper definitions for shared library support
#if defined _MSC_VER || defined __CYGWIN__
  #define IRESEARCH_HELPER_DLL_IMPORT __declspec(dllimport)
  #define IRESEARCH_HELPER_DLL_EXPORT __declspec(dllexport)
  #define IRESEARCH_HELPER_DLL_LOCAL
  #define IRESEARCH_HELPER_TEMPLATE_IMPORT 
  #define IRESEARCH_HELPER_TEMPLATE_EXPORT

#if _MSC_VER < 1900 // prior the vc14    
  #define CONSTEXPR
  #define NOEXCEPT throw()
#else
  #define CONSTEXPR constexpr
  #define NOEXCEPT noexcept 
#endif

  #define ALIGNOF(v) __alignof(v)
  #define FORCE_INLINE inline __forceinline
  #define NO_INLINE __declspec(noinline)
  #define RESTRICT __restrict 
  #define ALIGNED_VALUE(_value, _type) union { _value; _type ___align; }
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define IRESEARCH_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
    #define CONSTEXPR constexpr
  #else
    #define IRESEARCH_HELPER_DLL_IMPORT
    #define IRESEARCH_HELPER_DLL_EXPORT
    #define IRESEARCH_HELPER_DLL_LOCAL
    #define CONSTEXPR const
  #endif
  #define IRESEARCH_HELPER_TEMPLATE_IMPORT IRESEARCH_HELPER_DLL_IMPORT 
  #define IRESEARCH_HELPER_TEMPLATE_EXPORT IRESEARCH_HELPER_DLL_EXPORT 

  #define NOEXCEPT noexcept
  #define ALIGNOF(v) alignof(v)
  #define FORCE_INLINE inline __attribute__ ((always_inline))
  #define NO_INLINE __attribute__ ((noinline))
  #define RESTRICT __restrict__
  #define ALIGNED_VALUE(_value, _type) _value alignas( _type );
#endif

// GCC before v5 does not implicitly call the move constructor on local values
// returned from functions, e.g. std::unique_ptr
//
// MSVC2013 doesn't support c++11 in a proper way
// sometimes it can't choose move constructor for
// move-only types while returning a value.
// The following macro tries to avoid potential
// performance problems on other compilers since
// 'return std::move(x)' prevents such compiler
// optimizations like 'copy elision'
#if (defined(__GNUC__) && (__GNUC__ < 5)) \
    || (defined(_MSC_VER) && _MSC_VER < 1900)
  #define IMPLICIT_MOVE_WORKAROUND(x) std::move(x)
#else
  #define IMPLICIT_MOVE_WORKAROUND(x) x
#endif

// hook for MSVC2017.3, MSVC2017.4 MSVC2017.5 optimized code
// these versions produce incorrect code when inlining optimizations are enabled
// for versions @see https://github.com/lordmulder/MUtilities/blob/master/include/MUtils/Version.h
#if defined(_MSC_VER) \
    && !defined(_DEBUG) \
    && (((_MSC_FULL_VER >= 191125506) && (_MSC_FULL_VER <= 191125508)) \
        || ((_MSC_FULL_VER >= 191125542) && (_MSC_FULL_VER <= 191125547)) \
        || ((_MSC_FULL_VER >= 191225830) && (_MSC_FULL_VER <= 191225835)))
  #define MSVC2017_345_OPTIMIZED_WORKAROUND(...) __VA_ARGS__
#else
  #define MSVC2017_345_OPTIMIZED_WORKAROUND(...)
#endif

// hook for MSVC-only code
#if defined(_MSC_VER)
  #define MSVC_ONLY(...) __VA_ARGS__
#else
  #define MSVC_ONLY(...)
#endif

// hook for MSVC2013-only code
#if defined(_MSC_VER) && _MSC_VER == 1800
  #define MSVC2013_ONLY(...) __VA_ARGS__
#else
  #define MSVC2013_ONLY(...)
#endif

// hook for MSVC2015-only code
#if defined(_MSC_VER) && _MSC_VER == 1900
  #define MSVC2015_ONLY(...) __VA_ARGS__
#else
  #define MSVC2015_ONLY(...)
#endif

// hook for MSVC2015 optimized-only code
#if defined(_MSC_VER) && !defined(_DEBUG) && _MSC_VER == 1900
  #define MSVC2015_OPTIMIZED_ONLY(...) __VA_ARGS__
#else
  #define MSVC2015_OPTIMIZED_ONLY(...)
#endif

// hook for MSVC2017-only code (2017.2 || 2017.3/2017.4 || 2017.5)
#if defined(_MSC_VER) \
    && (_MSC_VER == 1910 || _MSC_VER == 1911 || _MSC_VER == 1912)
  #define MSVC2017_ONLY(...) __VA_ARGS__
#else
  #define MSVC2017_ONLY(...)
#endif

// hook for GCC-only code
#if defined(__GNUC__)
  #define GCC_ONLY(...) __VA_ARGS__
#else
  #define GCC_ONLY(...)
#endif

// hool for Valgrind-only code
#if !defined(IRESEARCH_VALGRIND)
  #define VALGRIND_ONLY(...) __VA_ARGS__
#else
  #define VALGRIND_ONLY(...)
#endif

// check if sizeof(float_t) == sizeof(double_t)
#if defined(FLT_EVAL_METHOD) && ((FLT_EVAL_METHOD == 1) || (FLT_EVAL_METHOD == 2))
  static_assert(sizeof(float_t) == sizeof(double_t), "sizeof(float_t) != sizeof(double_t)");
  #define FLOAT_T_IS_DOUBLE_T
#else
  static_assert(sizeof(float_t) != sizeof(double_t), "sizeof(float_t) == sizeof(double_t)");
  #undef FLOAT_T_IS_DOUBLE_T
#endif

// IRESEARCH_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static build)
// IRESEARCH_LOCAL is used for non-api symbols.
// IRESEARCH_PLUGIN is used for public API symbols of plugin modules
#ifdef IRESEARCH_DLL
  #ifdef IRESEARCH_DLL_EXPORTS
    #define IRESEARCH_API IRESEARCH_HELPER_DLL_EXPORT    
    #define IRESEARCH_API_TEMPLATE IRESEARCH_HELPER_TEMPLATE_EXPORT 
  #else
    #define IRESEARCH_API IRESEARCH_HELPER_DLL_IMPORT
    #define IRESEARCH_API_TEMPLATE IRESEARCH_HELPER_TEMPLATE_IMPORT 
  #endif // IRESEARCH_DLL_EXPORTS
  #define IRESEARCH_API_PRIVATE_VARIABLES_BEGIN MSVC_ONLY(__pragma(warning(disable: 4251)))
  #define IRESEARCH_API_PRIVATE_VARIABLES_END MSVC_ONLY(__pragma(warning(default: 4251)))
  #define IRESEARCH_LOCAL IRESEARCH_HELPER_DLL_LOCAL
  #ifdef IRESEARCH_DLL_PLUGIN
    #define IRESEARCH_PLUGIN IRESEARCH_HELPER_DLL_EXPORT
    #define IRESEARCH_PLUGIN_EXPORT extern "C" IRESEARCH_HELPER_DLL_EXPORT
  #else
    #define IRESEARCH_PLUGIN IRESEARCH_HELPER_DLL_IMPORT
    #define IRESEARCH_PLUGIN_EXPORT
  #endif // IRESEARCH_DLL_PLUGIN
  #define IRESEARCH_TEMPLATE_EXPORT(x) template IRESEARCH_API x
  #define IRESEARCH_TEMPLATE_IMPORT(x) extern template x
#else // IRESEARCH_DLL is not defined: this means IRESEARCH is a static lib.
  #define IRESEARCH_API 
  #define IRESEARCH_API_TEMPLATE
  #define IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  #define IRESEARCH_API_PRIVATE_VARIABLES_END
  #define IRESEARCH_LOCAL
  #define IRESEARCH_PLUGIN
  #define IRESEARCH_TEMPLATE_EXPORT(x)
  #define IRESEARCH_TEMPLATE_IMPORT(x) 
#endif // IRESEARCH_DLL

// MSVC 2015 does not define __cpp_lib_generic_associative_lookup macro
#if (defined(__cpp_lib_generic_associative_lookup) || (defined(_MSC_VER) && _MSC_VER >= 1900))
  #define IRESEARCH_GENERIC_ASSOCIATIVE_LOOKUP
#endif

#if defined(__GNUC__)
  #define IRESEARCH_INIT(f) \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
  #define RESEARCH_FINI(f) \
    static void f(void) __attribute__((destructor)); \
    static void f(void)
#elif defined(_MSC_VER)
  #define IRESEARCH_INIT(f) \
    static void __cdecl f(void); \
    static int f ## _init_wrapper(void) { f(); return 0; } \
    __declspec(allocate(".CRT$XCU")) void (__cdecl*f##_)(void) = f ## _init_wrapper; \
    static void __cdecl f(void)
  #define RESEARCH_FINI(f) \
    static void __cdecl f(void); \
    static int f ## _fini_wrapper(void) { atexit(f); return 0; } \
    __declspec(allocate(".CRT$XCU")) static int (__cdecl*f##_)(void) = f ## _fini_wrapper; \
    static void __cdecl f(void)
#endif

// define function name used for pretty printing
// NOTE: the alias points to a compile time finction not a preprocessor macro
#if defined(__GNUC__)
  #define CURRENT_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
  #define CURRENT_FUNCTION __FUNCSIG__
#endif

#ifndef __has_feature
  #define IRESEARCH_COMPILER_HAS_FEATURE(x) 0 // Compatibility with non-clang compilers.
#else
  #define IRESEARCH_COMPILER_HAS_FEATURE(x) __has_feature(x)
#endif

//////////////////////////////////////////////////////////

#define UNUSED(par) (void)(par)

#ifndef AGB_IGNORE_UNUSED
#if defined(__GNUC__)
#define ADB_IGNORE_UNUSED __attribute__((unused))
#else
#define ADB_IGNORE_UNUSED /* unused */
#endif
#endif

#define NS_BEGIN(ns) namespace ns {
#define NS_LOCAL namespace {
#define NS_ROOT NS_BEGIN(iresearch)
#define NS_END }

NS_ROOT NS_END // ROOT namespace predeclaration
namespace irs = ::iresearch;

#define ASSERT( cond, mess ) assert( (cond) && (mess) )

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include "types.hpp" // iresearch types

#endif // IRESEACH_SHARED_H
