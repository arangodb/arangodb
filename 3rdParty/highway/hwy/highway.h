// Copyright 2020 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Main header required before using vector types.

// IWYU pragma: begin_exports
// Export does not seem to be recursive, so re-export these (also in base.h)
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/detect_compiler_arch.h"
#include "hwy/highway_export.h"
#include "hwy/targets.h"
// IWYU pragma: end_exports

// This include guard is checked by foreach_target, so avoid the usual _H_
// suffix to prevent copybara from renaming it. NOTE: ops/*-inl.h are included
// after/outside this include guard.
#ifndef HWY_HIGHWAY_INCLUDED
#define HWY_HIGHWAY_INCLUDED

namespace hwy {

// API version (https://semver.org/); keep in sync with CMakeLists.txt.
#define HWY_MAJOR 1
#define HWY_MINOR 0
#define HWY_PATCH 4

//------------------------------------------------------------------------------
// Shorthand for tags (defined in shared-inl.h) used to select overloads.
// Note that ScalableTag<T> is preferred over HWY_FULL, and CappedTag<T, N> over
// HWY_CAPPED(T, N).

// HWY_FULL(T[,LMUL=1]) is a native vector/group. LMUL is the number of
// registers in the group, and is ignored on targets that do not support groups.
#define HWY_FULL1(T) hwy::HWY_NAMESPACE::ScalableTag<T>
#define HWY_FULL2(T, LMUL) \
  hwy::HWY_NAMESPACE::ScalableTag<T, hwy::CeilLog2(HWY_MAX(0, LMUL))>
#define HWY_3TH_ARG(arg1, arg2, arg3, ...) arg3
// Workaround for MSVC grouping __VA_ARGS__ into a single argument
#define HWY_FULL_RECOMPOSER(args_with_paren) HWY_3TH_ARG args_with_paren
// Trailing comma avoids -pedantic false alarm
#define HWY_CHOOSE_FULL(...) \
  HWY_FULL_RECOMPOSER((__VA_ARGS__, HWY_FULL2, HWY_FULL1, ))
#define HWY_FULL(...) HWY_CHOOSE_FULL(__VA_ARGS__())(__VA_ARGS__)

// Vector of up to MAX_N lanes. It's better to use full vectors where possible.
#define HWY_CAPPED(T, MAX_N) hwy::HWY_NAMESPACE::CappedTag<T, MAX_N>

//------------------------------------------------------------------------------
// Export user functions for static/dynamic dispatch

// Evaluates to 0 inside a translation unit if it is generating anything but the
// static target (the last one if multiple targets are enabled). Used to prevent
// redefinitions of HWY_EXPORT. Unless foreach_target.h is included, we only
// compile once anyway, so this is 1 unless it is or has been included.
#ifndef HWY_ONCE
#define HWY_ONCE 1
#endif

// HWY_STATIC_DISPATCH(FUNC_NAME) is the namespace-qualified FUNC_NAME for
// HWY_STATIC_TARGET (the only defined namespace unless HWY_TARGET_INCLUDE is
// defined), and can be used to deduce the return type of Choose*.
#if HWY_STATIC_TARGET == HWY_SCALAR
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SCALAR::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_EMU128
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_EMU128::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_RVV
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_RVV::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_WASM_EMU256
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_WASM_EMU256::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_WASM
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_WASM::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_NEON_WITHOUT_AES
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_NEON_WITHOUT_AES::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_NEON
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_NEON::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SVE
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SVE::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SVE2
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SVE2::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SVE_256
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SVE_256::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SVE2_128
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SVE2_128::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_PPC8
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_PPC8::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_PPC9
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_PPC9::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_PPC10
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_PPC10::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SSE2
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SSE2::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SSSE3
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SSSE3::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SSE4
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SSE4::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX2
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX2::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX3
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX3::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX3_DL
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX3_DL::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX3_ZEN4
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX3_ZEN4::FUNC_NAME
#endif

// HWY_CHOOSE_*(FUNC_NAME) expands to the function pointer for that target or
// nullptr is that target was not compiled.
#if HWY_TARGETS & HWY_EMU128
#define HWY_CHOOSE_FALLBACK(FUNC_NAME) &N_EMU128::FUNC_NAME
#elif HWY_TARGETS & HWY_SCALAR
#define HWY_CHOOSE_FALLBACK(FUNC_NAME) &N_SCALAR::FUNC_NAME
#else
// When HWY_SCALAR/HWY_EMU128 are not present and other targets were disabled at
// runtime, fall back to the baseline with HWY_STATIC_DISPATCH().
#define HWY_CHOOSE_FALLBACK(FUNC_NAME) &HWY_STATIC_DISPATCH(FUNC_NAME)
#endif

#if HWY_TARGETS & HWY_WASM_EMU256
#define HWY_CHOOSE_WASM_EMU256(FUNC_NAME) &N_WASM_EMU256::FUNC_NAME
#else
#define HWY_CHOOSE_WASM_EMU256(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_WASM
#define HWY_CHOOSE_WASM(FUNC_NAME) &N_WASM::FUNC_NAME
#else
#define HWY_CHOOSE_WASM(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_RVV
#define HWY_CHOOSE_RVV(FUNC_NAME) &N_RVV::FUNC_NAME
#else
#define HWY_CHOOSE_RVV(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
#define HWY_CHOOSE_NEON_WITHOUT_AES(FUNC_NAME) &N_NEON_WITHOUT_AES::FUNC_NAME
#else
#define HWY_CHOOSE_NEON_WITHOUT_AES(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_NEON
#define HWY_CHOOSE_NEON(FUNC_NAME) &N_NEON::FUNC_NAME
#else
#define HWY_CHOOSE_NEON(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SVE
#define HWY_CHOOSE_SVE(FUNC_NAME) &N_SVE::FUNC_NAME
#else
#define HWY_CHOOSE_SVE(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SVE2
#define HWY_CHOOSE_SVE2(FUNC_NAME) &N_SVE2::FUNC_NAME
#else
#define HWY_CHOOSE_SVE2(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SVE_256
#define HWY_CHOOSE_SVE_256(FUNC_NAME) &N_SVE_256::FUNC_NAME
#else
#define HWY_CHOOSE_SVE_256(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SVE2_128
#define HWY_CHOOSE_SVE2_128(FUNC_NAME) &N_SVE2_128::FUNC_NAME
#else
#define HWY_CHOOSE_SVE2_128(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_PPC8
#define HWY_CHOOSE_PPC8(FUNC_NAME) &N_PPC8::FUNC_NAME
#else
#define HWY_CHOOSE_PPC8(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_PPC9
#define HWY_CHOOSE_PPC9(FUNC_NAME) &N_PPC9::FUNC_NAME
#else
#define HWY_CHOOSE_PPC9(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_PPC10
#define HWY_CHOOSE_PPC10(FUNC_NAME) &N_PPC10::FUNC_NAME
#else
#define HWY_CHOOSE_PPC10(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SSE2
#define HWY_CHOOSE_SSE2(FUNC_NAME) &N_SSE2::FUNC_NAME
#else
#define HWY_CHOOSE_SSE2(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SSSE3
#define HWY_CHOOSE_SSSE3(FUNC_NAME) &N_SSSE3::FUNC_NAME
#else
#define HWY_CHOOSE_SSSE3(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_SSE4
#define HWY_CHOOSE_SSE4(FUNC_NAME) &N_SSE4::FUNC_NAME
#else
#define HWY_CHOOSE_SSE4(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_AVX2
#define HWY_CHOOSE_AVX2(FUNC_NAME) &N_AVX2::FUNC_NAME
#else
#define HWY_CHOOSE_AVX2(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_AVX3
#define HWY_CHOOSE_AVX3(FUNC_NAME) &N_AVX3::FUNC_NAME
#else
#define HWY_CHOOSE_AVX3(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_AVX3_DL
#define HWY_CHOOSE_AVX3_DL(FUNC_NAME) &N_AVX3_DL::FUNC_NAME
#else
#define HWY_CHOOSE_AVX3_DL(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_AVX3_ZEN4
#define HWY_CHOOSE_AVX3_ZEN4(FUNC_NAME) &N_AVX3_ZEN4::FUNC_NAME
#else
#define HWY_CHOOSE_AVX3_ZEN4(FUNC_NAME) nullptr
#endif

// MSVC 2017 workaround: the non-type template parameter to ChooseAndCall
// apparently cannot be an array. Use a function pointer instead, which has the
// disadvantage that we call the static (not best) target on the first call to
// any HWY_DYNAMIC_DISPATCH.
#if HWY_COMPILER_MSVC && HWY_COMPILER_MSVC < 1915
#define HWY_DISPATCH_WORKAROUND 1
#else
#define HWY_DISPATCH_WORKAROUND 0
#endif

// Provides a static member function which is what is called during the first
// HWY_DYNAMIC_DISPATCH, where GetIndex is still zero, and instantiations of
// this function are the first entry in the tables created by HWY_EXPORT.
template <typename RetType, typename... Args>
struct FunctionCache {
 public:
  typedef RetType(FunctionType)(Args...);

#if HWY_DISPATCH_WORKAROUND
  template <FunctionType* const func>
  static RetType ChooseAndCall(Args... args) {
    ChosenTarget& chosen_target = GetChosenTarget();
    chosen_target.Update(SupportedTargets());
    return (*func)(args...);
  }
#else
  // A template function that when instantiated has the same signature as the
  // function being called. This function initializes the bit array of targets
  // supported by the current CPU and then calls the appropriate entry within
  // the HWY_EXPORT table. Subsequent calls via HWY_DYNAMIC_DISPATCH to any
  // exported functions, even those defined by different translation units,
  // will dispatch directly to the best available target.
  template <FunctionType* const table[]>
  static RetType ChooseAndCall(Args... args) {
    ChosenTarget& chosen_target = GetChosenTarget();
    chosen_target.Update(SupportedTargets());
    return (table[chosen_target.GetIndex()])(args...);
  }
#endif  // HWY_DISPATCH_WORKAROUND
};

// Used to deduce the template parameters RetType and Args from a function.
template <typename RetType, typename... Args>
FunctionCache<RetType, Args...> DeduceFunctionCache(RetType (*)(Args...)) {
  return FunctionCache<RetType, Args...>();
}

#define HWY_DISPATCH_TABLE(FUNC_NAME) \
  HWY_CONCAT(FUNC_NAME, HighwayDispatchTable)

// HWY_EXPORT(FUNC_NAME); expands to a static array that is used by
// HWY_DYNAMIC_DISPATCH() to call the appropriate function at runtime. This
// static array must be defined at the same namespace level as the function
// it is exporting.
// After being exported, it can be called from other parts of the same source
// file using HWY_DYNAMIC_DISPATCH(), in particular from a function wrapper
// like in the following example:
//
//   #include "hwy/highway.h"
//   HWY_BEFORE_NAMESPACE();
//   namespace skeleton {
//   namespace HWY_NAMESPACE {
//
//   void MyFunction(int a, char b, const char* c) { ... }
//
//   // NOLINTNEXTLINE(google-readability-namespace-comments)
//   }  // namespace HWY_NAMESPACE
//   }  // namespace skeleton
//   HWY_AFTER_NAMESPACE();
//
//   namespace skeleton {
//   HWY_EXPORT(MyFunction);  // Defines the dispatch table in this scope.
//
//   void MyFunction(int a, char b, const char* c) {
//     return HWY_DYNAMIC_DISPATCH(MyFunction)(a, b, c);
//   }
//   }  // namespace skeleton
//

#if HWY_IDE || ((HWY_TARGETS & (HWY_TARGETS - 1)) == 0)

// Simplified version for IDE or the dynamic dispatch case with only one target.
// This case still uses a table, although of a single element, to provide the
// same compile error conditions as with the dynamic dispatch case when multiple
// targets are being compiled.
#define HWY_EXPORT(FUNC_NAME)                                             \
  HWY_MAYBE_UNUSED static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME)) const \
  HWY_DISPATCH_TABLE(FUNC_NAME)[1] = {&HWY_STATIC_DISPATCH(FUNC_NAME)}
