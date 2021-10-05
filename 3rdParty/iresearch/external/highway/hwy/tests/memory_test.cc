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

// Ensure incompabilities with Windows macros (e.g. #define StoreFence) are
// detected. Must come before Highway headers.
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include <stddef.h>
#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/memory_test.cc"
#include "hwy/cache_control.h"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestLoadStore {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const auto hi = Iota(d, 1 + N);
    const auto lo = Iota(d, 1);
    auto lanes = AllocateAligned<T>(2 * N);
    Store(hi, d, &lanes[N]);
    Store(lo, d, &lanes[0]);

    // Aligned load
    const auto lo2 = Load(d, &lanes[0]);
    HWY_ASSERT_VEC_EQ(d, lo2, lo);

    // Aligned store
    auto lanes2 = AllocateAligned<T>(2 * N);
    Store(lo2, d, &lanes2[0]);
    Store(hi, d, &lanes2[N]);
    for (size_t i = 0; i < 2 * N; ++i) {
      HWY_ASSERT_EQ(lanes[i], lanes2[i]);
    }

    // Unaligned load
    const auto vu = LoadU(d, &lanes[1]);
    auto lanes3 = AllocateAligned<T>(N);
    Store(vu, d, lanes3.get());
    for (size_t i = 0; i < N; ++i) {
      HWY_ASSERT_EQ(T(i + 2), lanes3[i]);
    }

    // Unaligned store
    StoreU(lo2, d, &lanes2[N / 2]);
    size_t i = 0;
    for (; i < N / 2; ++i) {
      HWY_ASSERT_EQ(lanes[i], lanes2[i]);
    }
    for (; i < 3 * N / 2; ++i) {
      HWY_ASSERT_EQ(T(i - N / 2 + 1), lanes2[i]);
    }
    // Subsequent values remain unchanged.
    for (; i < 2 * N; ++i) {
      HWY_ASSERT_EQ(T(i + 1), lanes2[i]);
    }
  }
};

HWY_NOINLINE void TestAllLoadStore() {
  ForAllTypes(ForPartialVectors<TestLoadStore>());
}

struct TestStoreInterleaved3 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be interleaved
    auto bytes = AllocateAligned<uint8_t>(3 * N);
    for (size_t i = 0; i < 3 * N; ++i) {
      bytes[i] = static_cast<uint8_t>(Random32(&rng) & 0xFF);
    }
    const auto in0 = Load(d, &bytes[0 * N]);
    const auto in1 = Load(d, &bytes[1 * N]);
    const auto in2 = Load(d, &bytes[2 * N]);

    // Interleave here, ensure vector results match scalar
    auto expected = AllocateAligned<T>(4 * N);
    auto actual_aligned = AllocateAligned<T>(4 * N + 1);
    T* actual = actual_aligned.get() + 1;

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        expected[3 * i + 0] = bytes[0 * N + i];
        expected[3 * i + 1] = bytes[1 * N + i];
        expected[3 * i + 2] = bytes[2 * N + i];
        // Ensure we do not write more than 3*N bytes
        expected[3 * N + i] = actual[3 * N + i] = 0;
      }
      StoreInterleaved3(in0, in1, in2, d, actual);
      size_t pos = 0;
      if (!BytesEqual(expected.get(), actual, 4 * N, &pos)) {
        Print(d, "in0", in0, pos / 3);
        Print(d, "in1", in1, pos / 3);
        Print(d, "in2", in2, pos / 3);
        const size_t i = pos - pos % 3;
        fprintf(stderr, "interleaved %d %d %d  %d %d %d\n", actual[i],
                actual[i + 1], actual[i + 2], actual[i + 3], actual[i + 4],
                actual[i + 5]);
        HWY_ASSERT(false);
      }
    }
  }
};

HWY_NOINLINE void TestAllStoreInterleaved3() {
#if HWY_TARGET == HWY_RVV
  // Segments are limited to 8 registers, so we can only go up to LMUL=2.
  const ForExtendableVectors<TestStoreInterleaved3, 4> test;
#else
  const ForPartialVectors<TestStoreInterleaved3> test;
#endif
  test(uint8_t());
}

