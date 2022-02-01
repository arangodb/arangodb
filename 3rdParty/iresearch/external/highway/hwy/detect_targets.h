// Copyright 2021 Google LLC
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

#ifndef HIGHWAY_HWY_DETECT_TARGETS_H_
#define HIGHWAY_HWY_DETECT_TARGETS_H_

// Defines targets and chooses which to enable.

#include "hwy/detect_compiler_arch.h"

//------------------------------------------------------------------------------
// Optional configuration

// See ../quick_reference.md for documentation of these macros.

// Uncomment to override the default baseline determined from predefined macros:
// #define HWY_BASELINE_TARGETS (HWY_SSE4 | HWY_SCALAR)

// Uncomment to override the default blocklist:
// #define HWY_BROKEN_TARGETS HWY_AVX3

// Uncomment to definitely avoid generating those target(s):
// #define HWY_DISABLED_TARGETS HWY_SSE4

// Uncomment to avoid emitting BMI/BMI2/FMA instructions (allows generating
// AVX2 target for VMs which support AVX2 but not the other instruction sets)
// #define HWY_DISABLE_BMI2_FMA

//------------------------------------------------------------------------------
// Targets

// Unique bit value for each target. A lower value is "better" (e.g. more lanes)
// than a higher value within the same group/platform - see HWY_STATIC_TARGET.
//
// All values are unconditionally defined so we can test HWY_TARGETS without
// first checking the HWY_ARCH_*.
//
// The C99 preprocessor evaluates #if expressions using intmax_t types, so we
// can use 32-bit literals.

// 1,2: reserved

// Currently satisfiable by Ice Lake (VNNI, VPCLMULQDQ, VBMI2, VAES). Later to
// be added: BF16 (Cooper Lake). VP2INTERSECT is only in Tiger Lake? We do not
// yet have uses for VBMI, VPOPCNTDQ, BITALG, GFNI.
#define HWY_AVX3_DL 4  // see HWY_WANT_AVX3_DL below
#define HWY_AVX3 8
#define HWY_AVX2 16
// 32: reserved for AVX
#define HWY_SSE4 64
#define HWY_SSSE3 128
// 0x100, 0x200: reserved for SSE3, SSE2

// The highest bit in the HWY_TARGETS mask that a x86 target can have. Used for
// dynamic dispatch. All x86 target bits must be lower or equal to
// (1 << HWY_HIGHEST_TARGET_BIT_X86) and they can only use
// HWY_MAX_DYNAMIC_TARGETS in total.
#define HWY_HIGHEST_TARGET_BIT_X86 9

#define HWY_SVE2 0x400
#define HWY_SVE 0x800
// 0x1000 reserved for Helium
#define HWY_NEON 0x2000

#define HWY_HIGHEST_TARGET_BIT_ARM 13

// 0x4000, 0x8000 reserved
#define HWY_PPC8 0x10000  // v2.07 or 3
// 0x20000, 0x40000 reserved for prior VSX/AltiVec

#define HWY_HIGHEST_TARGET_BIT_PPC 18

// 0x80000 reserved
#define HWY_WASM 0x100000

#define HWY_HIGHEST_TARGET_BIT_WASM 20

// 0x200000, 0x400000, 0x800000 reserved

#define HWY_RVV 0x1000000

#define HWY_HIGHEST_TARGET_BIT_RVV 24

// 0x2000000, 0x4000000, 0x8000000, 0x10000000 reserved

#define HWY_SCALAR 0x20000000

#define HWY_HIGHEST_TARGET_BIT_SCALAR 29

// Cannot use higher values, otherwise HWY_TARGETS computation might overflow.

//------------------------------------------------------------------------------
// Set default blocklists

// Disabled means excluded from enabled at user's request. A separate config
// macro allows disabling without deactivating the blocklist below.
#ifndef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS 0
#endif

// Broken means excluded from enabled due to known compiler issues. Allow the
// user to override this blocklist without any guarantee of success.
#ifndef HWY_BROKEN_TARGETS

