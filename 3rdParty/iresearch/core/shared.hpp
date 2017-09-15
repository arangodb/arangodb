//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
// Export/Import definition
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
  #if __GNUC__ >= 4
    #define IRESEARCH_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define IRESEARCH_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
    #define	CONSTEXPR constexpr
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

// hook for MSVC-only code
#if defined(_MSC_VER)
  #define MSVC_ONLY(...) __VA_ARGS__
#else
  #define MSVC_ONLY(...)
#endif

// hook for GCC-only code
#if defined(__GNUC__)
  #define GCC_ONLY(...) __VA_ARGS__
#else
  #define GCC_ONLY(...)
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

//////////////////////////////////////////////////////////

#define UNUSED(par) (void)(par)

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