struct TestStoreInterleaved4 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be interleaved
    auto bytes = AllocateAligned<uint8_t>(4 * N);
    for (size_t i = 0; i < 4 * N; ++i) {
      bytes[i] = static_cast<uint8_t>(Random32(&rng) & 0xFF);
    }
    const auto in0 = Load(d, &bytes[0 * N]);
    const auto in1 = Load(d, &bytes[1 * N]);
    const auto in2 = Load(d, &bytes[2 * N]);
    const auto in3 = Load(d, &bytes[3 * N]);

    // Interleave here, ensure vector results match scalar
    auto expected = AllocateAligned<T>(5 * N);
    auto actual_aligned = AllocateAligned<T>(5 * N + 1);
    T* actual = actual_aligned.get() + 1;

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        expected[4 * i + 0] = bytes[0 * N + i];
        expected[4 * i + 1] = bytes[1 * N + i];
        expected[4 * i + 2] = bytes[2 * N + i];
        expected[4 * i + 3] = bytes[3 * N + i];
        // Ensure we do not write more than 4*N bytes
        expected[4 * N + i] = actual[4 * N + i] = 0;
      }
      StoreInterleaved4(in0, in1, in2, in3, d, actual);
      size_t pos = 0;
      if (!BytesEqual(expected.get(), actual, 5 * N, &pos)) {
        Print(d, "in0", in0, pos / 4);
        Print(d, "in1", in1, pos / 4);
        Print(d, "in2", in2, pos / 4);
        Print(d, "in3", in3, pos / 4);
        const size_t i = pos;
        fprintf(stderr, "interleaved %d %d %d %d  %d %d %d %d\n", actual[i],
                actual[i + 1], actual[i + 2], actual[i + 3], actual[i + 4],
                actual[i + 5], actual[i + 6], actual[i + 7]);
        HWY_ASSERT(false);
      }
    }
  }
};

HWY_NOINLINE void TestAllStoreInterleaved4() {
#if HWY_TARGET == HWY_RVV
  // Segments are limited to 8 registers, so we can only go up to LMUL=2.
  const ForExtendableVectors<TestStoreInterleaved4, 4> test;
#else
  const ForPartialVectors<TestStoreInterleaved4> test;
#endif
  test(uint8_t());
}

struct TestLoadDup128 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Scalar does not define LoadDup128.
#if HWY_TARGET != HWY_SCALAR || HWY_IDE
    constexpr size_t N128 = 16 / sizeof(T);
    alignas(16) T lanes[N128];
    for (size_t i = 0; i < N128; ++i) {
      lanes[i] = static_cast<T>(1 + i);
    }

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>(i % N128 + 1);
    }

    HWY_ASSERT_VEC_EQ(d, expected.get(), LoadDup128(d, lanes));
#else
    (void)d;
#endif
  }
};

HWY_NOINLINE void TestAllLoadDup128() {
  ForAllTypes(ForGE128Vectors<TestLoadDup128>());
}

struct TestStream {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v = Iota(d, T(1));
    const size_t affected_bytes =
        (Lanes(d) * sizeof(T) + HWY_STREAM_MULTIPLE - 1) &
        ~size_t(HWY_STREAM_MULTIPLE - 1);
    const size_t affected_lanes = affected_bytes / sizeof(T);
    auto out = AllocateAligned<T>(2 * affected_lanes);
    std::fill(out.get(), out.get() + 2 * affected_lanes, T(0));

    Stream(v, d, out.get());
    StoreFence();
    const auto actual = Load(d, out.get());
    HWY_ASSERT_VEC_EQ(d, v, actual);
    // Ensure Stream didn't modify more memory than expected
    for (size_t i = affected_lanes; i < 2 * affected_lanes; ++i) {
      HWY_ASSERT_EQ(T(0), out[i]);
    }
  }
};

HWY_NOINLINE void TestAllStream() {
  const ForPartialVectors<TestStream> test;
  // No u8,u16.
  test(uint32_t());
  test(uint64_t());
  // No i8,i16.
  test(int32_t());
  test(int64_t());
  ForFloatTypes(test);
}

// Assumes little-endian byte order!
struct TestScatter {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Offset = MakeSigned<T>;

    const size_t N = Lanes(d);
    const size_t range = 4 * N;                  // number of items to scatter
    const size_t max_bytes = range * sizeof(T);  // upper bound on offset

    RandomState rng;

    // Data to be scattered
    auto bytes = AllocateAligned<uint8_t>(max_bytes);
    for (size_t i = 0; i < max_bytes; ++i) {
      bytes[i] = static_cast<uint8_t>(Random32(&rng) & 0xFF);
    }
    const auto data = Load(d, reinterpret_cast<const T*>(bytes.get()));

    // Scatter into these regions, ensure vector results match scalar
    auto expected = AllocateAligned<T>(range);
    auto actual = AllocateAligned<T>(range);

