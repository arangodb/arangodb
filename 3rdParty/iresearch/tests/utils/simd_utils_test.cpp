////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include <cstring>

#include "utils/misc.hpp"
#include "utils/std.hpp"
#include "utils/simd_utils.hpp"

TEST(simd_utils_test, delta32) {
  {
    HWY_ALIGN uint32_t values[1024];
    std::iota(std::begin(values), std::end(values), 42);

    // 128-bit
    {
      HWY_ALIGN uint32_t encoded[1024];
      std::memcpy(encoded, values, sizeof values);
      irs::simd::delta_encode<IRESEARCH_COUNTOF(encoded), true, uint32_t, 0>(encoded, encoded[0] - 1);
      ASSERT_TRUE(std::all_of(std::begin(encoded), std::end(encoded), [](auto v) { return 1 == v; }));
    }

#if HWY_CAP_GE256
    // 256-bit and greater
    {
      HWY_ALIGN uint32_t encoded[1024];
      std::memcpy(encoded, values, sizeof values);
      irs::simd::delta_encode<IRESEARCH_COUNTOF(encoded), true, uint32_t, 1>(encoded, encoded[0] - 1);
      ASSERT_TRUE(std::all_of(std::begin(encoded), std::end(encoded), [](auto v) { return 1 == v; }));
    }
#endif
  }

  {
    HWY_ALIGN uint32_t values[1024];
    std::iota(std::begin(values), std::end(values), 42);
    values[IRESEARCH_COUNTOF(values) / 2] = values[1 + (IRESEARCH_COUNTOF(values) / 2)];

    // 128-bit
    {
      HWY_ALIGN uint32_t encoded[1024];
      std::memcpy(encoded, values, sizeof values);
      irs::simd::delta_encode<IRESEARCH_COUNTOF(encoded), true, uint32_t, 0>(encoded, encoded[0] - 1);

      ASSERT_TRUE(std::all_of(
        std::begin(encoded),
        std::begin(encoded) + IRESEARCH_COUNTOF(values) / 2,
        [](auto v) { return 1 == v; }));
      ASSERT_EQ(2, encoded[IRESEARCH_COUNTOF(values) / 2]);
      ASSERT_EQ(0, encoded[1 + IRESEARCH_COUNTOF(values) / 2]);
      ASSERT_TRUE(std::all_of(
        2 + std::begin(encoded) + IRESEARCH_COUNTOF(values) / 2,
        std::end(encoded),
        [](auto v) { return 1 == v; }));
    }

#if HWY_CAP_GE256
    // 256-bit and greater
    {
      HWY_ALIGN uint32_t encoded[1024];
      std::memcpy(encoded, values, sizeof values);
      irs::simd::delta_encode<IRESEARCH_COUNTOF(encoded), true, uint32_t, 1>(encoded, encoded[0] - 1);
      ASSERT_TRUE(std::all_of(
        std::begin(encoded),
        std::begin(encoded) + IRESEARCH_COUNTOF(values) / 2,
        [](auto v) { return 1 == v; }));
      ASSERT_EQ(2, encoded[IRESEARCH_COUNTOF(values) / 2]);
      ASSERT_EQ(0, encoded[1 + IRESEARCH_COUNTOF(values) / 2]);
      ASSERT_TRUE(std::all_of(
        2 + std::begin(encoded) + IRESEARCH_COUNTOF(values) / 2,
        std::end(encoded),
        [](auto v) { return 1 == v; }));
    }
#endif
  }
}

TEST(simd_utils_test, avg) {
  HWY_ALIGN uint32_t values[1024];
  std::iota(std::begin(values), std::end(values), 42);

  HWY_ALIGN uint32_t encoded[1024];
  std::memcpy(encoded, values, sizeof values);
  const auto stats = irs::simd::avg_encode<IRESEARCH_COUNTOF(encoded), true>(encoded);

  HWY_ALIGN uint32_t decoded[IRESEARCH_COUNTOF(encoded)];
  irs::simd::avg_decode<IRESEARCH_COUNTOF(encoded), true>(encoded, decoded, stats.first, stats.second);

  ASSERT_TRUE(std::equal(std::begin(values), std::end(values),
                         std::begin(decoded), std::end(decoded)));
}

