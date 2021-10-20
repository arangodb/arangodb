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
#define HWY_TARGET_INCLUDE "tests/swizzle_test.cc"
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
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(v0));

    // Zero after shifting out the high/low byte
    auto bytes = AllocateAligned<uint8_t>(N8);
    std::fill(bytes.get(), bytes.get() + N8, 0);
    bytes[N8 - 1] = 0x7F;
    const auto vhi = BitCast(d, Load(du8, bytes.get()));
    bytes[N8 - 1] = 0;
    bytes[0] = 0x7F;
    const auto vlo = BitCast(d, Load(du8, bytes.get()));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftLeftBytes<1>(vhi));
    HWY_ASSERT_VEC_EQ(d, v0, ShiftRightBytes<1>(vlo));

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

    for (size_t block = 0; block < N8; block += kBlockSize) {
      memcpy(expected_bytes + block, in_bytes + block + 1, kBlockSize - 1);
      expected_bytes[block + kBlockSize - 1] = 0;
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightBytes<1>(v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftBytes() {
  ForIntegerTypes(ForGE128Vectors<TestShiftBytes>());
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
    HWY_ASSERT_VEC_EQ(d, v, ShiftRightLanes<0>(v));

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);

    for (size_t i = 0; i < N; ++i) {
      expected[i] = (i % kLanesPerBlock) == 0 ? T(0) : T(i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftLeftLanes<1>(v));

    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          (i % kLanesPerBlock) == (kLanesPerBlock - 1) ? T(0) : T(2 + i);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ShiftRightLanes<1>(v));
#else
    (void)d;
#endif  // #if HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllShiftLanes() {
  ForAllTypes(ForGE128Vectors<TestShiftLanes>());
}

template <typename D, int kLane>
struct TestBroadcastR {
  HWY_NOINLINE void operator()() const {
// TODO(janwas): fix failure
#if HWY_TARGET != HWY_WASM
    using T = typename D::T;
    const D d;
    const size_t N = Lanes(d);
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
#endif
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

struct TestTableLookupBytes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;
    const size_t N = Lanes(d);
    const size_t N8 = Lanes(Repartition<uint8_t, D>());
    auto in_bytes = AllocateAligned<uint8_t>(N8);
    for (size_t i = 0; i < N8; ++i) {
      in_bytes[i] = Random32(&rng) & 0xFF;
    }
    const auto in =
        BitCast(d, Load(d, reinterpret_cast<const T*>(in_bytes.get())));

    // Enough test data; for larger vectors, upper lanes will be zero.
    const uint8_t index_bytes_source[64] = {
        // Same index as source, multiple outputs from same input,
        // unused input (9), ascending/descending and nonconsecutive neighbors.
        0,  2,  1, 2, 15, 12, 13, 14, 6,  7,  8,  5,  4,  3,  10, 11,
        11, 10, 3, 4, 5,  8,  7,  6,  14, 13, 12, 15, 2,  1,  2,  0,
        4,  3,  2, 2, 5,  6,  7,  7,  15, 15, 15, 15, 15, 15, 0,  1};
    auto index_bytes = AllocateAligned<uint8_t>(N8);
    for (size_t i = 0; i < N8; ++i) {
      index_bytes[i] = (i < 64) ? index_bytes_source[i] : 0;
      // Avoid undefined results / asan error for scalar by capping indices.
      if (index_bytes[i] >= N * sizeof(T)) {
        index_bytes[i] = static_cast<uint8_t>(N * sizeof(T) - 1);
      }
    }
    const auto indices = Load(d, reinterpret_cast<const T*>(index_bytes.get()));
    auto expected = AllocateAligned<T>(N);
    uint8_t* expected_bytes = reinterpret_cast<uint8_t*>(expected.get());

    // Byte indices wrap around
    const size_t mod = HWY_MIN(N8, 256);
    for (size_t block = 0; block < N8; block += 16) {
      for (size_t i = 0; i < 16 && (block + i) < N8; ++i) {
        const uint8_t index = index_bytes[block + i];
        expected_bytes[block + i] = in_bytes[(block + index) % mod];
      }
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), TableLookupBytes(in, indices));
  }
};