    const Rebind<Offset, D> d_offsets;
    auto offsets = AllocateAligned<Offset>(N);  // or indices

    for (size_t rep = 0; rep < 100; ++rep) {
      // Byte offsets
      std::fill(expected.get(), expected.get() + range, T(0));
      std::fill(actual.get(), actual.get() + range, T(0));
      for (size_t i = 0; i < N; ++i) {
        // Must be aligned
        offsets[i] = static_cast<Offset>((Random32(&rng) % range) * sizeof(T));
        CopyBytes<sizeof(T)>(
            bytes.get() + i * sizeof(T),
            reinterpret_cast<uint8_t*>(expected.get()) + offsets[i]);
      }
      const auto voffsets = Load(d_offsets, offsets.get());
      ScatterOffset(data, d, actual.get(), voffsets);
      if (!BytesEqual(expected.get(), actual.get(), max_bytes)) {
        Print(d, "Data", data);
        Print(d_offsets, "Offsets", voffsets);
        HWY_ASSERT(false);
      }

      // Indices
      std::fill(expected.get(), expected.get() + range, T(0));
      std::fill(actual.get(), actual.get() + range, T(0));
      for (size_t i = 0; i < N; ++i) {
        offsets[i] = static_cast<Offset>(Random32(&rng) % range);
        CopyBytes<sizeof(T)>(bytes.get() + i * sizeof(T),
                             &expected[size_t(offsets[i])]);
      }
      const auto vindices = Load(d_offsets, offsets.get());
      ScatterIndex(data, d, actual.get(), vindices);
      if (!BytesEqual(expected.get(), actual.get(), max_bytes)) {
        Print(d, "Data", data);
        Print(d_offsets, "Indices", vindices);
        HWY_ASSERT(false);
      }
    }
  }
};

HWY_NOINLINE void TestAllScatter() {
  // No u8,u16,i8,i16.
  const ForPartialVectors<TestScatter> test;
  test(uint32_t());
  test(int32_t());

#if HWY_CAP_INTEGER64
  test(uint64_t());
  test(int64_t());
#endif

  ForFloatTypes(test);
}

struct TestGather {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Offset = MakeSigned<T>;

    const size_t N = Lanes(d);
    const size_t range = 4 * N;                  // number of items to gather
    const size_t max_bytes = range * sizeof(T);  // upper bound on offset

    RandomState rng;

    // Data to be gathered from
    auto bytes = AllocateAligned<uint8_t>(max_bytes);
    for (size_t i = 0; i < max_bytes; ++i) {
      bytes[i] = static_cast<uint8_t>(Random32(&rng) & 0xFF);
    }

    auto expected = AllocateAligned<T>(N);
    auto offsets = AllocateAligned<Offset>(N);
    auto indices = AllocateAligned<Offset>(N);

    for (size_t rep = 0; rep < 100; ++rep) {
      // Offsets
      for (size_t i = 0; i < N; ++i) {
        // Must be aligned
        offsets[i] = static_cast<Offset>((Random32(&rng) % range) * sizeof(T));
        CopyBytes<sizeof(T)>(bytes.get() + offsets[i], &expected[i]);
      }

      const Rebind<Offset, D> d_offset;
      const T* base = reinterpret_cast<const T*>(bytes.get());
      auto actual = GatherOffset(d, base, Load(d_offset, offsets.get()));
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual);

      // Indices
      for (size_t i = 0; i < N; ++i) {
        indices[i] =
            static_cast<Offset>(Random32(&rng) % (max_bytes / sizeof(T)));
        CopyBytes<sizeof(T)>(base + indices[i], &expected[i]);
      }
      actual = GatherIndex(d, base, Load(d_offset, indices.get()));
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual);
    }
  }
};

HWY_NOINLINE void TestAllGather() {
  // No u8,u16,i8,i16.
  const ForPartialVectors<TestGather> test;
  test(uint32_t());
  test(int32_t());

#if HWY_CAP_INTEGER64
  test(uint64_t());
  test(int64_t());
#endif
  ForFloatTypes(test);
}

HWY_NOINLINE void TestAllCache() {
  LoadFence();
  StoreFence();
  int test = 0;
  Prefetch(&test);
  FlushCacheline(&test);
  Pause();
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyMemoryTest);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllLoadStore);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllStoreInterleaved3);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllStoreInterleaved4);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllLoadDup128);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllStream);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllScatter);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllGather);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllCache);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif
