// Copyright 2019 Google LLC
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

#include "hwy/targets.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS  // before inttypes.h
#endif
#include <inttypes.h>  // IWYU pragma: keep (PRIx64)
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>  // abort / exit

#include "hwy/highway.h"
#include "hwy/per_target.h"  // VectorBytes

#if HWY_IS_ASAN || HWY_IS_MSAN || HWY_IS_TSAN
#include "sanitizer/common_interface_defs.h"  // __sanitizer_print_stack_trace
#endif

#if HWY_ARCH_X86
#include <xmmintrin.h>
#if HWY_COMPILER_MSVC
#include <intrin.h>
#else  // !HWY_COMPILER_MSVC
#include <cpuid.h>
#endif  // HWY_COMPILER_MSVC

#elif (HWY_ARCH_ARM || HWY_ARCH_PPC) && HWY_OS_LINUX
// sys/auxv.h does not always include asm/hwcap.h, or define HWCAP*, hence we
// still include this directly. See #1199.
#ifndef TOOLCHAIN_MISS_ASM_HWCAP_H
#include <asm/hwcap.h>
#endif
#ifndef TOOLCHAIN_MISS_SYS_AUXV_H
#include <sys/auxv.h>
#endif

#endif  // HWY_ARCH_*

namespace hwy {
namespace {

// When running tests, this value can be set to the mocked supported targets
// mask. Only written to from a single thread before the test starts.
int64_t supported_targets_for_test_ = 0;

// Mask of targets disabled at runtime with DisableTargets.
int64_t supported_mask_ = LimitsMax<int64_t>();

#if HWY_ARCH_X86 && HWY_HAVE_RUNTIME_DISPATCH
namespace x86 {

// Calls CPUID instruction with eax=level and ecx=count and returns the result
// in abcd array where abcd = {eax, ebx, ecx, edx} (hence the name abcd).
HWY_INLINE void Cpuid(const uint32_t level, const uint32_t count,
                      uint32_t* HWY_RESTRICT abcd) {
#if HWY_COMPILER_MSVC
  int regs[4];
  __cpuidex(regs, level, count);
  for (int i = 0; i < 4; ++i) {
    abcd[i] = regs[i];
  }
#else   // HWY_COMPILER_MSVC
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  __cpuid_count(level, count, a, b, c, d);
  abcd[0] = a;
  abcd[1] = b;
  abcd[2] = c;
  abcd[3] = d;
#endif  // HWY_COMPILER_MSVC
}

HWY_INLINE bool IsBitSet(const uint32_t reg, const int index) {
  return (reg & (1U << index)) != 0;
}

// Returns the lower 32 bits of extended control register 0.
// Requires CPU support for "OSXSAVE" (see below).
uint32_t ReadXCR0() {
#if HWY_COMPILER_MSVC
  return static_cast<uint32_t>(_xgetbv(0));
#else   // HWY_COMPILER_MSVC
  uint32_t xcr0, xcr0_high;
  const uint32_t index = 0;
  asm volatile(".byte 0x0F, 0x01, 0xD0"
               : "=a"(xcr0), "=d"(xcr0_high)
               : "c"(index));
  return xcr0;
#endif  // HWY_COMPILER_MSVC
}

bool IsAMD() {
  uint32_t abcd[4];
  Cpuid(0, 0, abcd);
  const uint32_t max_level = abcd[0];
  return max_level >= 1 && abcd[1] == 0x68747541 && abcd[2] == 0x444d4163 &&
         abcd[3] == 0x69746e65;
}

// Arbitrary bit indices indicating which instruction set extensions are
// supported. Use enum to ensure values are distinct.
enum class FeatureIndex : uint32_t {
  kSSE = 0,
  kSSE2,
  kSSE3,
  kSSSE3,

  kSSE41,
  kSSE42,
  kCLMUL,
  kAES,

  kAVX,
  kAVX2,
  kF16C,
  kFMA,
  kLZCNT,
  kBMI,
  kBMI2,

