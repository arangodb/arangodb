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

#ifndef HIGHWAY_HWY_TARGETS_H_
#define HIGHWAY_HWY_TARGETS_H_

#include <vector>

// For SIMD module implementations and their callers. Defines which targets to
// generate and call.

#include "hwy/base.h"

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

// 1,2,4: reserved
#define HWY_AVX3 8
#define HWY_AVX2 16
// 32: reserved for AVX
#define HWY_SSE4 64
// 0x80, 0x100, 0x200: reserved for SSSE3, SSE3, SSE2

// The highest bit in the HWY_TARGETS mask that a x86 target can have. Used for
// dynamic dispatch. All x86 target bits must be lower or equal to
// (1 << HWY_HIGHEST_TARGET_BIT_X86) and they can only use
// HWY_MAX_DYNAMIC_TARGETS in total.
#define HWY_HIGHEST_TARGET_BIT_X86 9

// 0x400, 0x800, 0x1000 reserved for SVE, SVE2, Helium
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
// SSE4 codegen (msan failure), so disable all those targets.
#if HWY_ARCH_X86 && (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 700)
// TODO: Disable all non-scalar targets for every build target once we have
// clang-7 enabled in our builders.
#ifdef MEMORY_SANITIZER
#define HWY_BROKEN_TARGETS (HWY_SSE4 | HWY_AVX2 | HWY_AVX3)
#else
#define HWY_BROKEN_TARGETS 0
#endif
// This entails a major speed reduction, so warn unless the user explicitly
// opts in to scalar-only.
#if !defined(HWY_COMPILE_ONLY_SCALAR)
#pragma message("x86 Clang <= 6: define HWY_COMPILE_ONLY_SCALAR or upgrade.")
#endif

// MSVC, or 32-bit may fail to compile AVX2/3.
#elif HWY_COMPILER_MSVC != 0 || HWY_ARCH_X86_32
#define HWY_BROKEN_TARGETS (HWY_AVX2 | HWY_AVX3)
#pragma message("Disabling AVX2/3 due to known issues with MSVC/32-bit builds")

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

#ifdef __wasm_simd128__
#define HWY_BASELINE_WASM HWY_WASM
#else
#define HWY_BASELINE_WASM 0
#endif

#ifdef __VSX__
#define HWY_BASELINE_PPC8 HWY_PPC8
#else
#define HWY_BASELINE_PPC8 0
#endif

// GCC 4.5.4 only defines the former; 5.4 defines both.
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#define HWY_BASELINE_NEON HWY_NEON
#else
#define HWY_BASELINE_NEON 0
#endif

#ifdef __SSE4_1__
#define HWY_BASELINE_SSE4 HWY_SSE4
#else
#define HWY_BASELINE_SSE4 0
#endif

#ifdef __AVX2__
#define HWY_BASELINE_AVX2 HWY_AVX2
#else
#define HWY_BASELINE_AVX2 0
#endif

#ifdef __AVX512F__
#define HWY_BASELINE_AVX3 HWY_AVX3
#else
#define HWY_BASELINE_AVX3 0
#endif

#ifdef __riscv_vector
#define HWY_BASELINE_RVV HWY_RVV
#else
#define HWY_BASELINE_RVV 0
#endif

#define HWY_BASELINE_TARGETS                                                \
  (HWY_SCALAR | HWY_BASELINE_WASM | HWY_BASELINE_PPC8 | HWY_BASELINE_NEON | \
   HWY_BASELINE_SSE4 | HWY_BASELINE_AVX2 | HWY_BASELINE_AVX3 |              \
   HWY_BASELINE_RVV)

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

// Attainable means enabled and the compiler allows intrinsics (even when not
// allowed to autovectorize). Used in 3 and 4.
#if HWY_ARCH_X86
#define HWY_ATTAINABLE_TARGETS \
  HWY_ENABLED(HWY_SCALAR | HWY_SSE4 | HWY_AVX2 | HWY_AVX3)
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
#elif defined(HWY_COMPILE_ALL_ATTAINABLE)
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

//------------------------------------------------------------------------------

