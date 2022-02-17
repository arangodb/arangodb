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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/blockwise_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestShiftBytes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define Shift*Bytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const Repartition<uint8_t, D> du8;
    const size_t N8 = Lanes(du8);

    // Zero remains zero
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(v0));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(d, v0));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(d, v0));

    // Zero after shifting out the high/low byte
    auto bytes = AllocateAligned<uint8_t>(N8);
    std::fill(bytes.get(), bytes.get() + N8, 0);
    bytes[N8 - 1] = 0x7F;
    const auto vhi = BitCast(d, Load(du8, bytes.get()));
    bytes[N8 - 1] = 0;
    bytes[0] = 0x7F;
    const auto vlo = BitCast(d, Load(du8, bytes.get()));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(vhi));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(d, vhi));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(d, vlo));

    // Check expected result with Iota
    const size_t N = Lanes(d);
    auto in = AllocateAligned<T>(N);
    const uint8_t* in_bytes = reinterpret_cast<const uint8_t*>(in.get());
    const auto v = BitCast(d, Iota(du8, 1));
    Store(v, d, in.get());

    auto expected = AllocateAligned<T>(N);
    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    const size_t kBlockSize = HWY_MIN(N8, 16);
    for (size_t block = 0; block < N8; block += kBlockSize) {
      expected_bytes[block] = 0;
      memcpy(expected_bytes + block + 1, in_bytes + block, kBlockSize - 1);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftBytes<1>(v));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftBytes<1>(d, v));

    for (size_t block = 0; block < N8; block += kBlockSize) {
      memcpy(expected_bytes + block, in_bytes + block + 1, kBlockSize - 1);
      expected_bytes[block + kBlockSize - 1] = 0;
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightBytes<1>(d, v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftBytes() {
  ForIntegerTypes(ForPartialVectors<TestShiftBytes>());
}

struct TestShiftLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define Shift*Lanes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const auto v = Iota(d, T(1));
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    HWY_ASSERT_VEC_EQ(d, v, ShiftLeftLanes<0>(v));
    HWY_ASSERT_VEC_EQ(d, v, ShiftLeftLanes<0>(d, v));
    HWY_ASSERT_VEC_EQ(d, v, ShiftRightLanes<0>(d, v));

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);

    for (size_t i = 0; i < N; ++i) {
      expected[i] = (i % kLanesPerBlock) == 0 ? T(0) : T(i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftLanes<1>(v));
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftLanes<1>(d, v));

    for (size_t i = 0; i < N; ++i) {
      const size_t mod = i % kLanesPerBlock;
      expected[i] = mod == (kLanesPerBlock - 1) || i >= N - 1 ? T(0) : T(2 + i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightLanes<1>(d, v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftLanes() {
  ForAllTypes(ForPartialVectors<TestShiftLanes>());
}

template <typename D, int kLane>
struct TestBroadcastR {
  HWY_NOINLINE void operator()() const {
    using T = typename D::T;
    const D d;
    const size_t N = Lanes(d);
    if (kLane >= N) return;
    auto in_lanes = AllocateAligned<T>(N);
    std::fill(in_lanes.get(), in_lanes.get() + N, T(0));
    const size_t blockN = HWY_MIN(N * sizeof(T), 16) / sizeof(T);
    // Need to set within each 128-bit block
    for (size_t block = 0; block < N; block += blockN) {
      in_lanes[block + kLane] = static_cast<T>(block + 1);
    }
    const auto in = Load(d, in_lanes.get());
    auto expected = AllocateAligned<T>(N);
    for (size_t block = 0; block < N; block += blockN) {
      for (size_t i = 0; i < blockN; ++i) {
        expected[block + i] = T(block + 1);
      }
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Broadcast<kLane>(in));

    TestBroadcastR<D, kLane - 1>()();
  }
};

template <class D>
struct TestBroadcastR<D, -1> {
  void operator()() const {}
};

struct TestBroadcast {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    TestBroadcastR<D, HWY_MIN(MaxLanes(d), 16 / sizeof(T)) - 1>()();
  }
};

HWY_NOINLINE void TestAllBroadcast() {
  const ForPartialVectors<TestBroadcast> test;
  // No u8.
  test(uint16_t());
  test(uint32_t());
#if HWY_CAP_INTEGER64
  test(uint64_t());
#endif

  // No i8.
  test(int16_t());
  test(int32_t());
#if HWY_CAP_INTEGER64
  test(int64_t());
#endif

  ForFloatTypes(test);
}

template <bool kFull>
struct ChooseTableSize {
  template <typename T, typename DIdx>
  using type = DIdx;
};
template <>
struct ChooseTableSize<true> {
  template <typename T, typename DIdx>
  using type = HWY_FULL(T);
};

template <bool kFull>
struct TestTableLookupBytes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    RandomState rng;
    const typename ChooseTableSize<kFull>::template type<T, D> d_tbl;
    const Repartition<uint8_t, decltype(d_tbl)> d_tbl8;
    const size_t NT8 = Lanes(d_tbl8);

    const Repartition<uint8_t, D> d8;
    const size_t N = Lanes(d);
    const size_t N8 = Lanes(d8);

    // Random input bytes
    auto in_bytes = AllocateAligned<uint8_t>(NT8);
    for (size_t i = 0; i < NT8; ++i) {
      in_bytes[i] = Random32(&rng) & 0xFF;
    }
    const auto in = BitCast(d_tbl, Load(d_tbl8, in_bytes.get()));

    // Enough test data; for larger vectors, upper lanes will be zero.
    const uint8_t index_bytes_source[64] = {
        // Same index as source, multiple outputs from same input,
        // unused input (9), ascending/descending and nonconsecutive neighbors.
        0,  2,  1, 2, 15, 12, 13, 14, 6,  7,  8,  5,  4,  3,  10, 11,
        11, 10, 3, 4, 5,  8,  7,  6,  14, 13, 12, 15, 2,  1,  2,  0,
        4,  3,  2, 2, 5,  6,  7,  7,  15, 15, 15, 15, 15, 15, 0,  1};
    auto index_bytes = AllocateAligned<uint8_t>(N8);
    const size_t max_index = HWY_MIN(N8, 16) - 1;
    for (size_t i = 0; i < N8; ++i) {
      index_bytes[i] = (i < 64) ? index_bytes_source[i] : 0;
      // Avoid asan error for partial vectors.
      index_bytes[i] = static_cast<uint8_t>(HWY_MIN(index_bytes[i], max_index));
    }
    const auto indices = Load(d, reinterpret_cast<const T*>(index_bytes.get()));

    auto expected = AllocateAligned<T>(N);
    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    for (size_t block = 0; block < N8; block += 16) {
      for (size_t i = 0; i < 16 && (block + i) < N8; ++i) {
        const uint8_t index = index_bytes[block + i];
        HWY_ASSERT(block + index < N8);  // indices were already capped to N8.
        // For large vectors, the lane index may wrap around due to block.
        expected_bytes[block + i] = in_bytes[(block & 0xFF) + index];
      }
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), TableLookupBytes(in, indices));

    // Individually test zeroing each byte position.
    for (size_t i = 0; i < N8; ++i) {
      const uint8_t prev_expected = expected_bytes[i];
      const uint8_t prev_index = index_bytes[i];
      expected_bytes[i] = 0;

      const int idx = 0x80 + (int(Random32(&rng) & 7) << 4);
      HWY_ASSERT(0x80 <= idx && idx < 256);
      index_bytes[i] = static_cast<uint8_t>(idx);

      const auto indices =
          Load(d, reinterpret_cast<const T*>(index_bytes.get()));
      HWY_ASSERT_VEC_EQ(d, expected.get(), TableLookupBytesOr0(in, indices));
      expected_bytes[i] = prev_expected;
      index_bytes[i] = prev_index;
    }
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllTableLookupBytes() {
  // Partial index, same-sized table.
  ForIntegerTypes(ForPartialVectors<TestTableLookupBytes<false>>());

// TODO(janwas): requires LMUL trunc/ext, which is not yet implemented.
#if HWY_TARGET != HWY_RVV
  // Partial index, full-size table.
  ForIntegerTypes(ForPartialVectors<TestTableLookupBytes<true>>());
#endif
}

struct TestInterleaveLower {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TU = MakeUnsigned<T>;
    const size_t N = Lanes(d);
    auto even_lanes = AllocateAligned<T>(N);
    auto odd_lanes = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      even_lanes[i] = static_cast<T>(2 * i + 0);
      odd_lanes[i] = static_cast<T>(2 * i + 1);
    }
    const auto even = Load(d, even_lanes.get());
    const auto odd = Load(d, odd_lanes.get());

    const size_t blockN = HWY_MIN(16 / sizeof(T), N);
    for (size_t i = 0; i < Lanes(d); ++i) {
      const size_t block = i / blockN;
      const size_t index = (i % blockN) + block * 2 * blockN;
      expected[i] = static_cast<T>(index & LimitsMax<TU>());
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), InterleaveLower(even, odd));
    HWY_ASSERT_VEC_EQ(d, expected.get(), InterleaveLower(d, even, odd));
  }
};