// x86 clang-6: we saw multiple AVX2/3 compile errors and in one case invalid
// SSE4 codegen (possibly only for msan), so disable all those targets.
#if HWY_ARCH_X86 && (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 700)
#define HWY_BROKEN_TARGETS (HWY_SSE4 | HWY_AVX2 | HWY_AVX3 | HWY_AVX3_DL)
// This entails a major speed reduction, so warn unless the user explicitly
// opts in to scalar-only.
#if !defined(HWY_COMPILE_ONLY_SCALAR)
#pragma message("x86 Clang <= 6: define HWY_COMPILE_ONLY_SCALAR or upgrade.")
#endif

// 32-bit may fail to compile AVX2/3.
#elif HWY_ARCH_X86_32
#define HWY_BROKEN_TARGETS (HWY_AVX2 | HWY_AVX3 | HWY_AVX3_DL)

// MSVC AVX3 support is buggy: https://github.com/Mysticial/Flops/issues/16
#elif HWY_COMPILER_MSVC != 0
#define HWY_BROKEN_TARGETS (HWY_AVX3 | HWY_AVX3_DL)

// armv7be has not been tested and is not yet supported.
#elif HWY_ARCH_ARM_V7 && (defined(__ARM_BIG_ENDIAN) || defined(__BIG_ENDIAN))
#define HWY_BROKEN_TARGETS (HWY_NEON)

// SVE[2] require recent clang or gcc versions.
#elif (HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1100) ||\
(!HWY_COMPILER_CLANG && HWY_COMPILER_GCC && HWY_COMPILER_GCC < 1000)
#define HWY_BROKEN_TARGETS (HWY_SVE | HWY_SVE2)

#else
#define HWY_BROKEN_TARGETS 0
#endif

#endif  // HWY_BROKEN_TARGETS

// Enabled means not disabled nor blocklisted.
#define HWY_ENABLED(targets) \
  ((targets) & ~((HWY_DISABLED_TARGETS) | (HWY_BROKEN_TARGETS)))

//------------------------------------------------------------------------------
// Detect baseline targets using predefined macros

// Baseline means the targets for which the compiler is allowed to generate
// instructions, implying the target CPU would have to support them. Do not use
// this directly because it does not take the blocklist into account. Allow the
// user to override this without any guarantee of success.
#ifndef HWY_BASELINE_TARGETS

#if defined(HWY_EMULATE_SVE)
#define HWY_BASELINE_TARGETS HWY_SVE  // does not support SVE2
#define HWY_BASELINE_AVX3_DL 0
#else

// Also check HWY_ARCH to ensure that simulating unknown platforms ends up with
// HWY_TARGET == HWY_SCALAR.

#if HWY_ARCH_WASM && defined(__wasm_simd128__)
#define HWY_BASELINE_WASM HWY_WASM
#else
#define HWY_BASELINE_WASM 0
#endif

// Avoid choosing the PPC target until we have an implementation.
#if HWY_ARCH_PPC && defined(__VSX__) && 0
#define HWY_BASELINE_PPC8 HWY_PPC8
#else
#define HWY_BASELINE_PPC8 0
#endif

// SVE compiles, but is not yet tested.
#if HWY_ARCH_ARM && defined(__ARM_FEATURE_SVE2)
#define HWY_BASELINE_SVE2 HWY_SVE2
#else
#define HWY_BASELINE_SVE2 0
#endif

#if HWY_ARCH_ARM && defined(__ARM_FEATURE_SVE)
#define HWY_BASELINE_SVE HWY_SVE
#else
#define HWY_BASELINE_SVE 0
#endif

// GCC 4.5.4 only defines __ARM_NEON__; 5.4 defines both.
#if HWY_ARCH_ARM && (defined(__ARM_NEON__) || defined(__ARM_NEON))
#define HWY_BASELINE_NEON HWY_NEON
#else
#define HWY_BASELINE_NEON 0
#endif

// Special handling for MSVC because it has fewer predefined macros
#if HWY_COMPILER_MSVC && !HWY_COMPILER_CLANG

// We can only be sure SSSE3/SSE4 are enabled if AVX is
// (https://stackoverflow.com/questions/18563978/)
#if defined(__AVX__)
#define HWY_CHECK_SSSE3 1
#define HWY_CHECK_SSE4 1
#else
#define HWY_CHECK_SSSE3 0
#define HWY_CHECK_SSE4 0
#endif

