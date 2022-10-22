////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "DisjointSet.h"

TEST(GWEN_DISJOINT_SET, test_constructor) {
  auto ds = DisjointSet(10);
  EXPECT_EQ(ds.capacity(), static_cast<size_t>(10));
  auto ds0 = DisjointSet();
  EXPECT_EQ(ds0.capacity(), static_cast<size_t>(0));
}

TEST(GWEN_DISJOINT_SET, test_add_singleton) {
  auto ds = DisjointSet();
  EXPECT_TRUE(ds.addSingleton(2));
  EXPECT_EQ(ds.capacity(), 3u);
  EXPECT_FALSE(ds.addSingleton(2));
  EXPECT_TRUE(ds.addSingleton(1));
  EXPECT_EQ(ds.capacity(), 3u);
  EXPECT_TRUE(ds.addSingleton(4));
  EXPECT_EQ(ds.capacity(), 5u);
  EXPECT_TRUE(ds.addSingleton(0));
  EXPECT_EQ(ds.capacity(), 5u);
  EXPECT_TRUE(ds.addSingleton(5, 6));
  EXPECT_EQ(ds.capacity(), 6u);
  EXPECT_TRUE(ds.addSingleton(6, 8));
  EXPECT_EQ(ds.capacity(), 8u);
  EXPECT_TRUE(ds.addSingleton(3, 5));
  EXPECT_EQ(ds.capacity(), 8u);
  EXPECT_TRUE(ds.addSingleton(7, 5));
  EXPECT_EQ(ds.capacity(), 8u);
}

TEST(GWEN_DISJOINT_SET, test_merge_and_representatives) {
  // todo: test return value of merge
  auto ds = DisjointSet(10);
  for (size_t i = 0; i < 10; ++i) {
    ds.addSingleton(i);
  }
  ds.merge(0, 0);
  EXPECT_EQ(ds.representative(0), 0u);
  ds.merge(0, 1);
  // 0 and 1 have the same rank, so the second parameter
  // becomes the representative
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  ds.merge(0, 1);
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  ds.merge(1, 2);
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  // 1 has a higher rank
  EXPECT_EQ(ds.representative(2), 1u);
  ds.merge(3, 4);
  ds.merge(5, 6);
  ds.merge(3, 5);
  ds.merge(2, 3);
  // the representative of 3 has a higher rank than that of 2
  EXPECT_EQ(ds.representative(2), 6u);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