HWY_NOINLINE void TestAllTableLookupBytes() {
  ForIntegerTypes(ForPartialVectors<TestTableLookupBytes>());
}
struct TestTableLookupLanes {
#if HWY_TARGET == HWY_RVV
  using Index = uint32_t;
#else
  using Index = int32_t;
#endif

  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    auto idx = AllocateAligned<Index>(N);
    std::fill(idx.get(), idx.get() + N, Index(0));
    auto expected = AllocateAligned<T>(N);
    const auto v = Iota(d, 1);

    if (N <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < N; ++i0) {
        idx[0] = static_cast<Index>(i0);
        for (size_t i1 = 0; i1 < N; ++i1) {
          idx[1] = static_cast<Index>(i1);
          for (size_t i2 = 0; i2 < N; ++i2) {
            idx[2] = static_cast<Index>(i2);
            for (size_t i3 = 0; i3 < N; ++i3) {
              idx[3] = static_cast<Index>(i3);

              for (size_t i = 0; i < N; ++i) {
                expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque = SetTableIndices(d, idx.get());
              const auto actual = TableLookupLanes(v, opaque);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual);
            }
          }
        }
      }
    } else {
      // Too many permutations to test exhaustively; choose one with repeated
      // and cross-block indices and ensure indices do not exceed #lanes.
      // For larger vectors, upper lanes will be zero.
      HWY_ALIGN Index idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                        15, 14, 14, 15, 4, 9, 8, 5};
      for (size_t i = 0; i < N; ++i) {
        idx[i] = (i < 16) ? idx_source[i] : 0;
        // Avoid undefined results / asan error for scalar by capping indices.
        if (idx[i] >= static_cast<Index>(N)) {
          idx[i] = static_cast<Index>(N - 1);
        }
        expected[i] = static_cast<T>(idx[i] + 1);  // == v[idx[i]]
      }

      const auto opaque = SetTableIndices(d, idx.get());
      const auto actual = TableLookupLanes(v, opaque);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual);
    }
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllTableLookupLanes() {
  const ForFullVectors<TestTableLookupLanes> test;
  test(uint32_t());
  test(int32_t());
  test(float());
}

struct TestInterleave {
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

    const size_t blockN = 16 / sizeof(T);
    for (size_t i = 0; i < Lanes(d); ++i) {
      const size_t block = i / blockN;
      const size_t index = (i % blockN) + block * 2 * blockN;
      expected[i] = static_cast<T>(index & LimitsMax<TU>());
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), InterleaveLower(even, odd));

    for (size_t i = 0; i < Lanes(d); ++i) {
      const size_t block = i / blockN;
      expected[i] = T((i % blockN) + block * 2 * blockN + blockN);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), InterleaveUpper(even, odd));
  }
};

HWY_NOINLINE void TestAllInterleave() {
  // Not supported by HWY_SCALAR: Interleave(f32, f32) would return f32x2.
  ForAllTypes(ForGE128Vectors<TestInterleave>());
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
    auto expected = AllocateAligned<WideT>(Lanes(dw));
    const WideT blockN = static_cast<WideT>(16 / sizeof(WideT));
    for (size_t i = 0; i < Lanes(dw); ++i) {
      const size_t block = i / blockN;
      // Value of least-significant lane in lo-vector.
      const WideT lo =
          static_cast<WideT>(2 * (i % blockN) + 4 * block * blockN);
      const WideT kBits = static_cast<WideT>(sizeof(T) * 8);
      expected[i] =
          static_cast<WideT>((static_cast<WideT>(lo + 1) << kBits) + lo);
    }
    HWY_ASSERT_VEC_EQ(dw, expected.get(), ZipLower(even, odd));
  }
};

