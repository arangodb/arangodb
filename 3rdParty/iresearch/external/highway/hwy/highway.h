// Copyright 2020 Google LLC
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

// This include guard is checked by foreach_target, so avoid the usual _H_
// suffix to prevent copybara from renaming it. NOTE: ops/*-inl.h are included
// after/outside this include guard.
#ifndef HWY_HIGHWAY_INCLUDED
#define HWY_HIGHWAY_INCLUDED

// Main header required before using vector types.

#include "hwy/base.h"
#include "hwy/targets.h"

namespace hwy {

// API version (https://semver.org/)
#define HWY_MAJOR 0
#define HWY_MINOR 11
#define HWY_PATCH 1

//------------------------------------------------------------------------------
// Shorthand for descriptors (defined in shared-inl.h) used to select overloads.

// Because Highway functions take descriptor and/or vector arguments, ADL finds
// these functions without requiring users in project::HWY_NAMESPACE to
// qualify Highway functions with hwy::HWY_NAMESPACE. However, ADL rules for
// templates require `using hwy::HWY_NAMESPACE::ShiftLeft;` etc. declarations.

// HWY_FULL(T[,LMUL=1]) is a native vector/group. LMUL is the number of
// registers in the group, and is ignored on targets that do not support groups.
#define HWY_FULL1(T) hwy::HWY_NAMESPACE::Simd<T, HWY_LANES(T)>
#define HWY_3TH_ARG(arg1, arg2, arg3, ...) arg3
// Workaround for MSVC grouping __VA_ARGS__ into a single argument
#define HWY_FULL_RECOMPOSER(args_with_paren) HWY_3TH_ARG args_with_paren
// Trailing comma avoids -pedantic false alarm
#define HWY_CHOOSE_FULL(...) \
  HWY_FULL_RECOMPOSER((__VA_ARGS__, HWY_FULL2, HWY_FULL1, ))
#define HWY_FULL(...) HWY_CHOOSE_FULL(__VA_ARGS__())(__VA_ARGS__)

// Vector of up to MAX_N lanes.
#define HWY_CAPPED(T, MAX_N) \
  hwy::HWY_NAMESPACE::Simd<T, HWY_MIN(MAX_N, HWY_LANES(T))>

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
#elif HWY_STATIC_TARGET == HWY_RVV
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_RVV::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_WASM
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_WASM::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_NEON
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_NEON::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_PPC8
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_PPC8::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_SSE4
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_SSE4::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX2
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX2::FUNC_NAME
#elif HWY_STATIC_TARGET == HWY_AVX3
#define HWY_STATIC_DISPATCH(FUNC_NAME) N_AVX3::FUNC_NAME
#endif

// Dynamic dispatch declarations.

template <typename RetType, typename... Args>
struct FunctionCache {
 public:
  typedef RetType(FunctionType)(Args...);

  // A template function that when instantiated has the same signature as the
  // function being called. This function initializes the global cache of the
  // current supported targets mask used for dynamic dispatch and calls the
  // appropriate function. Since this mask used for dynamic dispatch is a
  // global cache, all the highway exported functions, even those exposed by
  // different modules, will be initialized after this function runs for any one
  // of those exported functions.
  template <FunctionType* const table[]>
  static RetType ChooseAndCall(Args... args) {
    // If we are running here it means we need to update the chosen target.
    chosen_target.Update();
    return (table[chosen_target.GetIndex()])(args...);
  }
};

// Factory function only used to infer the template parameters RetType and Args
// from a function passed to the factory.
template <typename RetType, typename... Args>
FunctionCache<RetType, Args...> FunctionCacheFactory(RetType (*)(Args...)) {
  return FunctionCache<RetType, Args...>();
}

// HWY_CHOOSE_*(FUNC_NAME) expands to the function pointer for that target or
// nullptr is that target was not compiled.
#if HWY_TARGETS & HWY_SCALAR
#define HWY_CHOOSE_SCALAR(FUNC_NAME) &N_SCALAR::FUNC_NAME
#else
// When scalar is not present and we try to use scalar because other targets
// were disabled at runtime we fall back to the baseline with
// HWY_STATIC_DISPATCH()
#define HWY_CHOOSE_SCALAR(FUNC_NAME) &HWY_STATIC_DISPATCH(FUNC_NAME)
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

#if HWY_TARGETS & HWY_NEON
#define HWY_CHOOSE_NEON(FUNC_NAME) &N_NEON::FUNC_NAME
#else
#define HWY_CHOOSE_NEON(FUNC_NAME) nullptr
#endif

#if HWY_TARGETS & HWY_PPC8
#define HWY_CHOOSE_PCC8(FUNC_NAME) &N_PPC8::FUNC_NAME
#else
#define HWY_CHOOSE_PPC8(FUNC_NAME) nullptr
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

#define HWY_DISPATCH_TABLE(FUNC_NAME) \
  HWY_CONCAT(FUNC_NAME, HighwayDispatchTable)

// HWY_EXPORT(FUNC_NAME); expands to a static array that is used by
// HWY_DYNAMIC_DISPATCH() to call the appropriate function at runtime. This
// static array must be defined at the same namespace level as the function
// it is exporting.
// After being exported, it can be called from other parts of the same source
// file using HWY_DYNAMIC_DISTPATCH(), in particular from a function wrapper
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
#define HWY_EXPORT(FUNC_NAME)                                       \
  HWY_MAYBE_UNUSED static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME)) \
      const HWY_DISPATCH_TABLE(FUNC_NAME)[1] = {                    \
          &HWY_STATIC_DISPATCH(FUNC_NAME)}
