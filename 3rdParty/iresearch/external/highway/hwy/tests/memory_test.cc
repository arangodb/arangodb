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
    const auto v = LoadDup128(d, lanes);
    const size_t N = Lanes(d);
    auto out = AllocateAligned<T>(N);
    Store(v, d, out.get());
    for (size_t i = 0; i < N; ++i) {
      HWY_ASSERT_EQ(T(i % N128 + 1), out[i]);
    }
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

struct TestGather {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Offset = MakeSigned<T>;

    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be gathered from
    const size_t max_bytes = 4 * N * sizeof(T);  // upper bound on offset
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
        offsets[i] =
            static_cast<Offset>(Random32(&rng) % (max_bytes - sizeof(T)));
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
  const ForPartialVectors<TestGather, 1, 1, HWY_GATHER_LANES(uint32_t)> test32;
  test32(uint32_t());
  test32(int32_t());

#if HWY_CAP_INTEGER64
  const ForPartialVectors<TestGather, 1, 1, HWY_GATHER_LANES(uint64_t)> test64;
  test64(uint64_t());
  test64(int64_t());
#endif

  ForPartialVectors<TestGather, 1, 1, HWY_GATHER_LANES(float)>()(float());

#if HWY_CAP_FLOAT64
  ForPartialVectors<TestGather, 1, 1, HWY_GATHER_LANES(double)>()(double());
#endif
}

HWY_NOINLINE void TestAllCache() {
  LoadFence();
  StoreFence();
  int test = 0;
  Prefetch(&test);
  FlushCacheline(&test);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
HWY_BEFORE_TEST(HwyMemoryTest);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllLoadStore);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllLoadDup128);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllStream);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllGather);
HWY_EXPORT_AND_TEST_P(HwyMemoryTest, TestAllCache);
HWY_AFTER_TEST();
#endif