struct TestZipUpper {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using WideT = MakeWide<T>;
    static_assert(sizeof(T) * 2 == sizeof(WideT), "Must be double-width");
    static_assert(IsSigned<T>() == IsSigned<WideT>(), "Must have same sign");
    const size_t N = Lanes(d);
    auto even_lanes = AllocateAligned<T>(N);
    auto odd_lanes = AllocateAligned<T>(N);
    for (size_t i = 0; i < Lanes(d); ++i) {
      even_lanes[i] = static_cast<T>(2 * i + 0);
      odd_lanes[i] = static_cast<T>(2 * i + 1);
    }
    const auto even = Load(d, even_lanes.get());
    const auto odd = Load(d, odd_lanes.get());

    const Repartition<WideT, D> dw;
    auto expected = AllocateAligned<WideT>(Lanes(dw));

    constexpr WideT blockN = static_cast<WideT>(16 / sizeof(WideT));
    for (size_t i = 0; i < Lanes(dw); ++i) {
      const size_t block = i / blockN;
      const WideT lo =
          static_cast<WideT>(2 * (i % blockN) + 4 * block * blockN);
      const WideT kBits = static_cast<WideT>(sizeof(T) * 8);
      expected[i] = static_cast<WideT>(
          (static_cast<WideT>(lo + 2 * blockN + 1) << kBits) + lo + 2 * blockN);
    }
    HWY_ASSERT_VEC_EQ(dw, expected.get(), ZipUpper(even, odd));
  }
};

HWY_NOINLINE void TestAllZip() {
  const ForPartialVectors<TestZipLower, 2> lower_unsigned;
  // TODO(janwas): fix
#if HWY_TARGET != HWY_RVV
  lower_unsigned(uint8_t());
#endif
  lower_unsigned(uint16_t());
#if HWY_CAP_INTEGER64
  lower_unsigned(uint32_t());  // generates u64
#endif

  const ForPartialVectors<TestZipLower, 2> lower_signed;
#if HWY_TARGET != HWY_RVV
  lower_signed(int8_t());
#endif
  lower_signed(int16_t());
#if HWY_CAP_INTEGER64
  lower_signed(int32_t());  // generates i64
#endif

  const ForGE128Vectors<TestZipUpper> upper_unsigned;
#if HWY_TARGET != HWY_RVV
  upper_unsigned(uint8_t());
#endif
  upper_unsigned(uint16_t());
#if HWY_CAP_INTEGER64
  upper_unsigned(uint32_t());  // generates u64
#endif

  const ForGE128Vectors<TestZipUpper> upper_signed;
#if HWY_TARGET != HWY_RVV
  upper_signed(int8_t());
#endif
  upper_signed(int16_t());
#if HWY_CAP_INTEGER64
  upper_signed(int32_t());  // generates i64
#endif

  // No float - concatenating f32 does not result in a f64
}

class TestSpecialShuffle32 {
 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, 0);

#define VERIFY_LANES_32(d, v, i3, i2, i1, i0) \
  VerifyLanes32((d), (v), (i3), (i2), (i1), (i0), __FILE__, __LINE__)

    VERIFY_LANES_32(d, Shuffle2301(v), 2, 3, 0, 1);
    VERIFY_LANES_32(d, Shuffle1032(v), 1, 0, 3, 2);
    VERIFY_LANES_32(d, Shuffle0321(v), 0, 3, 2, 1);
    VERIFY_LANES_32(d, Shuffle2103(v), 2, 1, 0, 3);
    VERIFY_LANES_32(d, Shuffle0123(v), 0, 1, 2, 3);