#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) HWY_STATIC_DISPATCH(FUNC_NAME)

#else

// Simplified version for MSVC 2017: function pointer instead of table.
#if HWY_DISPATCH_WORKAROUND

#define HWY_EXPORT(FUNC_NAME)                                                \
  static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME)) const HWY_DISPATCH_TABLE( \
      FUNC_NAME)[HWY_MAX_DYNAMIC_TARGETS + 2] = {                            \
      /* The first entry in the table initializes the global cache and       \
       * calls the function from HWY_STATIC_TARGET. */                       \
      &decltype(hwy::DeduceFunctionCache(&HWY_STATIC_DISPATCH(               \
          FUNC_NAME)))::ChooseAndCall<&HWY_STATIC_DISPATCH(FUNC_NAME)>,      \
      HWY_CHOOSE_TARGET_LIST(FUNC_NAME),                                     \
      HWY_CHOOSE_FALLBACK(FUNC_NAME),                                        \
  }

#else

// Dynamic dispatch case with one entry per dynamic target plus the fallback
// target and the initialization wrapper.
#define HWY_EXPORT(FUNC_NAME)                                                \
  static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME)) const HWY_DISPATCH_TABLE( \
      FUNC_NAME)[HWY_MAX_DYNAMIC_TARGETS + 2] = {                            \
      /* The first entry in the table initializes the global cache and       \
       * calls the appropriate function. */                                  \
      &decltype(hwy::DeduceFunctionCache(&HWY_STATIC_DISPATCH(               \
          FUNC_NAME)))::ChooseAndCall<HWY_DISPATCH_TABLE(FUNC_NAME)>,        \
      HWY_CHOOSE_TARGET_LIST(FUNC_NAME),                                     \
      HWY_CHOOSE_FALLBACK(FUNC_NAME),                                        \
  }

