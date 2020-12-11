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
#include <cstddef>
#include <stdint.h>
#include <math.h>
#include <string>

#include "types.hpp" // iresearch types

#if (defined(__GNUC__) && __GNUC__ == 8 && __GNUC_MINOR__ < 1)
  // protection against broken GCC 8.0 from Ubuntu 18.04 official repository
  #error "GCC 8.0 isn't officially supported (https://gcc.gnu.org/releases.html)"
#endif

////////////////////////////////////////////////////////////////////////////////
/// C++ standard
////////////////////////////////////////////////////////////////////////////////

#ifndef __cplusplus
  #error C++ is required
#endif

#define IRESEARCH_CXX_98 199711L // c++03/c++98
#define IRESEARCH_CXX_11 201103L // c++11
#define IRESEARCH_CXX_14 201402L // c++14
#define IRESEARCH_CXX_17 201703L // c++17

#if defined(_MSC_VER)
  // MSVC doesn't honor __cplusplus macro,
  // it always equals to IRESEARCH_CXX_98
  // therefore we use _MSC_VER
  #if _MSC_VER < 1910                       // before MSVC2017
    #error "at least C++17 is required"
  #elif _MSC_VER >= 1910 && _MSC_VER < 1920 // MSVC2017 and later
    #define IRESEARCH_CXX IRESEARCH_CXX_17
  #elif _MSC_VER >= 1920                    // MSVC2019 and later
    #define IRESEARCH_CXX IRESEARCH_CXX_17
  #endif
#else // GCC/Clang
  #if __cplusplus < IRESEARCH_CXX_17
    #error "at least C++17 is required"
  #endif

  #define IRESEARCH_CXX IRESEARCH_CXX_17
#endif

////////////////////////////////////////////////////////////////////////////////
/// Export/Import definitions
////////////////////////////////////////////////////////////////////////////////

// Generic helper definitions for shared library support
#if defined _MSC_VER || defined __CYGWIN__
  #define IRESEARCH_HELPER_DLL_IMPORT __declspec(dllimport)
  #define IRESEARCH_HELPER_DLL_EXPORT __declspec(dllexport)
  #define IRESEARCH_HELPER_DLL_LOCAL
  #define IRESEARCH_HELPER_TEMPLATE_IMPORT 
  #define IRESEARCH_HELPER_TEMPLATE_EXPORT

  #if _MSC_VER < 1900 // before msvc2015
    #error "compiler is not supported"
  #else
    // MSVC2017.1 - MSVC2017.7 does not correctly support alignas()
    // FIXME TODO find a workaround or do not use alignas(...) and remove definition from CMakeLists.txt
    static_assert(_MSC_VER <= 1910 || _MSC_VER >= 1916, "_MSC_VER > 1910 && _MSC_VER < 1915");
  #endif

  #define FORCE_INLINE inline __forceinline
  #define NO_INLINE __declspec(noinline)
  #define RESTRICT __restrict 
  #define IRESEARCH_IGNORE_UNUSED /* unused */
#else
  #if (defined(__GNUC__) && (__GNUC__ >= 4))
    #define IRESEARCH_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #error "compiler is not supported"
  #endif
  #define IRESEARCH_HELPER_TEMPLATE_IMPORT IRESEARCH_HELPER_DLL_IMPORT 
  #define IRESEARCH_HELPER_TEMPLATE_EXPORT IRESEARCH_HELPER_DLL_EXPORT 

  #define FORCE_INLINE inline __attribute__ ((always_inline))
  #define NO_INLINE __attribute__ ((noinline))
  #define RESTRICT __restrict__
  #define IRESEARCH_IGNORE_UNUSED __attribute__ ((unused))
#endif

// hook for GCC 8.1/8.2 optimized code
// these versions produce incorrect code when inlining optimizations are enabled
#if defined(__OPTIMIZE__) && defined(__GNUC__) \
    && ((__GNUC__ == 8 && __GNUC_MINOR__ == 1) \
        || (__GNUC__ == 8 && __GNUC_MINOR__ == 2))
  #define GCC8_12_OPTIMIZED_WORKAROUND(...) __VA_ARGS__
#else
  #define GCC8_12_OPTIMIZED_WORKAROUND(...)
#endif