namespace hwy {

// Returns (cached) bitfield of enabled targets that are supported on this CPU.
// Implemented in supported_targets.cc; unconditionally compiled to support the
// use case of binary-only distributions. The HWY_SUPPORTED_TARGETS wrapper may
// allow eliding calls to this function.
uint32_t SupportedTargets();

// Disable from runtime dispatch the mask of compiled in targets. Targets that
// were not enabled at compile time are ignored. This function is useful to
// disable a target supported by the CPU that is known to have bugs or when a
// lower target is desired. For this reason, attempts to disable targets which
// are in HWY_ENABLED_BASELINE have no effect so SupportedTargets() always
// returns at least the baseline target.
void DisableTargets(uint32_t disabled_targets);

// Single target: reduce code size by eliding the call and conditional branches
// inside Choose*() functions.
#if (HWY_TARGETS & (HWY_TARGETS - 1)) == 0
#define HWY_SUPPORTED_TARGETS HWY_TARGETS
#else
#define HWY_SUPPORTED_TARGETS hwy::SupportedTargets()
#endif

// Set the mock mask of CPU supported targets instead of the actual CPU
// supported targets computed in SupportedTargets(). The return value of
// SupportedTargets() will still be affected by the DisabledTargets() mask
// regardless of this mock, to prevent accidentally adding targets that are
// known to be buggy in the current CPU. Call with a mask of 0 to disable the
// mock and use the actual CPU supported targets instead.
void SetSupportedTargetsForTest(uint32_t targets);

// Returns whether the SupportedTargets() function was called since the last
// SetSupportedTargetsForTest() call.
bool SupportedTargetsCalledForTest();

// Return the list of targets in HWY_TARGETS supported by the CPU as a list of
// individual HWY_* target macros such as HWY_SCALAR or HWY_NEON. This list
// is affected by the current SetSupportedTargetsForTest() mock if any.
HWY_INLINE std::vector<uint32_t> SupportedAndGeneratedTargets() {
  std::vector<uint32_t> ret;
  for (uint32_t targets = SupportedTargets() & HWY_TARGETS; targets != 0;
       targets = targets & (targets - 1)) {
    uint32_t current_target = targets & ~(targets - 1);
    ret.push_back(current_target);
  }
  return ret;
}

static inline HWY_MAYBE_UNUSED const char* TargetName(uint32_t target) {
  switch (target) {
#if HWY_ARCH_X86
    case HWY_SSE4:
      return "SSE4";
    case HWY_AVX2:
      return "AVX2";
    case HWY_AVX3:
      return "AVX3";
#endif

#if HWY_ARCH_ARM
    case HWY_NEON:
      return "Neon";
#endif

#if HWY_ARCH_PPC
    case HWY_PPC8:
      return "Power8";
#endif

#if HWY_ARCH_WASM
    case HWY_WASM:
      return "Wasm";
#endif

#if HWY_ARCH_RVV
    case HWY_RVV:
      return "RVV";
#endif

    case HWY_SCALAR:
      return "Scalar";

    default:
      return "?";
  }
}

// The maximum number of dynamic targets on any architecture is defined by
// HWY_MAX_DYNAMIC_TARGETS and depends on the arch.

// For the ChosenTarget mask and index we use a different bit arrangement than
// in the HWY_TARGETS mask. Only the targets involved in the current
// architecture are used in this mask, and therefore only the least significant
// (HWY_MAX_DYNAMIC_TARGETS + 2) bits of the uint32_t mask are used. The least
// significant bit is set when the mask is not initialized, the next
// HWY_MAX_DYNAMIC_TARGETS more significant bits are a range of bits from the
// HWY_TARGETS or SupportedTargets() mask for the given architecture shifted to
// that position and the next more significant bit is used for the scalar
// target. Because of this we need to define equivalent values for HWY_TARGETS
// in this representation.
// This mask representation allows to use ctz() on this mask and obtain a small
// number that's used as an index of the table for dynamic dispatch. In this
// way the first entry is used when the mask is uninitialized, the following
// HWY_MAX_DYNAMIC_TARGETS are for dynamic dispatch and the last one is for
// scalar.

// The HWY_SCALAR bit in the ChosenTarget mask format.
#define HWY_CHOSEN_TARGET_MASK_SCALAR (1u << (HWY_MAX_DYNAMIC_TARGETS + 1))

// Converts from a HWY_TARGETS mask to a ChosenTarget mask format for the
// current architecture.
#define HWY_CHOSEN_TARGET_SHIFT(X)                                    \
  ((((X) >> (HWY_HIGHEST_TARGET_BIT + 1 - HWY_MAX_DYNAMIC_TARGETS)) & \
    ((1u << HWY_MAX_DYNAMIC_TARGETS) - 1))                            \
   << 1)

// The HWY_TARGETS mask in the ChosenTarget mask format.
#define HWY_CHOSEN_TARGET_MASK_TARGETS \
  (HWY_CHOSEN_TARGET_SHIFT(HWY_TARGETS) | HWY_CHOSEN_TARGET_MASK_SCALAR | 1u)

#if HWY_ARCH_X86
// Maximum number of dynamic targets, changing this value is an ABI incompatible
// change
#define HWY_MAX_DYNAMIC_TARGETS 10
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_X86
// These must match the order in which the HWY_TARGETS are defined
// starting by the least significant (HWY_HIGHEST_TARGET_BIT + 1 -
// HWY_MAX_DYNAMIC_TARGETS) bit. This list must contain exactly
// HWY_MAX_DYNAMIC_TARGETS elements and does not include SCALAR. The first entry
// corresponds to the best target. Don't include a "," at the end of the list.
#define HWY_CHOOSE_TARGET_LIST(func_name)        \
  nullptr,                        /* reserved */ \
      nullptr,                    /* reserved */ \
      nullptr,                    /* reserved */ \
      HWY_CHOOSE_AVX3(func_name), /* AVX3 */     \
      HWY_CHOOSE_AVX2(func_name), /* AVX2 */     \
      nullptr,                    /* AVX */      \
      HWY_CHOOSE_SSE4(func_name), /* SSE4 */     \
      nullptr,                    /* SSSE3 */    \
      nullptr,                    /* SSE3 */     \
      nullptr                     /* SSE2 */

#endif  // HWY_ARCH_X86

#if HWY_ARCH_ARM
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_ARM
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  nullptr,                       /* reserved */ \
      nullptr,                   /* reserved */ \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_NEON(func_name) /* NEON */

#endif  // HWY_ARCH_ARM

#if HWY_ARCH_PPC
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 5
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_PPC
#define HWY_CHOOSE_TARGET_LIST(func_name)        \
  nullptr,                        /* reserved */ \
      nullptr,                    /* reserved */ \
      HWY_CHOOSE_PPC8(func_name), /* PPC8 */     \
      nullptr,                    /* VSX */      \
      nullptr                     /* AltiVec */

#endif  // HWY_ARCH_PPC

#if HWY_ARCH_WASM
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_WASM
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  nullptr,                       /* reserved */ \
      nullptr,                   /* reserved */ \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_WASM(func_name) /* WASM */

#endif  // HWY_ARCH_WASM

#if HWY_ARCH_RVV
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_RVV
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  nullptr,                       /* reserved */ \
      nullptr,                   /* reserved */ \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_RVV(func_name) /* RVV */

#endif  // HWY_ARCH_RVV

struct ChosenTarget {
 public:
  // Update the ChosenTarget mask based on the current CPU supported
  // targets.
  void Update();

  // Reset the ChosenTarget to the uninitialized state.
  void DeInit() { mask_.store(1); }

  // Whether the ChosenTarget was initialized. This is useful to know whether
  // any HWY_DYNAMIC_DISPATCH function was called.
  bool IsInitialized() const { return mask_.load() != 1; }

  // Return the index in the dynamic dispatch table to be used by the current
  // CPU. Note that this method must be in the header file so it uses the value
  // of HWY_CHOSEN_TARGET_MASK_TARGETS defined in the translation unit that
  // calls it, which may be different from others. This allows to only consider
  // those targets that were actually compiled in this module.
  size_t HWY_INLINE GetIndex() const {
    return hwy::Num0BitsBelowLS1Bit_Nonzero32(mask_.load() &
                                              HWY_CHOSEN_TARGET_MASK_TARGETS);
  }

 private:
  // Initialized to 1 so GetChosenTargetIndex() returns 0.
  std::atomic<uint32_t> mask_{1};
};

extern ChosenTarget chosen_target;

}  // namespace hwy

#endif  // HIGHWAY_HWY_TARGETS_H_