#endif  // HWY_DISPATCH_WORKAROUND

#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) \
  (*(HWY_DISPATCH_TABLE(FUNC_NAME)[hwy::GetChosenTarget().GetIndex()]))

#endif  // HWY_IDE || ((HWY_TARGETS & (HWY_TARGETS - 1)) == 0)

// DEPRECATED names; please use HWY_HAVE_* instead.
#define HWY_CAP_INTEGER64 HWY_HAVE_INTEGER64
#define HWY_CAP_FLOAT16 HWY_HAVE_FLOAT16
#define HWY_CAP_FLOAT64 HWY_HAVE_FLOAT64

}  // namespace hwy

#endif  // HWY_HIGHWAY_INCLUDED

//------------------------------------------------------------------------------

// NOTE: the following definitions and ops/*.h depend on HWY_TARGET, so we want
// to include them once per target, which is ensured by the toggle check.
// Because ops/*.h are included under it, they do not need their own guard.
#if defined(HWY_HIGHWAY_PER_TARGET) == defined(HWY_TARGET_TOGGLE)
#ifdef HWY_HIGHWAY_PER_TARGET
#undef HWY_HIGHWAY_PER_TARGET
#else
#define HWY_HIGHWAY_PER_TARGET
#endif

// These define ops inside namespace hwy::HWY_NAMESPACE.
#if HWY_TARGET == HWY_SSE2 || HWY_TARGET == HWY_SSSE3 || \
    HWY_TARGET == HWY_SSE4