struct TestInterleaveUpper {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    if (N == 1) return;
    auto even_lanes = AllocateAligned<T>(N);
    auto odd_lanes = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      even_lanes[i] = static_cast<T>(2 * i + 0);
      odd_lanes[i] = static_cast<T>(2 * i + 1);
    }
    const auto even = Load(d, even_lanes.get());
    const auto odd = Load(d, odd_lanes.get());

    const size_t blockN = HWY_MIN(16 / sizeof(T), N);
    for (size_t i = 0; i < Lanes(d); ++i) {
      const size_t block = i / blockN;
      expected[i] = T((i % blockN) + block * 2 * blockN + blockN);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), InterleaveUpper(d, even, odd));
  }
};

HWY_NOINLINE void TestAllInterleave() {
  // Not DemoteVectors because this cannot be supported by HWY_SCALAR.
  ForAllTypes(ForShrinkableVectors<TestInterleaveLower>());
  ForAllTypes(ForShrinkableVectors<TestInterleaveUpper>());
}

struct TestZipLower {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using WideT = MakeWide<T>;
    static_assert(sizeof(T) * 2 == sizeof(WideT), "Must be double-width");
    static_assert(IsSigned<T>() == IsSigned<WideT>(), "Must have same sign");
    const size_t N = Lanes(d);
    auto even_lanes = AllocateAligned<T>(N);
    auto odd_lanes = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      even_lanes[i] = static_cast<T>(2 * i + 0);
      odd_lanes[i] = static_cast<T>(2 * i + 1);
    }
    const auto even = Load(d, even_lanes.get());
    const auto odd = Load(d, odd_lanes.get());

    const Repartition<WideT, D> dw;
    const size_t NW = Lanes(dw);
    auto expected = AllocateAligned<WideT>(NW);
    const size_t blockN = HWY_MIN(size_t(16) / sizeof(WideT), NW);

    for (size_t i = 0; i < NW; ++i) {
      const size_t block = i / blockN;
      // Value of least-significant lane in lo-vector.
      const size_t lo = 2u * (i % blockN) + 4u * block * blockN;
      const size_t kBits = sizeof(T) * 8;
      expected[i] = static_cast<WideT>((static_cast<WideT>(lo + 1) << kBits) +
                                       static_cast<WideT>(lo));
    }
    HWY_ASSERT_VEC_EQ(dw, expected.get(), ZipLower(even, odd));
    HWY_ASSERT_VEC_EQ(dw, expected.get(), ZipLower(dw, even, odd));
  }
};

