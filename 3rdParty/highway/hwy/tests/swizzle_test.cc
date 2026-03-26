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

#include <string.h>  // memset

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/swizzle_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestGetLane {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(1));
    HWY_ASSERT_EQ(T(1), GetLane(v));
  }
};

HWY_NOINLINE void TestAllGetLane() {
  ForAllTypes(ForPartialVectors<TestGetLane>());
}

struct TestExtractLane {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(1));
    for (size_t i = 0; i < Lanes(d); ++i) {
      const T actual = ExtractLane(v, i);
      HWY_ASSERT_EQ(static_cast<T>(i + 1), actual);
    }
  }
};

HWY_NOINLINE void TestAllExtractLane() {
  ForAllTypes(ForPartialVectors<TestExtractLane>());
}

struct TestInsertLane {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v = Iota(d, T(1));
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    Store(v, d, lanes.get());

    for (size_t i = 0; i < Lanes(d); ++i) {
      lanes[i] = T{0};
      const V actual = InsertLane(v, i, static_cast<T>(i + 1));
      HWY_ASSERT_VEC_EQ(d, v, actual);
      Store(v, d, lanes.get());  // restore lane i
    }
  }
};

HWY_NOINLINE void TestAllInsertLane() {
  ForAllTypes(ForPartialVectors<TestInsertLane>());
}

struct TestDupEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((static_cast<int>(i) & ~1) + 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), DupEven(Iota(d, 1)));
  }
};

HWY_NOINLINE void TestAllDupEven() {
  ForUIF3264(ForShrinkableVectors<TestDupEven>());
}

struct TestDupOdd {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((static_cast<int>(i) & ~1) + 2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), DupOdd(Iota(d, 1)));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllDupOdd() {
  ForUIF3264(ForShrinkableVectors<TestDupOdd>());
}

struct TestOddEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto even = Iota(d, 1);
    const auto odd = Iota(d, static_cast<T>(1 + N));
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>(1 + i + ((i & 1) ? N : 0));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), OddEven(odd, even));
  }
};

HWY_NOINLINE void TestAllOddEven() {
  ForAllTypes(ForShrinkableVectors<TestOddEven>());
}

struct TestOddEvenBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto even = Iota(d, 1);
    const auto odd = Iota(d, static_cast<T>(1 + N));
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / (16 / sizeof(T));
      expected[i] = static_cast<T>(1 + i + ((idx_block & 1) ? N : 0));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), OddEvenBlocks(odd, even));
  }
};

HWY_NOINLINE void TestAllOddEvenBlocks() {
  ForAllTypes(ForGEVectors<128, TestOddEvenBlocks>());
}

struct TestSwapAdjacentBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    constexpr size_t kLanesPerBlock = 16 / sizeof(T);
    if (N < 2 * kLanesPerBlock) return;
    const auto vi = Iota(d, 1);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / kLanesPerBlock;
      const size_t base = (idx_block ^ 1) * kLanesPerBlock;
      const size_t mod = i % kLanesPerBlock;
      expected[i] = static_cast<T>(1 + base + mod);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), SwapAdjacentBlocks(vi));
  }
};

HWY_NOINLINE void TestAllSwapAdjacentBlocks() {
  ForAllTypes(ForGEVectors<128, TestSwapAdjacentBlocks>());
}

struct TestTableLookupLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const RebindToSigned<D> di;
    using TI = TFromD<decltype(di)>;
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    auto idx = AllocateAligned<TI>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(idx && expected);
    memset(idx.get(), 0, N * sizeof(TI));
    const auto v = Iota(d, 1);

    if (N <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < N; ++i0) {
        idx[0] = static_cast<TI>(i0);

        for (size_t i1 = 0; i1 < N; ++i1) {
          if (N >= 2) idx[1] = static_cast<TI>(i1);
          for (size_t i2 = 0; i2 < N; ++i2) {
            if (N >= 4) idx[2] = static_cast<TI>(i2);
            for (size_t i3 = 0; i3 < N; ++i3) {
              if (N >= 4) idx[3] = static_cast<TI>(i3);

              for (size_t i = 0; i < N; ++i) {
                expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
              const auto actual1 = TableLookupLanes(v, opaque1);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

              const auto opaque2 = SetTableIndices(d, idx.get());
              const auto actual2 = TableLookupLanes(v, opaque2);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
            }
          }
        }
      }
    } else {
      // Too many permutations to test exhaustively; choose one with repeated
      // and cross-block indices and ensure indices do not exceed #lanes.
      // For larger vectors, upper lanes will be zero.
      HWY_ALIGN TI idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                     15, 14, 14, 15, 4, 9, 8, 5};
      for (size_t i = 0; i < N; ++i) {
        idx[i] = (i < 16) ? idx_source[i] : 0;
        // Avoid undefined results / asan error for scalar by capping indices.
        if (idx[i] >= static_cast<TI>(N)) {
          idx[i] = static_cast<TI>(N - 1);
        }
        expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
      }

      const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
      const auto actual1 = TableLookupLanes(v, opaque1);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

      const auto opaque2 = SetTableIndices(d, idx.get());
      const auto actual2 = TableLookupLanes(v, opaque2);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
    }
