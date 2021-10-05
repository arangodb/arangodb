////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "tests-common.h"

#ifndef _WIN32
#include <sys/time.h>
#endif

static uint8_t LocalBuffer[128];

TEST(CommonTest, StoreUInt64Zero) {
  uint64_t const value = 0;
  storeUInt64(&LocalBuffer[0], value);

  ASSERT_EQ(0UL, LocalBuffer[0]);
  ASSERT_EQ(0UL, LocalBuffer[1]);
  ASSERT_EQ(0UL, LocalBuffer[2]);
  ASSERT_EQ(0UL, LocalBuffer[3]);
  ASSERT_EQ(0UL, LocalBuffer[4]);
  ASSERT_EQ(0UL, LocalBuffer[5]);
  ASSERT_EQ(0UL, LocalBuffer[6]);
  ASSERT_EQ(0UL, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

TEST(CommonTest, StoreUInt64One) {
  uint64_t const value = 1;
  storeUInt64(&LocalBuffer[0], value);
  ASSERT_EQ(0x01, LocalBuffer[0]);
  ASSERT_EQ(0x00, LocalBuffer[1]);
  ASSERT_EQ(0x00, LocalBuffer[2]);
  ASSERT_EQ(0x00, LocalBuffer[3]);
  ASSERT_EQ(0x00, LocalBuffer[4]);
  ASSERT_EQ(0x00, LocalBuffer[5]);
  ASSERT_EQ(0x00, LocalBuffer[6]);
  ASSERT_EQ(0x00, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

TEST(CommonTest, StoreUInt64Something) {
  uint64_t const value = 259;
  storeUInt64(&LocalBuffer[0], value);
  ASSERT_EQ(0x03, LocalBuffer[0]);
  ASSERT_EQ(0x01, LocalBuffer[1]);
  ASSERT_EQ(0x00, LocalBuffer[2]);
  ASSERT_EQ(0x00, LocalBuffer[3]);
  ASSERT_EQ(0x00, LocalBuffer[4]);
  ASSERT_EQ(0x00, LocalBuffer[5]);
  ASSERT_EQ(0x00, LocalBuffer[6]);
  ASSERT_EQ(0x00, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

TEST(CommonTest, StoreUInt64SomethingElse) {
  uint64_t const value = 0xab12760039;
  storeUInt64(&LocalBuffer[0], value);
  ASSERT_EQ(0x39, LocalBuffer[0]);
  ASSERT_EQ(0x00, LocalBuffer[1]);
  ASSERT_EQ(0x76, LocalBuffer[2]);
  ASSERT_EQ(0x12, LocalBuffer[3]);
  ASSERT_EQ(0xab, LocalBuffer[4]);
  ASSERT_EQ(0x00, LocalBuffer[5]);
  ASSERT_EQ(0x00, LocalBuffer[6]);
  ASSERT_EQ(0x00, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

TEST(CommonTest, StoreUInt64SomethingMore) {
  uint64_t const value = 0x7f9831ab12761339;

  storeUInt64(&LocalBuffer[0], value);
  ASSERT_EQ(0x39, LocalBuffer[0]);
  ASSERT_EQ(0x13, LocalBuffer[1]);
  ASSERT_EQ(0x76, LocalBuffer[2]);
  ASSERT_EQ(0x12, LocalBuffer[3]);
  ASSERT_EQ(0xab, LocalBuffer[4]);
  ASSERT_EQ(0x31, LocalBuffer[5]);
  ASSERT_EQ(0x98, LocalBuffer[6]);
  ASSERT_EQ(0x7f, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

TEST(CommonTest, StoreUInt64Max) {
  uint64_t const value = 0xffffffffffffffffULL;

  storeUInt64(&LocalBuffer[0], value);
  ASSERT_EQ(0xff, LocalBuffer[0]);
  ASSERT_EQ(0xff, LocalBuffer[1]);
  ASSERT_EQ(0xff, LocalBuffer[2]);
  ASSERT_EQ(0xff, LocalBuffer[3]);
  ASSERT_EQ(0xff, LocalBuffer[4]);
  ASSERT_EQ(0xff, LocalBuffer[5]);
  ASSERT_EQ(0xff, LocalBuffer[6]);
  ASSERT_EQ(0xff, LocalBuffer[7]);

  ASSERT_EQ(value, readUInt64(&LocalBuffer[0]));
}

#ifndef _WIN32
TEST(CommonTest, CurrentUTCValue) {
  struct timeval t;
  gettimeofday(&t, 0);

  int64_t now = (t.tv_sec * 1000) + (t.tv_usec / 1000);
  int64_t utc = currentUTCDateValue();

  ASSERT_TRUE(utc >= now);
  ASSERT_TRUE((utc - now) < 120 * 1000);
}
#endif

TEST(CommonTest, AsmFunctions) {
  disableAssemblerFunctions();
  ASSERT_TRUE(assemblerFunctionsDisabled());
  ASSERT_FALSE(assemblerFunctionsEnabled());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
