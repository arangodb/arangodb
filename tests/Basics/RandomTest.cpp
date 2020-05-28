////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for RandomGenerator class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Random/RandomGenerator.h"

using namespace arangodb;

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