#else
    const TI index = 0;
    const auto v = Set(d, 1);
    const auto opaque1 = SetTableIndices(d, &index);
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque1));
    const auto opaque2 = IndicesFromVec(d, Zero(di));
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque2));
#endif
  }
};

HWY_NOINLINE void TestAllTableLookupLanes() {
  ForAllTypes(ForPartialVectors<TestTableLookupLanes>());
}

struct TestTwoTablesLookupLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const RebindToUnsigned<D> du;
    using TU = TFromD<decltype(du)>;

    const size_t N = Lanes(d);
    const size_t twiceN = N * 2;
    auto idx = AllocateAligned<TU>(twiceN);
    auto expected = AllocateAligned<T>(twiceN);
    HWY_ASSERT(idx && expected);
    memset(idx.get(), 0, twiceN * sizeof(TU));
    const auto a = Iota(d, 1);
    const auto b = Add(a, Set(d, static_cast<T>(N)));

    if (twiceN <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < twiceN; ++i0) {
        idx[0] = static_cast<TU>(i0);

        for (size_t i1 = 0; i1 < twiceN; ++i1) {
          if (twiceN >= 2) idx[1] = static_cast<TU>(i1);
          for (size_t i2 = 0; i2 < twiceN; ++i2) {
            if (twiceN >= 4) idx[2] = static_cast<TU>(i2);
            for (size_t i3 = 0; i3 < twiceN; ++i3) {
              if (twiceN >= 4) idx[3] = static_cast<TU>(i3);

              for (size_t i = 0; i < twiceN; ++i) {
                expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque1_a = IndicesFromVec(d, Load(du, idx.get()));
              const auto opaque1_b = IndicesFromVec(d, Load(du, idx.get() + N));
              const auto actual1_a = TwoTablesLookupLanes(d, a, b, opaque1_a);
              const auto actual1_b = TwoTablesLookupLanes(d, a, b, opaque1_b);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual1_a);
              HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual1_b);

              const auto opaque2_a = SetTableIndices(d, idx.get());
              const auto opaque2_b = SetTableIndices(d, idx.get() + N);
              const auto actual2_a = TwoTablesLookupLanes(d, a, b, opaque2_a);
              const auto actual2_b = TwoTablesLookupLanes(d, a, b, opaque2_b);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual2_a);
              HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual2_b);
            }
          }
        }
      }
    } else {
      constexpr size_t kLanesPerBlock = 16 / sizeof(T);
      constexpr size_t kMaxBlockIdx = static_cast<size_t>(LimitsMax<TU>()) >> 1;
      static_assert(kMaxBlockIdx > 0, "kMaxBlockIdx > 0 must be true");

      const size_t num_of_blocks_per_vect = HWY_MAX(N / kLanesPerBlock, 1);
      const size_t num_of_blocks_to_check =
          HWY_MIN(num_of_blocks_per_vect * 2, kMaxBlockIdx);

      for (size_t i = 0; i < num_of_blocks_to_check; i++) {
        // Too many permutations to test exhaustively; choose one with repeated
        // and cross-block indices and ensure indices do not exceed #lanes.
        // For larger vectors, upper lanes will be zero.
        HWY_ALIGN TU idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                       15, 14, 14, 15, 4, 9, 8, 5};
        for (size_t j = 0; j < twiceN; ++j) {
          idx[j] = static_cast<TU>((i * kLanesPerBlock + idx_source[j & 15] +
                                    (j & static_cast<size_t>(-16))) &
                                   (twiceN - 1));
          expected[j] = static_cast<T>(idx[j] + 1);  // == v[idx[j]]
        }

        const auto opaque1_a = IndicesFromVec(d, Load(du, idx.get()));
        const auto opaque1_b = IndicesFromVec(d, Load(du, idx.get() + N));
        const auto actual1_a = TwoTablesLookupLanes(d, a, b, opaque1_a);
        const auto actual1_b = TwoTablesLookupLanes(d, a, b, opaque1_b);
        HWY_ASSERT_VEC_EQ(d, expected.get(), actual1_a);
        HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual1_b);

        const auto opaque2_a = SetTableIndices(d, idx.get());
        const auto opaque2_b = SetTableIndices(d, idx.get() + N);
        const auto actual2_a = TwoTablesLookupLanes(d, a, b, opaque2_a);
        const auto actual2_b = TwoTablesLookupLanes(d, a, b, opaque2_b);
        HWY_ASSERT_VEC_EQ(d, expected.get(), actual2_a);
        HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual2_b);
      }
    }
  }
};

HWY_NOINLINE void TestAllTwoTablesLookupLanes() {
  ForAllTypes(ForPartialVectors<TestTwoTablesLookupLanes>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwySwizzleTest);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllGetLane);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllExtractLane);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllInsertLane);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllDupEven);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllDupOdd);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllOddEven);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllOddEvenBlocks);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllSwapAdjacentBlocks);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllTableLookupLanes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllTwoTablesLookupLanes);
}  // namespace hwy

#endif