#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) HWY_STATIC_DISPATCH(FUNC_NAME)

#else

// Dynamic dispatch case with one entry per dynamic target plus the scalar
// mode and the initialization wrapper.
#define HWY_EXPORT(FUNC_NAME)                                              \
  static decltype(&HWY_STATIC_DISPATCH(FUNC_NAME))                         \
      const HWY_DISPATCH_TABLE(FUNC_NAME)[HWY_MAX_DYNAMIC_TARGETS + 2] = { \
          /* The first entry in the table initializes the global cache and \
           * calls the appropriate function. */                            \
          &decltype(hwy::FunctionCacheFactory(&HWY_STATIC_DISPATCH(        \
              FUNC_NAME)))::ChooseAndCall<HWY_DISPATCH_TABLE(FUNC_NAME)>,  \
          HWY_CHOOSE_TARGET_LIST(FUNC_NAME),                               \
          HWY_CHOOSE_SCALAR(FUNC_NAME),                                    \
  }
#define HWY_DYNAMIC_DISPATCH(FUNC_NAME) \
  (*(HWY_DISPATCH_TABLE(FUNC_NAME)[hwy::chosen_target.GetIndex()]))

#endif  // HWY_IDE || ((HWY_TARGETS & (HWY_TARGETS - 1)) == 0)

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

#undef HWY_FULL2
#if HWY_TARGET == HWY_RVV
#define HWY_FULL2(T, LMUL) hwy::HWY_NAMESPACE::Simd<T, HWY_LANES(T) * (LMUL)>
#else
#define HWY_FULL2(T, LMUL) hwy::HWY_NAMESPACE::Simd<T, HWY_LANES(T)>
#endif

// These define ops inside namespace hwy::HWY_NAMESPACE.
#if HWY_TARGET == HWY_SSE4
#include "hwy/ops/x86_128-inl.h"
#elif HWY_TARGET == HWY_AVX2
#include "hwy/ops/x86_256-inl.h"
#elif HWY_TARGET == HWY_AVX3
#include "hwy/ops/x86_512-inl.h"
#elif HWY_TARGET == HWY_PPC8
#elif HWY_TARGET == HWY_NEON
#include "hwy/ops/arm_neon-inl.h"
#elif HWY_TARGET == HWY_WASM
#include "hwy/ops/wasm_128-inl.h"
#elif HWY_TARGET == HWY_RVV
#include "hwy/ops/rvv-inl.h"
#elif HWY_TARGET == HWY_SCALAR
#include "hwy/ops/scalar-inl.h"
#else
#pragma message("HWY_TARGET does not match any known target")
#endif  // HWY_TARGET

// Commonly used functions/types that must come after ops are defined.
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// The lane type of a vector type, e.g. float for Vec<Simd<float, 4>>.
template <class V>
using LaneType = decltype(GetLane(V()));

// Vector type, e.g. Vec128<float> for Simd<float, 4>. Useful as the return type
// of functions that do not take a vector argument, or as an argument type if
// the function only has a template argument for D, or for explicit type names
// instead of auto. This may be a built-in type.
template <class D>
using Vec = decltype(Zero(D()));

// Mask type. Useful as the return type of functions that do not take a mask
// argument, or as an argument type if the function only has a template argument
// for D, or for explicit type names instead of auto.
template <class D>
using Mask = decltype(MaskFromVec(Zero(D())));

// Returns the closest value to v within [lo, hi].
template <class V>
HWY_API V Clamp(const V v, const V lo, const V hi) {
  return Min(Max(lo, v), hi);
}

// CombineShiftRightBytes (and ..Lanes) are not available for the scalar target.
// TODO(janwas): implement for RVV
#if HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_RVV

template <size_t kLanes, class V>
HWY_API V CombineShiftRightLanes(const V hi, const V lo) {
  return CombineShiftRightBytes<kLanes * sizeof(LaneType<V>)>(hi, lo);
}

#endif

// Returns lanes with the most significant bit set and all other bits zero.
template <class D>
HWY_API Vec<D> SignBit(D d) {
  using Unsigned = MakeUnsigned<TFromD<D>>;
  const Unsigned bit = Unsigned(1) << (sizeof(Unsigned) * 8 - 1);
  return BitCast(d, Set(Rebind<Unsigned, D>(), bit));
}

// Returns quiet NaN.
template <class D>
HWY_API Vec<D> NaN(D d) {
  const RebindToSigned<D> di;
  // LimitsMax sets all exponent and mantissa bits to 1. The exponent plus
  // mantissa MSB (to indicate quiet) would be sufficient.
  return BitCast(d, Set(di, LimitsMax<TFromD<decltype(di)>>()));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HWY_HIGHWAY_PER_TARGET
