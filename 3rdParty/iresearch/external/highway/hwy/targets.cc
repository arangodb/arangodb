// Copyright 2019 Google LLC
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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <limits>

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
#include "sanitizer/common_interface_defs.h"  // __sanitizer_print_stack_trace
#endif                                        // defined(*_SANITIZER)

#if HWY_ARCH_X86
#include <xmmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace hwy {
namespace {

#if HWY_ARCH_X86

bool IsBitSet(const uint32_t reg, const int index) {
  return (reg & (1U << index)) != 0;
}

// Calls CPUID instruction with eax=level and ecx=count and returns the result
// in abcd array where abcd = {eax, ebx, ecx, edx} (hence the name abcd).
void Cpuid(const uint32_t level, const uint32_t count,
           uint32_t* HWY_RESTRICT abcd) {
#ifdef _MSC_VER
  int regs[4];
  __cpuidex(regs, level, count);
  for (int i = 0; i < 4; ++i) {
    abcd[i] = regs[i];
  }
#else
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  __cpuid_count(level, count, a, b, c, d);
  abcd[0] = a;
  abcd[1] = b;
  abcd[2] = c;
  abcd[3] = d;
#endif
}

// Returns the lower 32 bits of extended control register 0.
// Requires CPU support for "OSXSAVE" (see below).
uint32_t ReadXCR0() {
#ifdef _MSC_VER
  return static_cast<uint32_t>(_xgetbv(0));
#else
  uint32_t xcr0, xcr0_high;
  const uint32_t index = 0;
  asm volatile(".byte 0x0F, 0x01, 0xD0"
               : "=a"(xcr0), "=d"(xcr0_high)
               : "c"(index));
  return xcr0;
#endif
}

#endif  // HWY_ARCH_X86

// Not function-local => no compiler-generated locking.
std::atomic<uint32_t> supported_{0};  // Not yet initialized

// When running tests, this value can be set to the mocked supported targets
// mask. Only written to from a single thread before the test starts.
uint32_t supported_targets_for_test_ = 0;

// Mask of targets disabled at runtime with DisableTargets.
uint32_t supported_mask_{std::numeric_limits<uint32_t>::max()};

#if HWY_ARCH_X86
// Bits indicating which instruction set extensions are supported.
constexpr uint32_t kSSE = 1 << 0;
constexpr uint32_t kSSE2 = 1 << 1;
constexpr uint32_t kSSE3 = 1 << 2;
constexpr uint32_t kSSSE3 = 1 << 3;
constexpr uint32_t kSSE41 = 1 << 4;
constexpr uint32_t kSSE42 = 1 << 5;
constexpr uint32_t kGroupSSE4 = kSSE | kSSE2 | kSSE3 | kSSSE3 | kSSE41 | kSSE42;

constexpr uint32_t kAVX = 1u << 6;
constexpr uint32_t kAVX2 = 1u << 7;
constexpr uint32_t kFMA = 1u << 8;
constexpr uint32_t kLZCNT = 1u << 9;
constexpr uint32_t kBMI = 1u << 10;
constexpr uint32_t kBMI2 = 1u << 11;

// We normally assume BMI/BMI2/FMA are available if AVX2 is. This allows us to
// use BZHI and (compiler-generated) MULX. However, VirtualBox lacks them
// [https://www.virtualbox.org/ticket/15471]. Thus we provide the option of
// avoiding using and requiring these so AVX2 can still be used.
#ifdef HWY_DISABLE_BMI2_FMA
constexpr uint32_t kGroupAVX2 = kAVX | kAVX2 | kLZCNT;
#else
constexpr uint32_t kGroupAVX2 = kAVX | kAVX2 | kFMA | kLZCNT | kBMI | kBMI2;
#endif

constexpr uint32_t kAVX512F = 1u << 12;
constexpr uint32_t kAVX512VL = 1u << 13;
constexpr uint32_t kAVX512DQ = 1u << 14;
constexpr uint32_t kAVX512BW = 1u << 15;
constexpr uint32_t kGroupAVX3 = kAVX512F | kAVX512VL | kAVX512DQ | kAVX512BW;
#endif

}  // namespace

HWY_NORETURN void HWY_FORMAT(3, 4)
    Abort(const char* file, int line, const char* format, ...) {
  char buf[2000];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  fprintf(stderr, "Abort at %s:%d: %s\n", file, line, buf);
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
  // If compiled with any sanitizer print a stack trace. This call doesn't crash
  // the program, instead the trap below will crash it also allowing gdb to
  // break there.
  __sanitizer_print_stack_trace();
#endif  // defined(*_SANITIZER)
  fflush(stderr);

#if HWY_COMPILER_MSVC
  abort();  // Compile error without this due to HWY_NORETURN.
#else
  __builtin_trap();
#endif
}

void DisableTargets(uint32_t disabled_targets) {
  supported_mask_ = ~(disabled_targets & ~HWY_ENABLED_BASELINE);
  // We can call Update() here to initialize the mask but that will trigger a
  // call to SupportedTargets() which we use in tests to tell whether any of the
  // highway dynamic dispatch functions were used.
  chosen_target.DeInit();
}

