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
#include "Containers/SmallVector.h"

#include "gtest/gtest.h"
#include <cstdint>

TEST(SmallVectorTest, test_empty) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  EXPECT_EQ(values.size(), 0);
  EXPECT_EQ(values.capacity(), 32 / sizeof(uint64_t));
  EXPECT_TRUE(values.empty());
  
  values.clear();

  EXPECT_EQ(values.size(), 0);
  EXPECT_TRUE(values.empty());
}

TEST(SmallVectorTest, test_in_arena) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  EXPECT_EQ(values.capacity(), 4);

  // all values must be stored in the arena
  uint64_t const* arena = reinterpret_cast<uint64_t const*>(&values);
  for (size_t i = 0; i < 4; ++i) {
    values.push_back(i);
    EXPECT_EQ(&values[i], &arena[i]);
  }

  // this will overflow the arena
  values.push_back(1);
  EXPECT_EQ(values.capacity(), 8);
  
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NE(&values[i], &arena[i]);
  }
}

TEST(SmallVectorTest, test_data) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  EXPECT_EQ(values.capacity(), 4);

  // all values must be stored in the arena
  for (size_t i = 0; i < 4; ++i) {
    values.push_back(i);
  }

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(i, values.data()[i]);
  }

  // this will overflow the arena
  values.push_back(4);
  EXPECT_EQ(values.capacity(), 8);
  
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(i, values.data()[i]);
  }
}


TEST(SmallVectorTest, test_capacity) {
  {
    arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;
  
    EXPECT_EQ(values.size(), 0);
    EXPECT_EQ(values.capacity(), 32 / sizeof(uint64_t));
    EXPECT_TRUE(values.empty());

    for (size_t i = 0; i < 4; ++i) {
      values.push_back(i);
      EXPECT_EQ(values.size(), i + 1);
      EXPECT_EQ(values.capacity(), 4);
    }

    values.push_back(666);
    EXPECT_EQ(values.size(), 5);
    EXPECT_EQ(values.capacity(), 8);
  }
  
  {
    arangodb::containers::SmallVectorWithArena<uint32_t, 64> values;
  
    EXPECT_EQ(values.size(), 0);
    EXPECT_EQ(values.capacity(), 64 / sizeof(uint32_t));
    EXPECT_TRUE(values.empty());
    
    for (size_t i = 0; i < 16; ++i) {
      values.push_back(static_cast<uint32_t>(i));
      EXPECT_EQ(values.size(), i + 1);
      EXPECT_EQ(values.capacity(), 16);
    }

    values.push_back(666);
    EXPECT_EQ(values.size(), 17);
    EXPECT_EQ(values.capacity(), 32);
  }
}

TEST(SmallVectorTest, test_fillup) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;
  
  values.push_back(0);
  EXPECT_EQ(values.size(), 1);
  EXPECT_FALSE(values.empty());

  values.push_back(1);
  EXPECT_EQ(values.size(), 2);
  EXPECT_FALSE(values.empty());
  
  values.push_back(2);
  EXPECT_EQ(values.size(), 3);
  EXPECT_FALSE(values.empty());
  
  values.push_back(3);
  EXPECT_EQ(values.size(), 4);
  EXPECT_FALSE(values.empty());
  
  // heap allocation
  values.push_back(4);
  EXPECT_EQ(values.size(), 5);
  EXPECT_FALSE(values.empty());
  
  for (uint64_t i = 0; i < 5; ++i) {
    EXPECT_EQ(i, values[i]);
    EXPECT_EQ(i, values.at(i));
  }
}

TEST(SmallVectorTest, test_fillmore) {
  arangodb::containers::SmallVectorWithArena<uint32_t, 32> values;

  for (uint32_t i = 0; i < 1000; ++i) {
    EXPECT_EQ(values.size(), i);
    values.push_back(i);
    EXPECT_EQ(values.size(), i + 1);
  }
  EXPECT_EQ(values.size(), 1000);
  
  for (uint32_t i = 0; i < 1000; ++i) {
    EXPECT_EQ(i, values[i]);
    EXPECT_EQ(i, values.at(i));
  }

  values.clear();
  EXPECT_EQ(values.size(), 0);
  EXPECT_TRUE(values.empty());
}