#include "hwy/ops/x86_128-inl.h"
#elif HWY_TARGET == HWY_AVX2
#include "hwy/ops/x86_256-inl.h"
#elif HWY_TARGET == HWY_AVX3 || HWY_TARGET == HWY_AVX3_DL || \
    HWY_TARGET == HWY_AVX3_ZEN4
#include "hwy/ops/x86_512-inl.h"
#elif HWY_TARGET == HWY_PPC8 || HWY_TARGET == HWY_PPC9 || \
    HWY_TARGET == HWY_PPC10
#include "hwy/ops/ppc_vsx-inl.h"
#elif HWY_TARGET == HWY_NEON || HWY_TARGET == HWY_NEON_WITHOUT_AES
#include "hwy/ops/arm_neon-inl.h"
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2 || \
    HWY_TARGET == HWY_SVE_256 || HWY_TARGET == HWY_SVE2_128
#include "hwy/ops/arm_sve-inl.h"
#elif HWY_TARGET == HWY_WASM_EMU256
#include "hwy/ops/wasm_256-inl.h"
#elif HWY_TARGET == HWY_WASM
#include "hwy/ops/wasm_128-inl.h"
#elif HWY_TARGET == HWY_RVV
#include "hwy/ops/rvv-inl.h"
#elif HWY_TARGET == HWY_EMU128
#include "hwy/ops/emu128-inl.h"
#elif HWY_TARGET == HWY_SCALAR
#include "hwy/ops/scalar-inl.h"
#else
#pragma message("HWY_TARGET does not match any known target")
#endif  // HWY_TARGET

#include "hwy/ops/generic_ops-inl.h"

#endif  // HWY_HIGHWAY_PER_TARGET
