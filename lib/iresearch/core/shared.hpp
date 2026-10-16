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

#pragma once

#include <bit>
#include <cfloat>  // for FLT_EVAL_METHOD

#include "types.hpp"

#ifndef __cplusplus
#error C++ is required
#endif

#if defined(_MSC_VER) || defined(__CYGWIN__)
#if defined(_MSC_VER) && _MSC_VER < 1920
#error "compiler is not supported"
#endif

#define IRS_FORCE_INLINE inline __forceinline
#define IRS_NO_INLINE __declspec(noinline)
#define IRS_RESTRICT __restrict
#define IRS_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#if !((defined(__GNUC__) && (__GNUC__ >= 10)) || \
      (defined(__clang__) && (__clang_major__ >= 11)))
#error "compiler is not supported"
#endif

#ifdef IRS_DISABLE_LOG
#define IRS_FORCE_INLINE inline
#else
#define IRS_FORCE_INLINE inline __attribute__((always_inline))
#endif
#define IRS_NO_INLINE __attribute__((noinline))
#define IRS_RESTRICT __restrict__
#define IRS_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

// hook for MSVC-only code
#if defined(_MSC_VER) && !defined(__clang__)
#define MSVC_ONLY(...) __VA_ARGS__
#else
#define MSVC_ONLY(...)
#endif

// check if sizeof(float_t) == sizeof(double_t)
#if defined(FLT_EVAL_METHOD) && \
  ((FLT_EVAL_METHOD == 1) || (FLT_EVAL_METHOD == 2))
static_assert(sizeof(float_t) == sizeof(double_t));
#define FLOAT_T_IS_DOUBLE_T
#else
static_assert(sizeof(float_t) != sizeof(double_t));
#undef FLOAT_T_IS_DOUBLE_T
#endif

// Windows uses wchar_t for unicode handling
#ifdef _WIN32
#define IR_NATIVE_STRING(s) L##s
#else
#define IR_NATIVE_STRING(s) s
#endif

#ifndef IRS_FUNC_NAME
#if defined(__clang__) || defined(__GNUC__)
#define IRS_FUNC_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define IRS_FUNC_NAME __FUNCSIG__
#else
#define IRS_FUNC_NAME __func__
#endif
#endif

#ifndef __has_feature
#define IRS_HAS_FEATURE(x) 0
#else
#define IRS_HAS_FEATURE(x) __has_feature(x)
#endif

#ifndef __has_attribute
#define IRS_HAS_ATTRIBUTE(x) 0
#else
#define IRS_HAS_ATTRIBUTE(x) __has_attribute(x)
#endif

#if IRS_HAS_ATTRIBUTE(nonnull)
#define IRS_ATTRIBUTE_NONNULL(arg_index) __attribute__((nonnull(arg_index)))
#else
#define IRS_ATTRIBUTE_NONNULL(arg_index)
#endif

// for MSVC on x64 architecture SSE2 is always enabled
#if defined(__SSE2__) || defined(__ARM_NEON) || defined(__ARM_NEON__) || \
  (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64)))
#define IRESEARCH_SSE2
#endif

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

#define IRS_IGNORE(value) (void)(value)

namespace irs {

consteval bool is_big_endian() noexcept {
  return std::endian::native == std::endian::big;
}

inline constexpr size_t kCmpXChg16Align = 16;

}  // namespace irs

#define IRS_STRINGIFY(x) #x
#define IRS_TO_STRING(x) IRS_STRINGIFY(x)