TEST(SmallVectorTest, test_refill) {
  arangodb::containers::SmallVectorWithArena<uint32_t, 32> values;

  for (uint32_t i = 0; i < 1000; ++i) {
    EXPECT_EQ(values.size(), i);
    values.push_back(i);
    EXPECT_EQ(values.size(), i + 1);
  }
  values.clear();
  for (uint32_t i = 0; i < 512; ++i) {
    values.emplace_back(i * 2);
  }
  EXPECT_EQ(values.size(), 512);
  for (uint32_t i = 0; i < 512; ++i) {
    EXPECT_EQ(i * 2, values[i]);
    EXPECT_EQ(i * 2, values.at(i));
  }
}

TEST(SmallVectorTest, test_resize) {
  arangodb::containers::SmallVectorWithArena<uint32_t, 32> values;

  values.resize(10000);
  EXPECT_EQ(values.size(), 10000);
  for (uint32_t i = 0; i < 10000; ++i) {
    EXPECT_EQ(0, values[i]);
    EXPECT_EQ(0, values.at(i));
  }

  values.emplace_back(1);
  EXPECT_EQ(values.size(), 10001);
  EXPECT_EQ(1, values[10000]);
  EXPECT_EQ(1, values.at(10000));

  values.resize(3);
  EXPECT_EQ(values.size(), 3);
  for (uint32_t i = 0; i < 3; ++i) {
    EXPECT_EQ(0, values[i]);
    EXPECT_EQ(0, values.at(i));
  }

  values.resize(0);
  EXPECT_EQ(values.size(), 0);
}

TEST(SmallVectorTest, test_at) {
  arangodb::containers::SmallVectorWithArena<uint32_t, 32> values;

  EXPECT_THROW(values.at(0), std::out_of_range);
  EXPECT_THROW(values.at(1), std::out_of_range);
  EXPECT_THROW(values.at(12345), std::out_of_range);

  values.resize(10);
  EXPECT_EQ(0, values.at(9));
  EXPECT_THROW(values.at(10), std::out_of_range);
}

TEST(SmallVectorTest, test_front) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  values.push_back(666);
  EXPECT_EQ(666, values.front());
  
  values.push_back(999);
  EXPECT_EQ(666, values.front());

  for (uint64_t i = 0; i < 10; ++i) {
    values.push_back(i);
    EXPECT_EQ(666, values.front());
  }
}

TEST(SmallVectorTest, test_back) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  values.push_back(666);
  EXPECT_EQ(666, values.back());
  
  values.push_back(999);
  EXPECT_EQ(999, values.back());

  for (uint64_t i = 0; i < 10; ++i) {
    values.push_back(i);
    EXPECT_EQ(i, values.back());
  }
}

TEST(SmallVectorTest, test_iterator) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  EXPECT_EQ(values.begin(), values.end());

  for (uint64_t i = 0; i < 100; ++i) {
    values.push_back(i * 3);
    EXPECT_NE(values.begin(), values.end());
  }

  uint64_t i = 0;
  auto it = values.begin();
  while (it != values.end()) {
    EXPECT_EQ(i * 3, *it);
    ++it;
    ++i;
  }
}

TEST(SmallVectorTest, test_reverse_iterator) {
  arangodb::containers::SmallVectorWithArena<uint64_t, 32> values;

  EXPECT_EQ(values.begin(), values.end());

  for (uint64_t i = 0; i < 100; ++i) {
    values.push_back(i * 3);
    EXPECT_NE(values.begin(), values.end());
  }

  uint64_t i = 0;
  auto it = values.rbegin();
  while (it != values.rend()) {
    EXPECT_EQ((100 - i - 1) * 3, *it);
    ++it;
    ++i;
  }
}
