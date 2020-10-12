//
// immer: immutable data structures for C++
// Copyright (C) 2016, 2017, 2018 Juan Pedro Bolivar Puente
//
// This software is distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://boost.org/LICENSE_1_0.txt
//

#pragma once

#if defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)
#define IMMER_NODISCARD [[nodiscard]]
#elif defined(__has_cpp_attribute) && __has_cpp_attribute(gnu::warn_unused_result)
// This seems to be causing problems in GCC 6, where when used, methods
// overloads based on the value category (&, &&) are detected as ambiguous
//   #define IMMER_NODISCARD [[gnu::warn_unused_result]]
#define IMMER_NODISCARD
#else
#define IMMER_NODISCARD
#endif

#ifndef IMMER_DEBUG_TRACES
#define IMMER_DEBUG_TRACES 0
#endif

#ifndef IMMER_DEBUG_PRINT
#define IMMER_DEBUG_PRINT 0
#endif

#ifndef IMMER_DEBUG_DEEP_CHECK
#define IMMER_DEBUG_DEEP_CHECK 0
#endif

#if IMMER_DEBUG_TRACES || IMMER_DEBUG_PRINT
#include <iostream>
#include <prettyprint.hpp>
#endif

#if IMMER_DEBUG_TRACES
#define IMMER_TRACE(...) std::cout << __VA_ARGS__ << std::endl
#else
#define IMMER_TRACE(...)
#endif
#define IMMER_TRACE_F(...)                                              \
    IMMER_TRACE(__FILE__ << ":" << __LINE__ << ": " << __VA_ARGS__)
#define IMMER_TRACE_E(expr)                             \
    IMMER_TRACE("    " << #expr << " = " << (expr))

#if defined(_MSC_VER)
#define IMMER_UNREACHABLE    __assume(false)
#define IMMER_LIKELY(cond)   cond
#define IMMER_UNLIKELY(cond) cond
#define IMMER_FORCEINLINE    __forceinline
#define IMMER_PREFETCH(p)
#else
#define IMMER_UNREACHABLE    __builtin_unreachable()
#define IMMER_LIKELY(cond)   __builtin_expect(!!(cond), 1)
#define IMMER_UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#define IMMER_FORCEINLINE    inline __attribute__ ((always_inline))
#define IMMER_PREFETCH(p)
// #define IMMER_PREFETCH(p)    __builtin_prefetch(p)
#endif

#define IMMER_DESCENT_DEEP 0

#ifdef NDEBUG
#define IMMER_ENABLE_DEBUG_SIZE_HEAP 0
#else
#define IMMER_ENABLE_DEBUG_SIZE_HEAP 1
#endif

namespace immer {

const auto default_bits = 5;
const auto default_free_list_size = 1 << 10;

} // namespace immer
