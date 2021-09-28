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

// Sets macros based on HWY_TARGET.

// This include guard is toggled by foreach_target, so avoid the usual _H_
// suffix to prevent copybara from renaming it.
#if defined(HWY_SET_MACROS_PER_TARGET) == defined(HWY_TARGET_TOGGLE)
#ifdef HWY_SET_MACROS_PER_TARGET
#undef HWY_SET_MACROS_PER_TARGET
#else
#define HWY_SET_MACROS_PER_TARGET
#endif

#endif  // HWY_SET_MACROS_PER_TARGET

#include "hwy/targets.h"

#undef HWY_NAMESPACE
#undef HWY_ALIGN
#undef HWY_LANES

#undef HWY_GATHER_LANES
#undef HWY_VARIABLE_SHIFT_LANES
#undef HWY_COMPARE64_LANES
#undef HWY_MINMAX64_LANES

#undef HWY_CAP_INTEGER64
#undef HWY_CAP_FLOAT64
#undef HWY_CAP_GE256
#undef HWY_CAP_GE512

#undef HWY_TARGET_STR

// Before include guard so we redefine HWY_TARGET_STR on each include,
// governed by the current HWY_TARGET.
//-----------------------------------------------------------------------------
// SSE4
#if HWY_TARGET == HWY_SSE4

#define HWY_NAMESPACE N_SSE4
#define HWY_ALIGN alignas(16)
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_GATHER_LANES(T) 1
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_COMPARE64_LANES 2
#define HWY_MINMAX64_LANES 1

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_TARGET_STR "sse2,ssse3,sse4.1"

//-----------------------------------------------------------------------------
// AVX2
#elif HWY_TARGET == HWY_AVX2

#define HWY_NAMESPACE N_AVX2
#define HWY_ALIGN alignas(32)
#define HWY_LANES(T) (32 / sizeof(T))

#define HWY_GATHER_LANES(T) HWY_LANES(T)
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_COMPARE64_LANES 4
#define HWY_MINMAX64_LANES 1

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 1
#define HWY_CAP_GE512 0

#if defined(HWY_DISABLE_BMI2_FMA)
#define HWY_TARGET_STR "avx,avx2,f16c"
#else
#define HWY_TARGET_STR "avx,avx2,bmi,bmi2,fma,f16c"
#endif

//-----------------------------------------------------------------------------
// AVX3
#elif HWY_TARGET == HWY_AVX3

#define HWY_ALIGN alignas(64)
#define HWY_LANES(T) (64 / sizeof(T))

#define HWY_GATHER_LANES(T) HWY_LANES(T)
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_COMPARE64_LANES 8
#define HWY_MINMAX64_LANES 8

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 1
#define HWY_CAP_GE512 1

#define HWY_NAMESPACE N_AVX3

// Must include AVX2 because an AVX3 test may call AVX2 functions (e.g. when
// converting to half-vectors). HWY_DISABLE_BMI2_FMA is not relevant because if
// we have AVX3, we should also have BMI2/FMA.
#define HWY_TARGET_STR \
  "avx,avx2,bmi,bmi2,fma,f16c,avx512f,avx512vl,avx512dq,avx512bw"

//-----------------------------------------------------------------------------
// PPC8
#elif HWY_TARGET == HWY_PPC8

#define HWY_ALIGN alignas(16)
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_GATHER_LANES(T) 1
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_COMPARE64_LANES 2
#define HWY_MINMAX64_LANES 2

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_PPC8

#define HWY_TARGET_STR "altivec,vsx"

//-----------------------------------------------------------------------------
// NEON
#elif HWY_TARGET == HWY_NEON

#define HWY_ALIGN alignas(16)
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_GATHER_LANES(T) 1
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_MINMAX64_LANES 2
#define HWY_COMPARE64_LANES 2

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#ifdef __arm__
#define HWY_CAP_FLOAT64 0
#else
#define HWY_CAP_FLOAT64 1
#endif

#define HWY_NAMESPACE N_NEON

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.

//-----------------------------------------------------------------------------
// WASM
#elif HWY_TARGET == HWY_WASM

#define HWY_ALIGN alignas(16)
#define HWY_LANES(T) (16 / sizeof(T))

#define HWY_GATHER_LANES(T) 1
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
#define HWY_COMPARE64_LANES 2
#define HWY_MINMAX64_LANES 2

#define HWY_CAP_INTEGER64 0
#define HWY_CAP_FLOAT64 0
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_WASM

#define HWY_TARGET_STR "simd128"

//-----------------------------------------------------------------------------
// RVV
#elif HWY_TARGET == HWY_RVV

// RVV only requires lane alignment, not natural alignment of the entire vector,
// and the compiler already aligns builtin types, so nothing to do here.
#define HWY_ALIGN

// Arbitrary constant, not the actual lane count! Large enough that we can
// mul/div by 8 for LMUL. Value matches kMaxVectorSize, see base.h.
#define HWY_LANES(T) (4096 / sizeof(T))

#define HWY_GATHER_LANES(T) HWY_LANES(T)
#define HWY_VARIABLE_SHIFT_LANES(T) HWY_LANES(T)
// Cannot use HWY_LANES/sizeof here because these are used in an #if.
#define HWY_COMPARE64_LANES 256
#define HWY_MINMAX64_LANES 256

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_RVV

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.
// (rv64gcv is not a valid target)

//-----------------------------------------------------------------------------
// SCALAR
#elif HWY_TARGET == HWY_SCALAR

#define HWY_ALIGN
#define HWY_LANES(T) 1

#define HWY_GATHER_LANES(T) 1
#define HWY_VARIABLE_SHIFT_LANES(T) 1
#define HWY_COMPARE64_LANES 1
#define HWY_MINMAX64_LANES 1

#define HWY_CAP_INTEGER64 1
#define HWY_CAP_FLOAT64 1
#define HWY_CAP_GE256 0
#define HWY_CAP_GE512 0

#define HWY_NAMESPACE N_SCALAR

// HWY_TARGET_STR remains undefined so HWY_ATTR is a no-op.

#else
#pragma message("HWY_TARGET does not match any known target")
#endif  // HWY_TARGET

// Clang <9 requires this be invoked at file scope, before any namespace.
#undef HWY_BEFORE_NAMESPACE
#if defined(HWY_TARGET_STR)
#define HWY_BEFORE_NAMESPACE()        \
  HWY_PUSH_ATTRIBUTES(HWY_TARGET_STR) \
  static_assert(true, "For requiring trailing semicolon")
#else
// avoids compiler warning if no HWY_TARGET_STR
#define HWY_BEFORE_NAMESPACE() \
  static_assert(true, "For requiring trailing semicolon")
#endif

// Clang <9 requires any namespaces be closed before this macro.
#undef HWY_AFTER_NAMESPACE
#if defined(HWY_TARGET_STR)
#define HWY_AFTER_NAMESPACE() \
  HWY_POP_ATTRIBUTES          \
  static_assert(true, "For requiring trailing semicolon")
#else
// avoids compiler warning if no HWY_TARGET_STR
#define HWY_AFTER_NAMESPACE() \
  static_assert(true, "For requiring trailing semicolon")
#endif

#undef HWY_ATTR
#if defined(HWY_TARGET_STR) && HWY_HAS_ATTRIBUTE(target)
#define HWY_ATTR __attribute__((target(HWY_TARGET_STR)))
#else
#define HWY_ATTR
#endif