void SetSupportedTargetsForTest(uint32_t targets) {
  // Reset the cached supported_ value to 0 to force a re-evaluation in the
  // next call to SupportedTargets() which will use the mocked value set here
  // if not zero.
  supported_.store(0, std::memory_order_release);
  supported_targets_for_test_ = targets;
  chosen_target.DeInit();
}

bool SupportedTargetsCalledForTest() {
  return supported_.load(std::memory_order_acquire) != 0;
}

uint32_t SupportedTargets() {
  uint32_t bits = supported_.load(std::memory_order_acquire);
  // Already initialized?
  if (HWY_LIKELY(bits != 0)) {
    return bits & supported_mask_;
  }

  // When running tests, this allows to mock the current supported targets.
  if (HWY_UNLIKELY(supported_targets_for_test_ != 0)) {
    // Store the value to signal that this was used.
    supported_.store(supported_targets_for_test_, std::memory_order_release);
    return supported_targets_for_test_ & supported_mask_;
  }

  bits = HWY_SCALAR;

#if HWY_ARCH_X86
  uint32_t flags = 0;
  uint32_t abcd[4];

  Cpuid(0, 0, abcd);
  const uint32_t max_level = abcd[0];

  // Standard feature flags
  Cpuid(1, 0, abcd);
  flags |= IsBitSet(abcd[3], 25) ? kSSE : 0;
  flags |= IsBitSet(abcd[3], 26) ? kSSE2 : 0;
  flags |= IsBitSet(abcd[2], 0) ? kSSE3 : 0;
  flags |= IsBitSet(abcd[2], 9) ? kSSSE3 : 0;
  flags |= IsBitSet(abcd[2], 19) ? kSSE41 : 0;
  flags |= IsBitSet(abcd[2], 20) ? kSSE42 : 0;
  flags |= IsBitSet(abcd[2], 12) ? kFMA : 0;
  flags |= IsBitSet(abcd[2], 28) ? kAVX : 0;
  const bool has_osxsave = IsBitSet(abcd[2], 27);

  // Extended feature flags
  Cpuid(0x80000001U, 0, abcd);
  flags |= IsBitSet(abcd[2], 5) ? kLZCNT : 0;

  // Extended features
  if (max_level >= 7) {
    Cpuid(7, 0, abcd);
    flags |= IsBitSet(abcd[1], 3) ? kBMI : 0;
    flags |= IsBitSet(abcd[1], 5) ? kAVX2 : 0;
    flags |= IsBitSet(abcd[1], 8) ? kBMI2 : 0;

    flags |= IsBitSet(abcd[1], 16) ? kAVX512F : 0;
    flags |= IsBitSet(abcd[1], 17) ? kAVX512DQ : 0;
    flags |= IsBitSet(abcd[1], 30) ? kAVX512BW : 0;
    flags |= IsBitSet(abcd[1], 31) ? kAVX512VL : 0;
  }

  // Verify OS support for XSAVE, without which XMM/YMM registers are not
  // preserved across context switches and are not safe to use.
  if (has_osxsave) {
    const uint32_t xcr0 = ReadXCR0();
    // XMM
    if (!IsBitSet(xcr0, 1)) {
      flags = 0;
    }
    // YMM
    if (!IsBitSet(xcr0, 2)) {
      flags &= ~kGroupAVX2;
    }
    // ZMM + opmask
    if ((xcr0 & 0x70) != 0x70) {
      flags &= ~kGroupAVX3;
    }
  }

  // Set target bit(s) if all their group's flags are all set.
  if ((flags & kGroupAVX3) == kGroupAVX3) {
    bits |= HWY_AVX3;
  }
  if ((flags & kGroupAVX2) == kGroupAVX2) {
    bits |= HWY_AVX2;
  }
  if ((flags & kGroupSSE4) == kGroupSSE4) {
    bits |= HWY_SSE4;
  }
#else
  // TODO(janwas): detect for other platforms
  bits = HWY_ENABLED_BASELINE;
#endif  // HWY_ARCH_X86

  if ((bits & HWY_ENABLED_BASELINE) != HWY_ENABLED_BASELINE) {
    fprintf(stderr, "WARNING: CPU supports %zx but software requires %x\n",
            size_t(bits), HWY_ENABLED_BASELINE);
  }

  supported_.store(bits, std::memory_order_release);
  return bits & supported_mask_;
}

// Declared in targets.h
ChosenTarget chosen_target;

void ChosenTarget::Update() {
  // The supported variable contains the current CPU supported targets shifted
  // to the location expected by the ChosenTarget mask. We enabled SCALAR
  // regardless of whether it was compiled since it is also used as the
  // fallback mechanism to the baseline target.
  uint32_t supported = HWY_CHOSEN_TARGET_SHIFT(hwy::SupportedTargets()) |
                       HWY_CHOSEN_TARGET_MASK_SCALAR;
  mask_.store(supported);
}

}  // namespace hwy
