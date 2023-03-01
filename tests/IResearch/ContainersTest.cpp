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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <set>
#include <tuple>
#include "gtest/gtest.h"
#include "utils/thread_utils.hpp"

#include "IResearch/Containers.h"

TEST(AsyncValue, empty) {
  {
    arangodb::iresearch::AsyncValue<char> asyncValue{nullptr};
    EXPECT_TRUE(asyncValue.empty());
    asyncValue.reset();
    EXPECT_TRUE(asyncValue.empty());
  }
  {
    char c = 'a';
    arangodb::iresearch::AsyncValue<char> asyncValue{&c};
    EXPECT_FALSE(asyncValue.empty());
    asyncValue.reset();
    EXPECT_TRUE(asyncValue.empty());
  }
}

TEST(AsyncValue, lock) {
  {
    arangodb::iresearch::AsyncValue<char> asyncValue{nullptr};
    EXPECT_FALSE(asyncValue.lock());
    asyncValue.reset();
    EXPECT_FALSE(asyncValue.lock());
  }
  {
    char c = 'a';
    arangodb::iresearch::AsyncValue<char> asyncValue{&c};
    auto value = asyncValue.lock();
    EXPECT_TRUE(value);
    EXPECT_EQ(*value.get(), 'a');
  }
}

TEST(AsyncValue, multithread) {
  char c = 'a';
  arangodb::iresearch::AsyncValue<char> asyncValue{&c};
  std::atomic_size_t count = 0;
  std::thread lock1{[&] {
    EXPECT_FALSE(asyncValue.empty());
    auto value = asyncValue.lock();
    count.fetch_add(1);
    EXPECT_TRUE(value);
    EXPECT_EQ(*value.get(), 'a');
  }};
  std::thread lock2{[&] {
    EXPECT_FALSE(asyncValue.empty());
    auto value = asyncValue.lock();
    count.fetch_add(1);
    EXPECT_TRUE(value);
    EXPECT_EQ(*value.get(), 'a');
  }};
  std::thread reset1{[&] {
    while (count.load() != 2) {
      std::this_thread::yield();
    }
    asyncValue.reset();
  }};
  std::thread reset2{[&] {
    while (count.load() != 2) {
      std::this_thread::yield();
    }
    asyncValue.reset();
    auto value = asyncValue.lock();
    EXPECT_FALSE(value);
  }};
  std::thread lockAfterReset{[&] {
    arangodb::iresearch::AsyncValue<char>::Value value;
    do {
      value = asyncValue.lock();
    } while (value);
    EXPECT_TRUE(asyncValue.empty());
  }};
  lockAfterReset.join();
  reset2.join();
  reset1.join();
  lock2.join();
  lock1.join();
}

TEST(ContainersTest, test_UniqueHeapInstance) {
  {
    struct TestStruct {};

    // ensure copy works (different instance)
    {
      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance;
      auto* ptr = instance.get();

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance1;
      auto* ptr1 = instance1.get();
      EXPECT_FALSE((ptr == instance1.get()));
      instance1 = instance;
      EXPECT_FALSE((ptr1 == instance1.get()));
      EXPECT_FALSE((ptr == instance1.get()));

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance2(instance);
      EXPECT_FALSE((ptr == instance2.get()));
    }

    // ensure element copy works (different instance)
    {
      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance;
      auto* ptr = instance.get();

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance1;
      auto* ptr1 = instance1.get();
      EXPECT_FALSE((ptr == instance1.get()));
      instance1 = *instance;
      EXPECT_TRUE((ptr1 == instance1.get()));
      EXPECT_FALSE((ptr == instance1.get()));

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance2(*instance);
      EXPECT_FALSE((ptr == instance2.get()));
    }
  }

  {
    static size_t counter = 0;
    struct TestStruct {
      size_t id;
      TestStruct() : id(++counter) {}
      TestStruct(TestStruct&& other) : id(other.id){};
      TestStruct(TestStruct const&) = delete;
      TestStruct& operator=(TestStruct const&) = delete;
      TestStruct& operator=(TestStruct&& other) {
        id = other.id;
        return *this;
      }
    };

    // ensure move works (same instance)
    {
      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance;
      auto* ptr = instance.get();

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance1;
      EXPECT_FALSE((ptr == instance1.get()));
      instance1 = std::move(instance);
      EXPECT_TRUE((ptr == instance1.get()));

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance2(
          std::move(instance1));
      EXPECT_TRUE((ptr == instance2.get()));
    }

    // ensure value move works (same instance)
    {
      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance;
      auto* ptr = instance.get();
      auto id = ptr->id;

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance1;
      EXPECT_FALSE((ptr == instance1.get()));
      EXPECT_FALSE((id == instance1->id));
      instance1 = std::move(*instance);
      EXPECT_TRUE((id == instance1->id));

      arangodb::iresearch::UniqueHeapInstance<TestStruct> instance2(
          std::move(*instance1));
      EXPECT_TRUE((id == instance2->id));
    }
  }
}