TEST(simd_utils_test, zigzag32) {
  auto expected = Iota(HWY_FULL(int32_t){}, 0);
  auto encoded = irs::simd::zig_zag_encode(expected);
  auto decoded = irs::simd::zig_zag_decode(encoded);
  ASSERT_TRUE(AllTrue(expected == decoded));
}

TEST(simd_utils_test, zigzag64) {
  auto expected = Iota(HWY_FULL(int64_t){}, 0);
  auto encoded = irs::simd::zig_zag_encode(expected);
  auto decoded = irs::simd::zig_zag_decode(encoded);
  ASSERT_TRUE(AllTrue(expected == decoded));
}

TEST(simd_utils_test, all_equal) {
  constexpr size_t BLOCK_SIZE = 128;
  HWY_ALIGN uint32_t values[BLOCK_SIZE*2];
  std::fill(std::begin(values), std::end(values), 42);
  ASSERT_TRUE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)));
  ASSERT_TRUE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)-1));
  ASSERT_TRUE(irs::simd::all_equal<true>(std::begin(values), 31));
  ASSERT_TRUE(irs::simd::all_equal<true>(std::begin(values), 33));

  values[0] = 0;
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)-1));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), 31));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), 33));

  {
    auto* begin = values;
    for (size_t i = 0; i < 32; ++i) {
      begin[0] = 0;
      begin[1] = 1;
      begin[2] = 2;
      begin[3] = 4;
      begin += 4;
    }
  }
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), IRESEARCH_COUNTOF(values)-1));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), 31));
  ASSERT_FALSE(irs::simd::all_equal<true>(std::begin(values), 33));
}

TEST(simd_utils_test, maxmin) {
  constexpr size_t BLOCK_SIZE = 128;
  HWY_ALIGN uint32_t values[BLOCK_SIZE*2];
  std::iota(std::begin(values), std::end(values), 42);
  ASSERT_EQ(
    (std::pair<uint32_t, uint32_t>(42, 42 + IRESEARCH_COUNTOF(values) - 1)),
    (irs::simd::maxmin<IRESEARCH_COUNTOF(values), true>(values)));
  ASSERT_EQ(
    (std::pair<uint32_t, uint32_t>(42, 42 + IRESEARCH_COUNTOF(values) - 2)),
    (irs::simd::maxmin<IRESEARCH_COUNTOF(values)-1, true>(values)));
}

TEST(simd_utils_test, maxbits) {
  constexpr size_t BLOCK_SIZE = 128;

  // 32-bit
  {
    HWY_ALIGN uint32_t values[BLOCK_SIZE*2];
    std::iota(std::begin(values), std::end(values), 42);
    const auto max = *std::max_element(std::begin(values), std::end(values));
    ASSERT_EQ(irs::packed::maxbits32(max), irs::simd::maxbits<true>(values, IRESEARCH_COUNTOF(values)));
  }

  // 32-bit
  {
    HWY_ALIGN uint32_t values[BLOCK_SIZE-1];
    std::iota(std::begin(values), std::end(values), 42);
    const auto max = *std::max_element(std::begin(values), std::end(values));
    ASSERT_EQ(irs::packed::maxbits32(max), irs::simd::maxbits<true>(values, IRESEARCH_COUNTOF(values)));
  }

  // 64-bit
  {
    HWY_ALIGN uint64_t values[BLOCK_SIZE*2];
    std::iota(std::begin(values), std::end(values), 42);
    const auto max = *std::max_element(std::begin(values), std::end(values));
    ASSERT_EQ(irs::packed::maxbits64(max), irs::simd::maxbits<true>(values, IRESEARCH_COUNTOF(values)));
  }

  // 64-bit
  {
    HWY_ALIGN uint64_t values[BLOCK_SIZE-1];
    std::iota(std::begin(values), std::end(values), 42);
    const auto max = *std::max_element(std::begin(values), std::end(values));
    ASSERT_EQ(irs::packed::maxbits64(max), irs::simd::maxbits<true>(values, IRESEARCH_COUNTOF(values)));
  }
}