struct TestZipUpper {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using WideT = MakeWide<T>;
    static_assert(sizeof(T) * 2 == sizeof(WideT), "Must be double-width");
    static_assert(IsSigned<T>() == IsSigned<WideT>(), "Must have same sign");
    const size_t N = Lanes(d);
    if (N < 16 / sizeof(T)) return;
    auto even_lanes = AllocateAligned<T>(N);
    auto odd_lanes = AllocateAligned<T>(N);
    for (size_t i = 0; i < Lanes(d); ++i) {
      even_lanes[i] = static_cast<T>(2 * i + 0);
      odd_lanes[i] = static_cast<T>(2 * i + 1);
    }
    const auto even = Load(d, even_lanes.get());
    const auto odd = Load(d, odd_lanes.get());

        const Repartition<WideT, D> dw;
    const size_t NW = Lanes(dw);
    auto expected = AllocateAligned<WideT>(NW);
    const size_t blockN = HWY_MIN(size_t(16) / sizeof(WideT), NW);

    for (size_t i = 0; i < NW; ++i) {
      const size_t block = i / blockN;
      const size_t lo = 2u * (i % blockN) + 4u * block * blockN;
      const size_t kBits = sizeof(T) * 8;
      expected[i] = static_cast<WideT>(
          (static_cast<WideT>(lo + 2 * blockN + 1) << kBits) +
          static_cast<WideT>(lo + 2 * blockN));
    }
    HWY_ASSERT_VEC_EQ(dw, expected.get(), ZipUpper(dw, even, odd));
  }
};