// Cannot check for PCLMUL/AES and BMI2/FMA/F16C individually; we assume
// PCLMUL/AES are available if SSE4 is, and BMI2/FMA/F16C if AVX2 is.
#define HWY_CHECK_PCLMUL_AES 1
#define HWY_CHECK_BMI2_FMA 1
#define HWY_CHECK_F16C 1

#else  // non-MSVC

#if defined(__SSSE3__)
#define HWY_CHECK_SSSE3 1
#else
#define HWY_CHECK_SSSE3 0
#endif

#if defined(__SSE4_1__) && defined(__SSE4_2__)
#define HWY_CHECK_SSE4 1
#else
#define HWY_CHECK_SSE4 0
#endif

// If these are disabled, they should not gate the availability of SSE4/AVX2.
#if defined(HWY_DISABLE_PCLMUL_AES) || (defined(__PCLMUL__) && defined(__AES__))
#define HWY_CHECK_PCLMUL_AES 1
#else
#define HWY_CHECK_PCLMUL_AES 0
#endif

#if defined(HWY_DISABLE_BMI2_FMA) || (defined(__BMI2__) && defined(__FMA__))
#define HWY_CHECK_BMI2_FMA 1
#else
#define HWY_CHECK_BMI2_FMA 0
#endif

#if defined(HWY_DISABLE_F16C) || defined(__F16C__)
#define HWY_CHECK_F16C 1
#else
#define HWY_CHECK_F16C 0
#endif

#endif  // non-MSVC

#if HWY_ARCH_X86 && HWY_CHECK_SSSE3
#define HWY_BASELINE_SSSE3 HWY_SSSE3
#else
#define HWY_BASELINE_SSSE3 0
#endif

#if HWY_ARCH_X86 && HWY_CHECK_SSE4 && HWY_CHECK_PCLMUL_AES
#define HWY_BASELINE_SSE4 HWY_SSE4
#else
#define HWY_BASELINE_SSE4 0
#endif

#if HWY_BASELINE_SSE4 != 0 && HWY_CHECK_BMI2_FMA && HWY_CHECK_F16C && \
    defined(__AVX2__)
#define HWY_BASELINE_AVX2 HWY_AVX2
#else
#define HWY_BASELINE_AVX2 0
#endif

// Require everything in AVX2 plus AVX-512 flags (also set by MSVC)
#if HWY_BASELINE_AVX2 != 0 && defined(__AVX512F__) && defined(__AVX512BW__) && \
    defined(__AVX512DQ__) && defined(__AVX512VL__)
#define HWY_BASELINE_AVX3 HWY_AVX3
#else
#define HWY_BASELINE_AVX3 0
#endif

// TODO(janwas): not yet known whether these will be set by MSVC
#if HWY_BASELINE_AVX3 != 0 && defined(__AVXVNNI__) && defined(__VAES__) && \
    defined(__VPCLMULQDQ__)
#define HWY_BASELINE_AVX3_DL HWY_AVX3_DL
#else
#define HWY_BASELINE_AVX3_DL 0
#endif

#if HWY_ARCH_RVV && defined(__riscv_vector)
#define HWY_BASELINE_RVV HWY_RVV
#else
#define HWY_BASELINE_RVV 0
#endif

#define HWY_BASELINE_TARGETS                                                \
  (HWY_SCALAR | HWY_BASELINE_WASM | HWY_BASELINE_PPC8 | HWY_BASELINE_SVE2 | \
   HWY_BASELINE_SVE | HWY_BASELINE_NEON | HWY_BASELINE_SSSE3 |              \
   HWY_BASELINE_SSE4 | HWY_BASELINE_AVX2 | HWY_BASELINE_AVX3 |              \
   HWY_BASELINE_AVX3_DL | HWY_BASELINE_RVV)

#endif  // HWY_EMULATE_SVE