#undef VERIFY_LANES_32
  }

 private:
  template <class D, class V>
  HWY_NOINLINE void VerifyLanes32(D d, V v, const int i3, const int i2,
                                  const int i1, const int i0,
                                  const char* filename, const int line) {
    using T = typename D::T;
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    Store(v, d, lanes.get());
    const std::string name = TypeName(lanes[0], N);
    constexpr size_t kBlockN = 16 / sizeof(T);
    for (int block = 0; block < static_cast<int>(N); block += kBlockN) {
      AssertEqual(T(block + i3), lanes[block + 3], name, filename, line);
      AssertEqual(T(block + i2), lanes[block + 2], name, filename, line);
      AssertEqual(T(block + i1), lanes[block + 1], name, filename, line);
      AssertEqual(T(block + i0), lanes[block + 0], name, filename, line);
    }
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
  HWY_NOINLINE void VerifyLanes64(D d, V v, const int i1, const int i0,
                                  const char* filename, const int line) {
    using T = typename D::T;
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    Store(v, d, lanes.get());
    const std::string name = TypeName(lanes[0], N);
    constexpr size_t kBlockN = 16 / sizeof(T);
    for (int block = 0; block < static_cast<int>(N); block += kBlockN) {
      AssertEqual(T(block + i1), lanes[block + 1], name, filename, line);
      AssertEqual(T(block + i0), lanes[block + 0], name, filename, line);
    }
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

struct TestConcatHalves {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // TODO(janwas): fix
#if HWY_TARGET != HWY_RVV
    // Construct inputs such that interleaved halves == iota.
    const auto expected = Iota(d, 1);

    const size_t N = Lanes(d);
    auto lo = AllocateAligned<T>(N);
    auto hi = AllocateAligned<T>(N);
    size_t i;
    for (i = 0; i < N / 2; ++i) {
      lo[i] = static_cast<T>(1 + i);
      hi[i] = static_cast<T>(lo[i] + T(N) / 2);
    }
    for (; i < N; ++i) {
      lo[i] = hi[i] = 0;
    }

    HWY_ASSERT_VEC_EQ(d, expected,
                      ConcatLowerLower(Load(d, hi.get()), Load(d, lo.get())));

    // Same for high blocks.
    for (i = 0; i < N / 2; ++i) {
      lo[i] = hi[i] = 0;
    }
    for (; i < N; ++i) {
      lo[i] = static_cast<T>(1 + i - N / 2);
      hi[i] = static_cast<T>(lo[i] + T(N) / 2);
    }

    HWY_ASSERT_VEC_EQ(d, expected,
                      ConcatUpperUpper(Load(d, hi.get()), Load(d, lo.get())));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllConcatHalves() {
  ForAllTypes(ForGE128Vectors<TestConcatHalves>());
}

struct TestConcatLowerUpper {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // TODO(janwas): fix
#if HWY_TARGET != HWY_RVV
    const size_t N = Lanes(d);
    // Middle part of Iota(1) == Iota(1 + N / 2).
    const auto lo = Iota(d, 1);
    const auto hi = Iota(d, 1 + N);
    HWY_ASSERT_VEC_EQ(d, Iota(d, 1 + N / 2), ConcatLowerUpper(hi, lo));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllConcatLowerUpper() {
  ForAllTypes(ForGE128Vectors<TestConcatLowerUpper>());
}

struct TestConcatUpperLower {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto lo = Iota(d, 1);
    const auto hi = Iota(d, 1 + N);
    auto expected = AllocateAligned<T>(N);
    size_t i = 0;
    for (; i < N / 2; ++i) {
      expected[i] = static_cast<T>(1 + i);
    }
    for (; i < N; ++i) {
      expected[i] = static_cast<T>(1 + i + N);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ConcatUpperLower(hi, lo));
  }
};

HWY_NOINLINE void TestAllConcatUpperLower() {
  ForAllTypes(ForGE128Vectors<TestConcatUpperLower>());
}

struct TestOddEven {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto even = Iota(d, 1);
    const auto odd = Iota(d, 1 + N);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>(1 + i + ((i & 1) ? N : 0));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), OddEven(odd, even));
  }
};

HWY_NOINLINE void TestAllOddEven() {
  ForAllTypes(ForGE128Vectors<TestOddEven>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
HWY_BEFORE_TEST(HwySwizzleTest);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllShiftBytes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllShiftLanes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllBroadcast);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllTableLookupBytes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllTableLookupLanes);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllInterleave);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllZip);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllSpecialShuffles);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllConcatHalves);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllConcatLowerUpper);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllConcatUpperLower);
HWY_EXPORT_AND_TEST_P(HwySwizzleTest, TestAllOddEven);
HWY_AFTER_TEST();
#endif
