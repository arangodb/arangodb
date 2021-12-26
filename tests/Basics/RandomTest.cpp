////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Random/RandomGenerator.h"

using namespace arangodb;

TEST(RandomGeneratorTest, test_RandomGeneratorTest_interval_uint16_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr uint16_t bounds[] = {1, 2, 4, 1023, 1024, 65535};
  for (uint16_t bound : bounds) {
    for (int i = 0; i < 10000; ++i) {
      uint16_t value = RandomGenerator::interval(bound);
      ASSERT_LE(value, bound);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_interval_uint32_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr uint32_t bounds[] = {1,    2,     4,     1023,
                                 1024, 65535, 65536, 4294967295ULL};
  for (uint32_t bound : bounds) {
    for (int i = 0; i < 10000; ++i) {
      uint32_t value = RandomGenerator::interval(bound);
      ASSERT_LE(value, bound);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_interval_uint64_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr uint64_t bounds[] = {1, 2, 4, 1023, 1024, 65535, 65536, 4294967295ULL, 4294967296ULL, 18446744073709551615ULL};
  for (uint64_t bound : bounds) {
    for (int i = 0; i < 10000; ++i) {
      uint64_t value = RandomGenerator::interval(bound);
      ASSERT_LE(value, bound);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_ranges_int16_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr std::pair<int16_t, int16_t> bounds[] = {
      {0, 0},         {0, 1},         {0, 2},         {0, 1023},
      {0, 1024},      {0, 32760},     {0, 32767},     {1, 1},
      {1, 2},         {1, 1023},      {1, 1024},      {1, 32766},
      {1, 32767},     {10, 10},       {10, 11},       {10, 32766},
      {10, 32767},    {1024, 32760},  {1024, 32766},  {1024, 32767},
      {32760, 32761}, {32760, 32765}, {32760, 32766}, {32760, 32767},
      {32766, 32766}, {32766, 32767}, {32767, 32767},
  };
  for (auto [lower, upper] : bounds) {
    for (int i = 0; i < 10000; ++i) {
      int16_t value = RandomGenerator::interval(lower, upper);
      ASSERT_GE(value, lower);
      ASSERT_LE(value, upper);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_ranges_int32_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr std::pair<int32_t, int32_t> bounds[] = {
      {0, 0},
      {0, 1},
      {0, 2},
      {0, 1023},
      {0, 1024},
      {0, 65534},
      {0, 65535},
      {0, 2147483647},
      {1, 1},
      {1, 2},
      {1, 1023},
      {1, 1024},
      {1, 65534},
      {1, 65535},
      {1, 2147483647},
      {10, 10},
      {10, 11},
      {10, 65534},
      {10, 65535},
      {10, 2147483647},
      {1024, 65534},
      {1024, 65535},
      {1024, 2147483647},
      {65530, 65534},
      {65530, 65535},
      {65530, 2147483647},
      {2147483640, 2147483640},
      {2147483640, 2147483641},
      {2147483640, 2147483642},
      {2147483640, 2147483646},
      {2147483640, 2147483647},
      {2147483646, 2147483646},
      {2147483646, 2147483647},
      {2147483647, 2147483647},
  };
  for (auto [lower, upper] : bounds) {
    for (int i = 0; i < 10000; ++i) {
      int32_t value = RandomGenerator::interval(lower, upper);
      ASSERT_GE(value, lower);
      ASSERT_LE(value, upper);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_ranges_int64_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr std::pair<int64_t, int64_t> bounds[] = {
      {0, 0},
      {0, 1},
      {0, 2},
      {0, 1023},
      {0, 1024},
      {0, 65534},
      {0, 65535},
      {0, 2147483647},
      {0, 9223372036854775807LL},
      {1, 1},
      {1, 2},
      {1, 1023},
      {1, 1024},
      {1, 65534},
      {1, 65535},
      {1, 2147483647},
      {1, 9223372036854775807LL},
      {10, 10},
      {10, 11},
      {10, 65534},
      {10, 65535},
      {10, 9223372036854775807LL},
      {2147483640, 2147483640},
      {2147483640, 2147483641},
      {2147483640, 2147483642},
      {2147483640, 2147483646},
      {2147483640, 2147483647},
      {2147483640, 9223372036854775807LL},
      {9223372036854775800LL, 9223372036854775800LL},
      {9223372036854775800LL, 9223372036854775806LL},
      {9223372036854775800LL, 9223372036854775807LL},
      {9223372036854775806LL, 9223372036854775806LL},
      {9223372036854775806LL, 9223372036854775807LL},
      {9223372036854775807LL, 9223372036854775807LL},
  };
  for (auto [lower, upper] : bounds) {
    for (int i = 0; i < 10000; ++i) {
      int64_t value = RandomGenerator::interval(lower, upper);
      ASSERT_GE(value, lower);
      ASSERT_LE(value, upper);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorTest_random_int32_brute) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  constexpr std::pair<int32_t, int32_t> bounds[] = {
      {0, 0},
      {0, 1},
      {0, 2},
      {0, 1023},
      {0, 1024},
      {0, 65534},
      {0, 65535},
      {0, 2147483646},
      {0, 2147483647},
      {1, 1},
      {1, 2},
      {1, 1023},
      {1, 1024},
      {1, 65534},
      {1, 65535},
      {1, 2147483646},
      {1, 2147483647},
      {10, 10},
      {10, 11},
      {10, 65534},
      {10, 65535},
      {10, 2147483647},
      {1024, 65534},
      {1024, 65535},
      {1024, 2147483647},
      {65530, 65534},
      {65530, 65535},
      {65530, 2000000000},
      {65530, 2147483600},
      {65535, 2147483600},
      {65536, 2147483600},
      {2000000000, 2147483640},
      {2000000000, 2147483641},
      {2000000000, 2147483642},
      {2000000000, 2147483646},
      {2000000000, 2147483647},
      {2147483640, 2147483640},
      {2147483640, 2147483641},
      {2147483640, 2147483642},
      {2147483640, 2147483646},
      {2147483640, 2147483647},
      {2147483646, 2147483646},
      {2147483646, 2147483647},
      {2147483647, 2147483647},
  };
  for (auto [lower, upper] : bounds) {
    for (int i = 0; i < 10000; ++i) {
      int32_t value = RandomGenerator::random(lower, upper);
      ASSERT_GE(value, lower);
      ASSERT_LE(value, upper);
    }
  }
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_ranges_int64) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  int64_t value;
  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(0));
  EXPECT_EQ(-5115110072778412328, value);

  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(-20));
  EXPECT_EQ(-4189371921854575647, value);

  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(19));
  EXPECT_EQ(-6664312351376758686, value);

  value = RandomGenerator::interval(int64_t(0), int64_t(INT64_MAX));
  EXPECT_EQ(7199328709248114754, value);

  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(INT64_MAX));
  EXPECT_EQ(7812659160807914514, value);

  value = RandomGenerator::interval(int64_t(0), int64_t(0));
  EXPECT_EQ(0, value);

  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(INT64_MIN));
  EXPECT_EQ(INT64_MIN, value);

  value = RandomGenerator::interval(int64_t(INT64_MAX), int64_t(INT64_MAX));
  EXPECT_EQ(INT64_MAX, value);

  value = RandomGenerator::interval(int64_t(5), int64_t(15));
  EXPECT_EQ(12, value);

  value = RandomGenerator::interval(int64_t(5), int64_t(15));
  EXPECT_EQ(6, value);

  value = RandomGenerator::interval(int64_t(5), int64_t(15));
  EXPECT_EQ(13, value);

  value = RandomGenerator::interval(int64_t(INT64_MIN), int64_t(-49874753588));
  EXPECT_EQ(-6563046468452533532, value);
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_ranges_int32) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  int32_t value;
  value = RandomGenerator::interval(int32_t(INT32_MIN), int32_t(0));
  EXPECT_EQ(-1190954371, value);

  value = RandomGenerator::interval(int32_t(0), int32_t(INT32_MAX));
  EXPECT_EQ(1694838488, value);

  value = RandomGenerator::interval(int32_t(INT32_MIN), int32_t(INT32_MAX));
  EXPECT_EQ(-975414162, value);

  value = RandomGenerator::interval(int32_t(0), int32_t(0));
  EXPECT_EQ(0, value);

  value = RandomGenerator::interval(int32_t(INT32_MIN), int32_t(INT32_MIN));
  EXPECT_EQ(INT32_MIN, value);

  value = RandomGenerator::interval(int32_t(INT32_MAX), int32_t(INT32_MAX));
  EXPECT_EQ(INT32_MAX, value);

  value = RandomGenerator::interval(int32_t(5), int32_t(15));
  EXPECT_EQ(9, value);

  value = RandomGenerator::interval(int32_t(5), int32_t(15));
  EXPECT_EQ(6, value);

  value = RandomGenerator::interval(int32_t(5), int32_t(15));
  EXPECT_EQ(11, value);

  value = RandomGenerator::interval(int32_t(INT32_MIN), int32_t(-485774));
  EXPECT_EQ(-1208965022, value);
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_ranges_int16) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  int16_t value;
  value = RandomGenerator::interval(int16_t(INT16_MIN), int16_t(0));
  EXPECT_EQ(-30601, value);

  value = RandomGenerator::interval(int16_t(0), int16_t(INT16_MAX));
  EXPECT_EQ(11992, value);

  value = RandomGenerator::interval(int16_t(INT16_MIN), int16_t(INT16_MAX));
  EXPECT_EQ(-9106, value);

  value = RandomGenerator::interval(int16_t(0), int16_t(0));
  EXPECT_EQ(0, value);

  value = RandomGenerator::interval(int16_t(INT16_MIN), int16_t(INT16_MIN));
  EXPECT_EQ(INT16_MIN, value);

  value = RandomGenerator::interval(int16_t(INT16_MAX), int16_t(INT16_MAX));
  EXPECT_EQ(INT16_MAX, value);

  value = RandomGenerator::interval(int16_t(5), int16_t(15));
  EXPECT_EQ(9, value);

  value = RandomGenerator::interval(int16_t(5), int16_t(15));
  EXPECT_EQ(6, value);

  value = RandomGenerator::interval(int16_t(5), int16_t(15));
  EXPECT_EQ(11, value);

  value = RandomGenerator::interval(int16_t(INT16_MIN), int16_t(-4854));
  EXPECT_EQ(-16442, value);
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_interval_uint64) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  uint64_t value;
  value = RandomGenerator::interval(uint64_t(0));
  EXPECT_EQ(0U, value);

  value = RandomGenerator::interval(uint64_t(UINT64_MAX));
  EXPECT_EQ(13331634000931139288U, value);

  value = RandomGenerator::interval(uint64_t(5));
  EXPECT_EQ(4U, value);

  value = RandomGenerator::interval(uint64_t(5));
  EXPECT_EQ(1U, value);

  value = RandomGenerator::interval(uint64_t(5));
  EXPECT_EQ(0U, value);
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_interval_uint32) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  uint32_t value;
  value = RandomGenerator::interval(uint32_t(0));
  EXPECT_EQ(0U, value);

  value = RandomGenerator::interval(uint32_t(UINT32_MAX));
  EXPECT_EQ(3104012925U, value);

  value = RandomGenerator::interval(uint32_t(5));
  EXPECT_EQ(4U, value);

  value = RandomGenerator::interval(uint32_t(5));
  EXPECT_EQ(4U, value);

  value = RandomGenerator::interval(uint32_t(5));
  EXPECT_EQ(1U, value);
}

TEST(RandomGeneratorTest, test_RandomGeneratorMersenne_interval_uint16) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  RandomGenerator::ensureDeviceIsInitialized();
  RandomGenerator::seed(0xdeadbeef);

  uint16_t value;
  value = RandomGenerator::interval(uint16_t(0));
  EXPECT_EQ(0U, value);

  value = RandomGenerator::interval(uint16_t(UINT16_MAX));
  EXPECT_EQ(31357U, value);

  value = RandomGenerator::interval(uint16_t(5));
  EXPECT_EQ(4U, value);

  value = RandomGenerator::interval(uint16_t(5));
  EXPECT_EQ(4U, value);

  value = RandomGenerator::interval(uint16_t(5));
  EXPECT_EQ(1U, value);
}