HWY_NOINLINE void TestAllZip() {
  const ForDemoteVectors<TestZipLower> lower_unsigned;
  // TODO(janwas): enable after LowerHalf available
#if HWY_TARGET != HWY_RVV
  lower_unsigned(uint8_t());
#endif
  lower_unsigned(uint16_t());
#if HWY_CAP_INTEGER64
  lower_unsigned(uint32_t());  // generates u64
#endif

  const ForDemoteVectors<TestZipLower> lower_signed;
#if HWY_TARGET != HWY_RVV
  lower_signed(int8_t());
#endif
  lower_signed(int16_t());
#if HWY_CAP_INTEGER64
  lower_signed(int32_t());  // generates i64
#endif

  const ForShrinkableVectors<TestZipUpper> upper_unsigned;
#if HWY_TARGET != HWY_RVV
  upper_unsigned(uint8_t());
#endif
  upper_unsigned(uint16_t());
#if HWY_CAP_INTEGER64
  upper_unsigned(uint32_t());  // generates u64
#endif

  const ForShrinkableVectors<TestZipUpper> upper_signed;
#if HWY_TARGET != HWY_RVV
  upper_signed(int8_t());
#endif
  upper_signed(int16_t());
#if HWY_CAP_INTEGER64
  upper_signed(int32_t());  // generates i64
#endif

  // No float - concatenating f32 does not result in a f64
}

template <int kBytes>
struct TestCombineShiftRightBytesR {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
// Scalar does not define CombineShiftRightBytes.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const size_t kBlockSize = 16;
    static_assert(kBytes < kBlockSize, "Shift count is per block");
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    if (N8 < 16) return;
    auto hi_bytes = AllocateAligned<uint8_t>(N8);
    auto lo_bytes = AllocateAligned<uint8_t>(N8);
    auto expected_bytes = AllocateAligned<uint8_t>(N8);
    uint8_t combined[2 * kBlockSize];

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N8; ++i) {
        hi_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
        lo_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
      }
      for (size_t i = 0; i < N8; i += kBlockSize) {
        CopyBytes<kBlockSize>(&lo_bytes[i], combined);
        CopyBytes<kBlockSize>(&hi_bytes[i], combined + kBlockSize);
        CopyBytes<kBlockSize>(combined + kBytes, &expected_bytes[i]);
      }

      const auto hi = BitCast(d, Load(d8, hi_bytes.get()));
      const auto lo = BitCast(d, Load(d8, lo_bytes.get()));
      const auto expected = BitCast(d, Load(d8, expected_bytes.get()));
      HWY_ASSERT_VEC_EQ(d, expected, CombineShiftRightBytes<kBytes>(d, hi, lo));
    }

    TestCombineShiftRightBytesR<kBytes - 1>()(t, d);
#else
    (void)t;
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

template <int kLanes>
struct TestCombineShiftRightLanesR {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
// Scalar does not define CombineShiftRightBytes (needed for *Lanes).
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    const Repartition<uint8_t, D> d8;
    const size_t N8 = Lanes(d8);
    if (N8 < 16) return;

    auto hi_bytes = AllocateAligned<uint8_t>(N8);
    auto lo_bytes = AllocateAligned<uint8_t>(N8);
    auto expected_bytes = AllocateAligned<uint8_t>(N8);
    const size_t kBlockSize = 16;
    uint8_t combined[2 * kBlockSize];

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N8; ++i) {
        hi_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
        lo_bytes[i] = static_cast<uint8_t>(Random64(&rng) & 0xFF);
      }
      for (size_t i = 0; i < N8; i += kBlockSize) {
        CopyBytes<kBlockSize>(&lo_bytes[i], combined);
        CopyBytes<kBlockSize>(&hi_bytes[i], combined + kBlockSize);
        CopyBytes<kBlockSize>(combined + kLanes * sizeof(T),
                              &expected_bytes[i]);
      }

      const auto hi = BitCast(d, Load(d8, hi_bytes.get()));
      const auto lo = BitCast(d, Load(d8, lo_bytes.get()));
      const auto expected = BitCast(d, Load(d8, expected_bytes.get()));
      HWY_ASSERT_VEC_EQ(d, expected, CombineShiftRightLanes<kLanes>(d, hi, lo));
    }

    TestCombineShiftRightLanesR<kLanes - 1>()(t, d);