// hook for MSVC2017.3-9 optimized code
// these versions produce incorrect code when inlining optimizations are enabled
// for versions @see https://github.com/lordmulder/MUtilities/blob/master/include/MUtils/Version.h
// and https://dev.to/yumetodo/list-of-mscver-and-mscfullver-8nd
// seems MSVC2019.0+ also have this problem
#if defined(_MSC_VER) \
    && !defined(_DEBUG) \
    && (((_MSC_FULL_VER >= 191125506) && (_MSC_FULL_VER <= 191125508)) \
        || ((_MSC_FULL_VER >= 191125542) && (_MSC_FULL_VER <= 191125547)) \
        || ((_MSC_FULL_VER >= 191225830) && (_MSC_FULL_VER <= 191225835)) \
        || ((_MSC_FULL_VER >= 191326128) && (_MSC_FULL_VER <= 191326132)) \
        || ((_MSC_FULL_VER >= 191426430) && (_MSC_FULL_VER <= 191426433)) \
        || ((_MSC_FULL_VER >= 191526726) && (_MSC_FULL_VER <= 191526732)) \
        || ((_MSC_FULL_VER >= 191627023) && (_MSC_FULL_VER <= 191627034)) \
        || (_MSC_FULL_VER >= 192027508))
  #define MSVC2017_3456789_MSVC2019_OPTIMIZED_WORKAROUND(...) __VA_ARGS__
#else
  #define MSVC2017_3456789_MSVC2019_OPTIMIZED_WORKAROUND(...)
#endif

// hook for MSVC-only code
#if defined(_MSC_VER)
  #define MSVC_ONLY(...) __VA_ARGS__
#else
  #define MSVC_ONLY(...)
#endif

// hook for MSVC2015-only code
#if defined(_MSC_VER) && _MSC_VER == 1900
  #define MSVC2015_ONLY(...) __VA_ARGS__
#else
  #define MSVC2015_ONLY(...)
#endif

// hook for GCC-only code
#if defined(__GNUC__)
  #define GCC_ONLY(...) __VA_ARGS__
#else
  #define GCC_ONLY(...)
#endif

// hool for Valgrind-only code
#if defined(IRESEARCH_VALGRIND)
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

// Windows uses wchar_t for unicode handling
#if defined(_WIN32)
  #define IR_NATIVE_STRING(s) L##s
#else
  #define IR_NATIVE_STRING(s) s
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
#if defined(__FUNCSIG__)
  #define IRESEARCH_CURRENT_FUNCTION __FUNCSIG__
#elif defined(__PRETTY_FUNCTION__) || defined(__GNUC__)
  #define IRESEARCH_CURRENT_FUNCTION __PRETTY_FUNCTION__
#else
  #error "compiler is not supported"
#endif

#ifndef __has_feature
  #define IRESEARCH_COMPILER_HAS_FEATURE(x) 0 // Compatibility with non-clang compilers.
#else
  #define IRESEARCH_COMPILER_HAS_FEATURE(x) __has_feature(x)
#endif

////////////////////////////////////////////////////////////////////////////////
/// SSE compatibility
////////////////////////////////////////////////////////////////////////////////

// for MSVC on x64 architecture SSE2 is always enabled
#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#define IRESEARCH_SSE2
#endif

#ifdef __SSE4_1__
#define IRESEARCH_SSE4_1
#endif

#ifdef __SSE4_2__
#define IRESEARCH_SSE4_2
#endif

#ifdef __AVX__
#define IRESEARCH_AVX
#endif

#ifdef __AVX2__
#define IRESEARCH_AVX2
#endif

////////////////////////////////////////////////////////////////////////////////

// likely/unlikely branch indicator
// macro definitions similar to the ones at
// https://kernelnewbies.org/FAQ/LikelyUnlikely
#if defined(__GNUC__) || defined(__GNUG__)
#define IRS_LIKELY(v) __builtin_expect(!!(v), 1)
#define IRS_UNLIKELY(v) __builtin_expect(!!(v), 0)
#else
#define IRS_LIKELY(v) v
#define IRS_UNLIKELY(v) v
#endif

#ifdef IRESEARCH_DEBUG
#define IRS_ASSERT(CHECK) \
    ( (CHECK) ? void(0) : []{assert(!#CHECK);}() )
#else
#define IRS_ASSERT(CHECK) void(0)
#endif

#define UNUSED(par) (void)(par)

namespace iresearch { }
namespace irs = ::iresearch;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// CMPXCHG16B requires that the destination
// (memory) operand be 16-byte aligned
#define IRESEARCH_CMPXCHG16B_ALIGNMENT 16

#endif // IRESEARCH_SHARED_H