#else
// User already defined HWY_BASELINE_TARGETS, but we still need to define
// HWY_BASELINE_AVX3 (matching user's definition) for HWY_CHECK_AVX3_DL.
#define HWY_BASELINE_AVX3_DL (HWY_BASELINE_TARGETS & HWY_AVX3_DL)
#endif  // HWY_BASELINE_TARGETS

//------------------------------------------------------------------------------
// Choose target for static dispatch

#define HWY_ENABLED_BASELINE HWY_ENABLED(HWY_BASELINE_TARGETS)
#if HWY_ENABLED_BASELINE == 0
#error "At least one baseline target must be defined and enabled"
#endif

// Best baseline, used for static dispatch. This is the least-significant 1-bit
// within HWY_ENABLED_BASELINE and lower bit values imply "better".
#define HWY_STATIC_TARGET (HWY_ENABLED_BASELINE & -HWY_ENABLED_BASELINE)

// Start by assuming static dispatch. If we later use dynamic dispatch, this
// will be defined to other targets during the multiple-inclusion, and finally
// return to the initial value. Defining this outside begin/end_target ensures
// inl headers successfully compile by themselves (required by Bazel).
#define HWY_TARGET HWY_STATIC_TARGET

//------------------------------------------------------------------------------
// Choose targets for dynamic dispatch according to one of four policies

#if (defined(HWY_COMPILE_ONLY_SCALAR) + defined(HWY_COMPILE_ONLY_STATIC) + \
     defined(HWY_COMPILE_ALL_ATTAINABLE)) > 1
#error "Invalid config: can only define a single policy for targets"
#endif

// Further to checking for disabled/broken targets, we only use AVX3_DL after
// explicit opt-in (via this macro OR baseline compiler flags) to avoid
// generating a codepath which is only helpful if the app uses AVX3_DL features.
#if defined(HWY_WANT_AVX3_DL)
#define HWY_CHECK_AVX3_DL HWY_AVX3_DL
#else
#define HWY_CHECK_AVX3_DL HWY_BASELINE_AVX3_DL
#endif

// Attainable means enabled and the compiler allows intrinsics (even when not
// allowed to autovectorize). Used in 3 and 4.
#if HWY_ARCH_X86
#define HWY_ATTAINABLE_TARGETS                                          \
  HWY_ENABLED(HWY_SCALAR | HWY_SSSE3 | HWY_SSE4 | HWY_AVX2 | HWY_AVX3 | \
              HWY_CHECK_AVX3_DL)
#else
#define HWY_ATTAINABLE_TARGETS HWY_ENABLED_BASELINE
#endif

// 1) For older compilers: disable all SIMD (could also set HWY_DISABLED_TARGETS
// to ~HWY_SCALAR, but this is more explicit).
#if defined(HWY_COMPILE_ONLY_SCALAR)
#undef HWY_STATIC_TARGET
#define HWY_STATIC_TARGET HWY_SCALAR  // override baseline
#define HWY_TARGETS HWY_SCALAR

// 2) For forcing static dispatch without code changes (removing HWY_EXPORT)
#elif defined(HWY_COMPILE_ONLY_STATIC)
#define HWY_TARGETS HWY_STATIC_TARGET

// 3) For tests: include all attainable targets (in particular: scalar)
#elif defined(HWY_COMPILE_ALL_ATTAINABLE) || defined(HWY_IS_TEST)
#define HWY_TARGETS HWY_ATTAINABLE_TARGETS

// 4) Default: attainable WITHOUT non-best baseline. This reduces code size by
// excluding superseded targets, in particular scalar.
#else
#define HWY_TARGETS (HWY_ATTAINABLE_TARGETS & (2 * HWY_STATIC_TARGET - 1))

#endif  // target policy

// HWY_ONCE and the multiple-inclusion mechanism rely on HWY_STATIC_TARGET being
// one of the dynamic targets. This also implies HWY_TARGETS != 0 and
// (HWY_TARGETS & HWY_ENABLED_BASELINE) != 0.
#if (HWY_TARGETS & HWY_STATIC_TARGET) == 0
#error "Logic error: best baseline should be included in dynamic targets"
#endif

#endif  // HIGHWAY_HWY_DETECT_TARGETS_H_
