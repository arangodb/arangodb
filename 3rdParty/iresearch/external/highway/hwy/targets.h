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
#include "hwy/detect_targets.h"

namespace hwy {

// Returns (cached) bitfield of enabled targets that are supported on this CPU.
// Implemented in targets.cc; unconditionally compiled to support the use case
// of binary-only distributions. The HWY_SUPPORTED_TARGETS wrapper may allow
// eliding calls to this function.
uint32_t SupportedTargets();

// Evaluates to a function call, or literal if there is a single target.
#if (HWY_TARGETS & (HWY_TARGETS - 1)) == 0
#define HWY_SUPPORTED_TARGETS HWY_TARGETS
#else
#define HWY_SUPPORTED_TARGETS hwy::SupportedTargets()
#endif

// Disable from runtime dispatch the mask of compiled in targets. Targets that
// were not enabled at compile time are ignored. This function is useful to
// disable a target supported by the CPU that is known to have bugs or when a
// lower target is desired. For this reason, attempts to disable targets which
// are in HWY_ENABLED_BASELINE have no effect so SupportedTargets() always
// returns at least the baseline target.
void DisableTargets(uint32_t disabled_targets);

// Set the mock mask of CPU supported targets instead of the actual CPU
// supported targets computed in SupportedTargets(). The return value of
// SupportedTargets() will still be affected by the DisableTargets() mask
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
    case HWY_SSSE3:
      return "SSSE3";
    case HWY_SSE4:
      return "SSE4";
    case HWY_AVX2:
      return "AVX2";
    case HWY_AVX3:
      return "AVX3";
    case HWY_AVX3_DL:
      return "AVX3_DL";
#endif

#if HWY_ARCH_ARM
    case HWY_SVE2:
      return "SVE2";
    case HWY_SVE:
      return "SVE";
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
      return "Unknown";  // must satisfy gtest IsValidParamName()
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
#define HWY_CHOOSE_TARGET_LIST(func_name)           \
  nullptr,                           /* reserved */ \
      nullptr,                       /* reserved */ \
      HWY_CHOOSE_AVX3_DL(func_name), /* AVX3_DL */  \
      HWY_CHOOSE_AVX3(func_name),    /* AVX3 */     \
      HWY_CHOOSE_AVX2(func_name),    /* AVX2 */     \
      nullptr,                       /* AVX */      \
      HWY_CHOOSE_SSE4(func_name),    /* SSE4 */     \
      HWY_CHOOSE_SSSE3(func_name),   /* SSSE3 */    \
      nullptr,                       /* SSE3 */     \
      nullptr                        /* SSE2 */

#elif HWY_ARCH_ARM
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_ARM
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  HWY_CHOOSE_SVE2(func_name),    /* SVE2 */     \
      HWY_CHOOSE_SVE(func_name), /* SVE */      \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_NEON(func_name) /* NEON */

#elif HWY_ARCH_PPC
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 5
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_PPC
#define HWY_CHOOSE_TARGET_LIST(func_name)        \
  nullptr,                        /* reserved */ \
      nullptr,                    /* reserved */ \
      HWY_CHOOSE_PPC8(func_name), /* PPC8 */     \
      nullptr,                    /* VSX */      \
      nullptr                     /* AltiVec */

#elif HWY_ARCH_WASM
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_WASM
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  nullptr,                       /* reserved */ \
      nullptr,                   /* reserved */ \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_WASM(func_name) /* WASM */

#elif HWY_ARCH_RVV
// See HWY_ARCH_X86 above for details.
#define HWY_MAX_DYNAMIC_TARGETS 4
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_RVV
#define HWY_CHOOSE_TARGET_LIST(func_name)       \
  nullptr,                       /* reserved */ \
      nullptr,                   /* reserved */ \
      nullptr,                   /* reserved */ \
      HWY_CHOOSE_RVV(func_name) /* RVV */

#else
// Unknown architecture, will use HWY_SCALAR without dynamic dispatch, though
// still creating single-entry tables in HWY_EXPORT to ensure portability.
#define HWY_MAX_DYNAMIC_TARGETS 1
#define HWY_HIGHEST_TARGET_BIT HWY_HIGHEST_TARGET_BIT_SCALAR
#endif

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