#else
    (void)t;
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

template <>
struct TestCombineShiftRightBytesR<0> {
  template <class T, class D>
  void operator()(T /*unused*/, D /*unused*/) {}
};

template <>
struct TestCombineShiftRightLanesR<0> {
  template <class T, class D>
  void operator()(T /*unused*/, D /*unused*/) {}
};

struct TestCombineShiftRight {
  template <class T, class D>
  HWY_NOINLINE void operator()(T t, D d) {
    constexpr int kMaxBytes = HWY_MIN(16, int(MaxLanes(d) * sizeof(T)));
    TestCombineShiftRightBytesR<kMaxBytes - 1>()(t, d);
    TestCombineShiftRightLanesR<kMaxBytes / int(sizeof(T)) - 1>()(t, d);
  }
};

HWY_NOINLINE void TestAllCombineShiftRight() {
  // Need at least 2 lanes.
  ForAllTypes(ForShrinkableVectors<TestCombineShiftRight>());
}

class TestSpecialShuffle32 {
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, 0);
    VerifyLanes32(d, Shuffle2301(v), 2, 3, 0, 1, __FILE__, __LINE__);
    VerifyLanes32(d, Shuffle1032(v), 1, 0, 3, 2, __FILE__, __LINE__);
    VerifyLanes32(d, Shuffle0321(v), 0, 3, 2, 1, __FILE__, __LINE__);
    VerifyLanes32(d, Shuffle2103(v), 2, 1, 0, 3, __FILE__, __LINE__);
    VerifyLanes32(d, Shuffle0123(v), 0, 1, 2, 3, __FILE__, __LINE__);
  }

 private:
  template <class D, class V>
  HWY_NOINLINE void VerifyLanes32(D d, VecArg<V> actual, const size_t i3,
                                  const size_t i2, const size_t i1,
                                  const size_t i0, const char* filename,
                                  const int line) {
    using T = TFromD<D>;
    constexpr size_t kBlockN = 16 / sizeof(T);
    const size_t N = Lanes(d);
    if (N < 4) return;
    auto expected = AllocateAligned<T>(N);
    for (size_t block = 0; block < N; block += kBlockN) {
      expected[block + 3] = static_cast<T>(block + i3);
      expected[block + 2] = static_cast<T>(block + i2);
      expected[block + 1] = static_cast<T>(block + i1);
      expected[block + 0] = static_cast<T>(block + i0);
    }
    AssertVecEqual(d, expected.get(), actual, filename, line);
  }
};

class TestSpecialShuffle64 {
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, 0);
    VerifyLanes64(d, Shuffle01(v), 0, 1, __FILE__, __LINE__);
  }

 private:
  template <class D, class V>
  HWY_NOINLINE void VerifyLanes64(D d, VecArg<V> actual, const size_t i1,
                                  const size_t i0, const char* filename,
                                  const int line) {
    using T = TFromD<D>;
    constexpr size_t kBlockN = 16 / sizeof(T);
    const size_t N = Lanes(d);
    if (N < 2) return;
    auto expected = AllocateAligned<T>(N);
    for (size_t block = 0; block < N; block += kBlockN) {
      expected[block + 1] = static_cast<T>(block + i1);
      expected[block + 0] = static_cast<T>(block + i0);
    }
    AssertVecEqual(d, expected.get(), actual, filename, line);
  }
};

HWY_NOINLINE void TestAllSpecialShuffles() {
  const ForGE128Vectors<TestSpecialShuffle32> test32;
  test32(uint32_t());
  test32(int32_t());
  test32(float());

#if HWY_CAP_INTEGER64
  const ForGE128Vectors<TestSpecialShuffle64> test64;
  test64(uint64_t());
  test64(int64_t());
#endif

#if HWY_CAP_FLOAT64
  const ForGE128Vectors<TestSpecialShuffle64> test_d;
  test_d(double());
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyBlockwiseTest);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllShiftBytes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllShiftLanes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllBroadcast);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllTableLookupBytes);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllInterleave);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllZip);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllCombineShiftRight);
HWY_EXPORT_AND_TEST_P(HwyBlockwiseTest, TestAllSpecialShuffles);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif
