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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS  // before inttypes.h
#endif
#include <inttypes.h>  // IWYU pragma: keep
#include <stdio.h>
#include <string.h>  // memcmp

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/mask_mem_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestMaskedLoad {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    auto bool_lanes = AllocateAligned<TI>(N);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(bool_lanes && lanes);

    const Vec<D> v = Iota(d, T{1});
    const Vec<D> v2 = Iota(d, T{2});
    Store(v, d, lanes.get());

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = (Random32(&rng) & 1024) ? TI(1) : TI(0);
      }

      const auto mask_i = Load(di, bool_lanes.get());
      const auto mask = RebindMask(d, Gt(mask_i, Zero(di)));
      const auto expected = IfThenElseZero(mask, Load(d, lanes.get()));
      const auto expected2 = IfThenElse(mask, Load(d, lanes.get()), v2);
      const auto actual = MaskedLoad(mask, d, lanes.get());
      const auto actual2 = MaskedLoadOr(v2, mask, d, lanes.get());
      HWY_ASSERT_VEC_EQ(d, expected, actual);
      HWY_ASSERT_VEC_EQ(d, expected2, actual2);
    }
  }
};

HWY_NOINLINE void TestAllMaskedLoad() {
  ForAllTypes(ForPartialVectors<TestMaskedLoad>());
}

struct TestBlendedStore {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    auto bool_lanes = AllocateAligned<TI>(N);
    auto actual = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(bool_lanes && actual && expected);

    const Vec<D> v = Iota(d, T{1});

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = (Random32(&rng) & 1024) ? TI(1) : TI(0);
        // Re-initialize to something distinct from v[i].
        actual[i] = static_cast<T>(127 - (i & 127));
        expected[i] = bool_lanes[i] ? static_cast<T>(i + 1) : actual[i];
      }

      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));
      BlendedStore(v, mask, d, actual.get());
      HWY_ASSERT_VEC_EQ(d, expected.get(), Load(d, actual.get()));
    }
  }
};

HWY_NOINLINE void TestAllBlendedStore() {
  ForAllTypes(ForPartialVectors<TestBlendedStore>());
}

class TestStoreMaskBits {
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*t*/, D /*d*/) {
    RandomState rng;
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    const size_t expected_num_bytes = (N + 7) / 8;
    auto bool_lanes = AllocateAligned<TI>(N);
    auto expected = AllocateAligned<uint8_t>(expected_num_bytes);
    auto actual = AllocateAligned<uint8_t>(HWY_MAX(8, expected_num_bytes));
    HWY_ASSERT(bool_lanes && actual && expected);

    const ScalableTag<uint8_t, -3> d_bits;

    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      // Generate random mask pattern.
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = static_cast<TI>((rng() & 1024) ? 1 : 0);
      }
      const auto bools = Load(di, bool_lanes.get());
      const auto mask = Gt(bools, Zero(di));

      // Requires at least 8 bytes, ensured above.
      const size_t bytes_written = StoreMaskBits(di, mask, actual.get());
      if (bytes_written != expected_num_bytes) {
        fprintf(stderr, "%s expected %" PRIu64 " bytes, actual %" PRIu64 "\n",
                TypeName(T(), N).c_str(),
                static_cast<uint64_t>(expected_num_bytes),
                static_cast<uint64_t>(bytes_written));

        HWY_ASSERT(false);
      }

      // Requires at least 8 bytes, ensured above.
      const auto mask2 = LoadMaskBits(di, actual.get());
      HWY_ASSERT_MASK_EQ(di, mask, mask2);

      memset(expected.get(), 0, expected_num_bytes);
      for (size_t i = 0; i < N; ++i) {
        expected[i / 8] =
            static_cast<uint8_t>(expected[i / 8] | (bool_lanes[i] << (i % 8)));
      }

      size_t i = 0;
      // Stored bits must match original mask
      for (; i < N; ++i) {
        const TI is_set = (actual[i / 8] & (1 << (i % 8))) ? 1 : 0;
        if (is_set != bool_lanes[i]) {
          fprintf(stderr, "%s lane %" PRIu64 ": expected %d, actual %d\n",
                  TypeName(T(), N).c_str(), static_cast<uint64_t>(i),
                  static_cast<int>(bool_lanes[i]), static_cast<int>(is_set));
          Print(di, "bools", bools, 0, N);
          Print(d_bits, "expected bytes", Load(d_bits, expected.get()), 0,
                expected_num_bytes);
          Print(d_bits, "actual bytes", Load(d_bits, actual.get()), 0,
                expected_num_bytes);

          HWY_ASSERT(false);
        }
      }
      // Any partial bits in the last byte must be zero
      for (; i < 8 * bytes_written; ++i) {
        const int bit = (actual[i / 8] & (1 << (i % 8)));
        if (bit != 0) {
          fprintf(stderr, "%s: bit #%" PRIu64 " should be zero\n",
                  TypeName(T(), N).c_str(), static_cast<uint64_t>(i));
          Print(di, "bools", bools, 0, N);
          Print(d_bits, "expected bytes", Load(d_bits, expected.get()), 0,
                expected_num_bytes);
          Print(d_bits, "actual bytes", Load(d_bits, actual.get()), 0,
                expected_num_bytes);

          HWY_ASSERT(false);
        }
      }
    }
  }
};

HWY_NOINLINE void TestAllStoreMaskBits() {
  ForAllTypes(ForPartialVectors<TestStoreMaskBits>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyMaskTest);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllMaskedLoad);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllBlendedStore);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllStoreMaskBits);
}  // namespace hwy

#endif
