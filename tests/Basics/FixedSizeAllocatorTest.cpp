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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/FixedSizeAllocator.h"
#include "Aql/AstNode.h"

using namespace arangodb;

TEST(FixedSizeAllocatorTest, test_Int) {
  FixedSizeAllocator<int> allocator;

  EXPECT_EQ(0, allocator.numUsed());

  int* p = allocator.allocate(24);

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(24, *p);
  EXPECT_EQ(1, allocator.numUsed());
  
  p = allocator.allocate(42);

  EXPECT_EQ(42, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(2, allocator.numUsed());

  p = allocator.allocate(23);

  EXPECT_EQ(23, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(3, allocator.numUsed());

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
}

TEST(FixedSizeAllocatorTest, test_UInt64) {
  FixedSizeAllocator<uint64_t> allocator;

  EXPECT_EQ(0, allocator.numUsed());

  uint64_t* p = allocator.allocate(24);

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(uint64_t)));
  EXPECT_EQ(24, *p);
  EXPECT_EQ(1, allocator.numUsed());
  
  p = allocator.allocate(42);

  EXPECT_EQ(42, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(2, allocator.numUsed());

  p = allocator.allocate(23);

  EXPECT_EQ(23, *p);
  EXPECT_EQ(0, (uintptr_t(p) % alignof(int)));
  EXPECT_EQ(3, allocator.numUsed());

  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
}

TEST(FixedSizeAllocatorTest, test_Struct) {
  struct Testee {
    Testee(std::string abc, std::string def)
      : abc(abc), def(def) {}

    std::string abc;
    std::string def;
  };

  FixedSizeAllocator<Testee> allocator;

  EXPECT_EQ(0, allocator.numUsed());

  Testee* p = allocator.allocate("foo", "bar");

  // first allocation should be aligned to a cache line
  EXPECT_EQ(0, (uintptr_t(p) % 64));
  EXPECT_EQ(0, (uintptr_t(p) % alignof(Testee)));
  EXPECT_EQ("foo", p->abc);
  EXPECT_EQ("bar", p->def);
  EXPECT_EQ(1, allocator.numUsed());
  
  p = allocator.allocate("foobar", "baz");
  EXPECT_EQ(0, (uintptr_t(p) % alignof(Testee)));
  EXPECT_EQ("foobar", p->abc);
  EXPECT_EQ("baz", p->def);
  EXPECT_EQ(2, allocator.numUsed());
  
  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
}

TEST(FixedSizeAllocatorTest, test_MassAllocation) {
  FixedSizeAllocator<std::string> allocator;

  EXPECT_EQ(0, allocator.numUsed());

  for (size_t i = 0; i < 10 * 1000; ++i) {
    std::string* p = allocator.allocate("test" + std::to_string(i));

    EXPECT_EQ("test" + std::to_string(i), *p);
    EXPECT_EQ(i + 1, allocator.numUsed());
  }
  
  allocator.clear();

  EXPECT_EQ(0, allocator.numUsed());
}