  kAVX512F,
  kAVX512VL,
  kAVX512CD,
  kAVX512DQ,
  kAVX512BW,

  kVNNI,
  kVPCLMULQDQ,
  kVBMI,
  kVBMI2,
  kVAES,
  kPOPCNTDQ,
  kBITALG,
  kGFNI,

  kSentinel
};
static_assert(static_cast<size_t>(FeatureIndex::kSentinel) < 64,
              "Too many bits for u64");

HWY_INLINE constexpr uint64_t Bit(FeatureIndex index) {
  return 1ull << static_cast<size_t>(index);
}

// Returns bit array of FeatureIndex from CPUID feature flags.
uint64_t FlagsFromCPUID() {
  uint64_t flags = 0;  // return value
  uint32_t abcd[4];
  Cpuid(0, 0, abcd);
  const uint32_t max_level = abcd[0];

  // Standard feature flags
  Cpuid(1, 0, abcd);
  flags |= IsBitSet(abcd[3], 25) ? Bit(FeatureIndex::kSSE) : 0;
  flags |= IsBitSet(abcd[3], 26) ? Bit(FeatureIndex::kSSE2) : 0;
  flags |= IsBitSet(abcd[2], 0) ? Bit(FeatureIndex::kSSE3) : 0;
  flags |= IsBitSet(abcd[2], 1) ? Bit(FeatureIndex::kCLMUL) : 0;
  flags |= IsBitSet(abcd[2], 9) ? Bit(FeatureIndex::kSSSE3) : 0;
  flags |= IsBitSet(abcd[2], 12) ? Bit(FeatureIndex::kFMA) : 0;
  flags |= IsBitSet(abcd[2], 19) ? Bit(FeatureIndex::kSSE41) : 0;
  flags |= IsBitSet(abcd[2], 20) ? Bit(FeatureIndex::kSSE42) : 0;
  flags |= IsBitSet(abcd[2], 25) ? Bit(FeatureIndex::kAES) : 0;
  flags |= IsBitSet(abcd[2], 28) ? Bit(FeatureIndex::kAVX) : 0;
  flags |= IsBitSet(abcd[2], 29) ? Bit(FeatureIndex::kF16C) : 0;

  // Extended feature flags
  Cpuid(0x80000001U, 0, abcd);
  flags |= IsBitSet(abcd[2], 5) ? Bit(FeatureIndex::kLZCNT) : 0;

  // Extended features
  if (max_level >= 7) {
    Cpuid(7, 0, abcd);
    flags |= IsBitSet(abcd[1], 3) ? Bit(FeatureIndex::kBMI) : 0;
    flags |= IsBitSet(abcd[1], 5) ? Bit(FeatureIndex::kAVX2) : 0;
    flags |= IsBitSet(abcd[1], 8) ? Bit(FeatureIndex::kBMI2) : 0;

    flags |= IsBitSet(abcd[1], 16) ? Bit(FeatureIndex::kAVX512F) : 0;
    flags |= IsBitSet(abcd[1], 17) ? Bit(FeatureIndex::kAVX512DQ) : 0;
    flags |= IsBitSet(abcd[1], 28) ? Bit(FeatureIndex::kAVX512CD) : 0;
    flags |= IsBitSet(abcd[1], 30) ? Bit(FeatureIndex::kAVX512BW) : 0;
    flags |= IsBitSet(abcd[1], 31) ? Bit(FeatureIndex::kAVX512VL) : 0;

    flags |= IsBitSet(abcd[2], 1) ? Bit(FeatureIndex::kVBMI) : 0;
    flags |= IsBitSet(abcd[2], 6) ? Bit(FeatureIndex::kVBMI2) : 0;
    flags |= IsBitSet(abcd[2], 8) ? Bit(FeatureIndex::kGFNI) : 0;
    flags |= IsBitSet(abcd[2], 9) ? Bit(FeatureIndex::kVAES) : 0;
    flags |= IsBitSet(abcd[2], 10) ? Bit(FeatureIndex::kVPCLMULQDQ) : 0;
    flags |= IsBitSet(abcd[2], 11) ? Bit(FeatureIndex::kVNNI) : 0;
    flags |= IsBitSet(abcd[2], 12) ? Bit(FeatureIndex::kBITALG) : 0;
    flags |= IsBitSet(abcd[2], 14) ? Bit(FeatureIndex::kPOPCNTDQ) : 0;
  }

  return flags;
}

// Each Highway target requires a 'group' of multiple features/flags.
constexpr uint64_t kGroupSSE2 =
    Bit(FeatureIndex::kSSE) | Bit(FeatureIndex::kSSE2);

constexpr uint64_t kGroupSSSE3 =
    Bit(FeatureIndex::kSSE3) | Bit(FeatureIndex::kSSSE3) | kGroupSSE2;

constexpr uint64_t kGroupSSE4 =
    Bit(FeatureIndex::kSSE41) | Bit(FeatureIndex::kSSE42) |
    Bit(FeatureIndex::kCLMUL) | Bit(FeatureIndex::kAES) | kGroupSSSE3;

// We normally assume BMI/BMI2/FMA are available if AVX2 is. This allows us to
// use BZHI and (compiler-generated) MULX. However, VirtualBox lacks them
// [https://www.virtualbox.org/ticket/15471]. Thus we provide the option of
// avoiding using and requiring these so AVX2 can still be used.
#ifdef HWY_DISABLE_BMI2_FMA
constexpr uint64_t kGroupBMI2_FMA = 0;
#else
constexpr uint64_t kGroupBMI2_FMA = Bit(FeatureIndex::kBMI) |
                                    Bit(FeatureIndex::kBMI2) |
                                    Bit(FeatureIndex::kFMA);
#endif

#ifdef HWY_DISABLE_F16C
constexpr uint64_t kGroupF16C = 0;
#else
constexpr uint64_t kGroupF16C = Bit(FeatureIndex::kF16C);
#endif

constexpr uint64_t kGroupAVX2 =
    Bit(FeatureIndex::kAVX) | Bit(FeatureIndex::kAVX2) |
    Bit(FeatureIndex::kLZCNT) | kGroupBMI2_FMA | kGroupF16C | kGroupSSE4;

constexpr uint64_t kGroupAVX3 =
    Bit(FeatureIndex::kAVX512F) | Bit(FeatureIndex::kAVX512VL) |
    Bit(FeatureIndex::kAVX512DQ) | Bit(FeatureIndex::kAVX512BW) |
    Bit(FeatureIndex::kAVX512CD) | kGroupAVX2;

constexpr uint64_t kGroupAVX3_DL =
    Bit(FeatureIndex::kVNNI) | Bit(FeatureIndex::kVPCLMULQDQ) |
    Bit(FeatureIndex::kVBMI) | Bit(FeatureIndex::kVBMI2) |
    Bit(FeatureIndex::kVAES) | Bit(FeatureIndex::kPOPCNTDQ) |
    Bit(FeatureIndex::kBITALG) | Bit(FeatureIndex::kGFNI) | kGroupAVX3;

int64_t DetectTargets() {
  int64_t bits = 0;  // return value of supported targets.
#if HWY_ARCH_X86_64
  bits |= HWY_SSE2;  // always present in x64
#endif

  const uint64_t flags = FlagsFromCPUID();
  // Set target bit(s) if all their group's flags are all set.
  if ((flags & kGroupAVX3_DL) == kGroupAVX3_DL) {
    bits |= HWY_AVX3_DL;
  }
  if ((flags & kGroupAVX3) == kGroupAVX3) {
    bits |= HWY_AVX3;
  }
  if ((flags & kGroupAVX2) == kGroupAVX2) {
    bits |= HWY_AVX2;
  }
  if ((flags & kGroupSSE4) == kGroupSSE4) {
    bits |= HWY_SSE4;
  }
  if ((flags & kGroupSSSE3) == kGroupSSSE3) {
    bits |= HWY_SSSE3;
  }
#if HWY_ARCH_X86_32
  if ((flags & kGroupSSE2) == kGroupSSE2) {
    bits |= HWY_SSE2;
  }
#endif

  // Clear bits if the OS does not support XSAVE - otherwise, registers
  // are not preserved across context switches.
  uint32_t abcd[4];
  Cpuid(1, 0, abcd);
  const bool has_osxsave = IsBitSet(abcd[2], 27);
  if (has_osxsave) {
    const uint32_t xcr0 = ReadXCR0();
    const int64_t min_avx3 = HWY_AVX3 | HWY_AVX3_DL;
    const int64_t min_avx2 = HWY_AVX2 | min_avx3;
    // XMM
    if (!IsBitSet(xcr0, 1)) {
#if HWY_ARCH_X86_64
      // The HWY_SSE2, HWY_SSSE3, and HWY_SSE4 bits do not need to be
      // cleared on x86_64, even if bit 1 of XCR0 is not set, as
      // the lower 128 bits of XMM0-XMM15 are guaranteed to be
      // preserved across context switches on x86_64

      // Only clear the AVX2/AVX3 bits on x86_64 if bit 1 of XCR0 is not set
      bits &= ~min_avx2;
#else
      bits &= ~(HWY_SSE2 | HWY_SSSE3 | HWY_SSE4 | min_avx2);
#endif
    }
    // YMM
    if (!IsBitSet(xcr0, 2)) {
      bits &= ~min_avx2;
    }
    // opmask, ZMM lo/hi
    if (!IsBitSet(xcr0, 5) || !IsBitSet(xcr0, 6) || !IsBitSet(xcr0, 7)) {
      bits &= ~min_avx3;
    }
  }  // has_osxsave

  // This is mainly to work around the slow Zen4 CompressStore. It's unclear
  // whether subsequent AMD models will be affected; assume yes.
  if ((bits & HWY_AVX3_DL) && IsAMD()) {
    bits |= HWY_AVX3_ZEN4;
  }

  return bits;
}

}  // namespace x86
#elif HWY_ARCH_ARM && HWY_HAVE_RUNTIME_DISPATCH
namespace arm {
int64_t DetectTargets() {
  int64_t bits = 0;               // return value of supported targets.
  using CapBits = unsigned long;  // NOLINT
  const CapBits hw = getauxval(AT_HWCAP);
  (void)hw;

#if HWY_ARCH_ARM_A64
  bits |= HWY_NEON_WITHOUT_AES;  // aarch64 always has NEON and VFPv4..

  // .. but not necessarily AES, which is required for HWY_NEON.
#if defined(HWCAP_AES)
  if (hw & HWCAP_AES) {
    bits |= HWY_NEON;
  }
#endif  // HWCAP_AES

#if defined(HWCAP_SVE)
  if (hw & HWCAP_SVE) {
    bits |= HWY_SVE;
  }
#endif

#if defined(HWCAP2_SVE2) && defined(HWCAP2_SVEAES)
  const CapBits hw2 = getauxval(AT_HWCAP2);
  if ((hw2 & HWCAP2_SVE2) && (hw2 & HWCAP2_SVEAES)) {
    bits |= HWY_SVE2;
  }
#endif

#else  // !HWY_ARCH_ARM_A64

// Some old auxv.h / hwcap.h do not define these. If not, treat as unsupported.
#if defined(HWCAP_NEON) && defined(HWCAP_VFPv4)
  if ((hw & HWCAP_NEON) && (hw & HWCAP_VFPv4)) {
    bits |= HWY_NEON_WITHOUT_AES;
  }
#endif

  // aarch32 would check getauxval(AT_HWCAP2) & HWCAP2_AES, but we do not yet
  // support that platform, and Armv7 lacks AES entirely. Because HWY_NEON
  // requires native AES instructions, we do not enable that target here.

#endif  // HWY_ARCH_ARM_A64
  return bits;
}
}  // namespace arm
#elif HWY_ARCH_PPC && HWY_HAVE_RUNTIME_DISPATCH
namespace ppc {

#ifndef PPC_FEATURE_HAS_ALTIVEC
#define PPC_FEATURE_HAS_ALTIVEC 0x10000000
#endif

#ifndef PPC_FEATURE_HAS_VSX
#define PPC_FEATURE_HAS_VSX 0x00000080
#endif

#ifndef PPC_FEATURE2_ARCH_2_07
#define PPC_FEATURE2_ARCH_2_07 0x80000000
#endif

#ifndef PPC_FEATURE2_VEC_CRYPTO
#define PPC_FEATURE2_VEC_CRYPTO 0x02000000
#endif

#ifndef PPC_FEATURE2_ARCH_3_00
#define PPC_FEATURE2_ARCH_3_00 0x00800000
#endif

#ifndef PPC_FEATURE2_ARCH_3_1
#define PPC_FEATURE2_ARCH_3_1 0x00040000
#endif

using CapBits = unsigned long;  // NOLINT

// For AT_HWCAP, the others are for AT_HWCAP2
constexpr CapBits kGroupVSX = PPC_FEATURE_HAS_ALTIVEC | PPC_FEATURE_HAS_VSX;

#if defined(HWY_DISABLE_PPC8_CRYPTO)
constexpr CapBits kGroupPPC8 = PPC_FEATURE2_ARCH_2_07;
#else
constexpr CapBits kGroupPPC8 = PPC_FEATURE2_ARCH_2_07 | PPC_FEATURE2_VEC_CRYPTO;
#endif
constexpr CapBits kGroupPPC9 = kGroupPPC8 | PPC_FEATURE2_ARCH_3_00;
constexpr CapBits kGroupPPC10 = kGroupPPC9 | PPC_FEATURE2_ARCH_3_1;

int64_t DetectTargets() {
  int64_t bits = 0;  // return value of supported targets.
  const CapBits hw = getauxval(AT_HWCAP);

  if ((hw & kGroupVSX) == kGroupVSX) {
    const CapBits hw2 = getauxval(AT_HWCAP2);
    if ((hw2 & kGroupPPC8) == kGroupPPC8) {
      bits |= HWY_PPC8;
    }
    if ((hw2 & kGroupPPC9) == kGroupPPC9) {
      bits |= HWY_PPC9;
    }
    if ((hw2 & kGroupPPC10) == kGroupPPC10) {
      bits |= HWY_PPC10;
    }
  }  // VSX
  return bits;
}
}  // namespace ppc
#endif  // HWY_ARCH_X86

// Returns targets supported by the CPU, independently of DisableTargets.
// Factored out of SupportedTargets to make its structure more obvious. Note
// that x86 CPUID may take several hundred cycles.
int64_t DetectTargets() {
  // Apps will use only one of these (the default is EMU128), but compile flags
  // for this TU may differ from that of the app, so allow both.
  int64_t bits = HWY_SCALAR | HWY_EMU128;

#if HWY_ARCH_X86 && HWY_HAVE_RUNTIME_DISPATCH
  bits |= x86::DetectTargets();
#elif HWY_ARCH_ARM && HWY_HAVE_RUNTIME_DISPATCH
  bits |= arm::DetectTargets();
#elif HWY_ARCH_PPC && HWY_HAVE_RUNTIME_DISPATCH
  bits |= ppc::DetectTargets();

#else
  // TODO(janwas): detect support for WASM/RVV.
  // This file is typically compiled without HWY_IS_TEST, but targets_test has
  // it set, and will expect all of its HWY_TARGETS (= all attainable) to be
  // supported.
  bits |= HWY_ENABLED_BASELINE;
#endif  // HWY_ARCH_*

  if ((bits & HWY_ENABLED_BASELINE) != HWY_ENABLED_BASELINE) {
    fprintf(stderr,
            "WARNING: CPU supports %" PRIx64 " but software requires %" PRIx64
            "\n",
            bits, static_cast<int64_t>(HWY_ENABLED_BASELINE));
  }

  return bits;
}

}  // namespace

HWY_DLLEXPORT HWY_NORETURN void HWY_FORMAT(3, 4)
    Abort(const char* file, int line, const char* format, ...) {
  char buf[2000];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  fprintf(stderr, "Abort at %s:%d: %s\n", file, line, buf);

// If compiled with any sanitizer, they can also print a stack trace.
#if HWY_IS_ASAN || HWY_IS_MSAN || HWY_IS_TSAN
  __sanitizer_print_stack_trace();
#endif  // HWY_IS_*
  fflush(stderr);

// Now terminate the program:
#if HWY_ARCH_RVV
  exit(1);  // trap/abort just freeze Spike.
#elif HWY_IS_DEBUG_BUILD && !HWY_COMPILER_MSVC
  // Facilitates breaking into a debugger, but don't use this in non-debug
  // builds because it looks like "illegal instruction", which is misleading.
  __builtin_trap();
#else
  abort();  // Compile error without this due to HWY_NORETURN.
#endif
}

HWY_DLLEXPORT void DisableTargets(int64_t disabled_targets) {
  supported_mask_ = static_cast<int64_t>(~disabled_targets);
  // This will take effect on the next call to SupportedTargets, which is
  // called right before GetChosenTarget::Update. However, calling Update here
  // would make it appear that HWY_DYNAMIC_DISPATCH was called, which we want
  // to check in tests. We instead de-initialize such that the next
  // HWY_DYNAMIC_DISPATCH calls GetChosenTarget::Update via FunctionCache.
  GetChosenTarget().DeInit();
}

HWY_DLLEXPORT void SetSupportedTargetsForTest(int64_t targets) {
  supported_targets_for_test_ = targets;
  GetChosenTarget().DeInit();  // see comment above
}

HWY_DLLEXPORT int64_t SupportedTargets() {
  int64_t targets = supported_targets_for_test_;
  if (HWY_LIKELY(targets == 0)) {
    // Mock not active. Re-detect instead of caching just in case we're on a
    // heterogeneous ISA (also requires some app support to pin threads). This
    // is only reached on the first HWY_DYNAMIC_DISPATCH or after each call to
    // DisableTargets or SetSupportedTargetsForTest.
    targets = DetectTargets();

    // VectorBytes invokes HWY_DYNAMIC_DISPATCH. To prevent infinite recursion,
    // first set up ChosenTarget. No need to Update() again afterwards with the
    // final targets - that will be done by a caller of this function.
    GetChosenTarget().Update(targets);

    // Now that we can call VectorBytes, check for targets with specific sizes.
    if (HWY_ARCH_ARM_A64) {
      const size_t vec_bytes = VectorBytes();  // uncached, see declaration
      if ((targets & HWY_SVE) && vec_bytes == 32) {
        targets = static_cast<int64_t>(targets | HWY_SVE_256);
      } else {
        targets = static_cast<int64_t>(targets & ~HWY_SVE_256);
      }
      if ((targets & HWY_SVE2) && vec_bytes == 16) {
        targets = static_cast<int64_t>(targets | HWY_SVE2_128);
      } else {
        targets = static_cast<int64_t>(targets & ~HWY_SVE2_128);
      }
    }  // HWY_ARCH_ARM_A64
  }

  targets &= supported_mask_;
  return targets == 0 ? HWY_STATIC_TARGET : targets;
}

HWY_DLLEXPORT ChosenTarget& GetChosenTarget() {
  static ChosenTarget chosen_target;
  return chosen_target;
}

}  // namespace hwy
