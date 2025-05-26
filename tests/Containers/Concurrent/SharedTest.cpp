////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "Containers/Concurrent/shared.h"
#include "Inspection/Format.h"

#include <gtest/gtest.h>
#include <variant>

using namespace arangodb::containers;

TEST(SharedTest, shared_reference_extends_lifetime) {
  SharedPtr<int> ref_copy;
  EXPECT_EQ(ref_copy.ref_count(), 1);
  {
    auto initial_ref = SharedPtr<int>{435};
    EXPECT_EQ(initial_ref.ref_count(), 1);
    EXPECT_EQ(ref_copy.ref_count(), 1);

    ref_copy = initial_ref;
    EXPECT_EQ(ref_copy.ref_count(), 2);
    EXPECT_EQ(initial_ref.ref_count(), ref_copy.ref_count());
  }
  EXPECT_EQ(ref_copy.ref_count(), 1);
  EXPECT_EQ(*ref_copy.get(), 435);
}

TEST(SharedTest, inspection_of_shared_reference_gives_shared_object) {
  auto ref = SharedPtr<int>{4};
  EXPECT_EQ(fmt::format("{}", arangodb::inspection::json(ref)), "4");
}

struct MyStruct {
  MyStruct(std::string x) : a{std::move(x)} {};
  bool operator==(MyStruct const&) const = default;
  std::string a;
};
template<typename Inspector>
auto inspect(Inspector& f, MyStruct& x) {
  return f.object(x).fields(f.field("a", x.a));
}
TEST(SharedTest, variant_ptr_can_include_a_copy_of_a_shared_reference) {
  {
    auto ref = SharedPtr<int>{435};
    EXPECT_EQ(*ref.get(), 435);
    EXPECT_EQ(ref.ref_count(), 1);
    {
      auto variant = AtomicSharedOrRawPtr<int, MyStruct>{ref};
      EXPECT_EQ(ref.ref_count(), 2);
      EXPECT_TRUE(std::holds_alternative<int*>(variant.get()));
      EXPECT_EQ(*std::get<int*>(variant.get()), 435);
    }
    EXPECT_EQ(ref.ref_count(), 1);
    EXPECT_EQ(*ref.get(), 435);
  }
  {
    auto ref = SharedPtr<MyStruct>{"abcde"};
    EXPECT_EQ(*ref.get(), MyStruct{"abcde"});
    EXPECT_EQ(ref.ref_count(), 1);
    {
      auto variant = AtomicSharedOrRawPtr<MyStruct, int>{ref};
      EXPECT_EQ(ref.ref_count(), 2);
      EXPECT_TRUE(std::holds_alternative<MyStruct*>(variant.get()));
      EXPECT_EQ(*std::get<MyStruct*>(variant.get()), MyStruct{"abcde"});
    }
    EXPECT_EQ(ref.ref_count(), 1);
    EXPECT_EQ(*ref.get(), MyStruct{"abcde"});
  }
}

TEST(SharedTest, variant_ptr_can_include_a_raw_pointer) {
  auto ptr = new MyStruct{"abcde"};
  auto variant = AtomicSharedOrRawPtr<int, MyStruct>{ptr};
  EXPECT_TRUE(std::holds_alternative<MyStruct*>(variant.get()));
  EXPECT_EQ(*std::get<MyStruct*>(variant.get()), MyStruct{"abcde"});
}

TEST(SharedTest, inspection_of_variant) {
  {
    auto ptr = new MyStruct{"abcde"};
    auto variant = AtomicSharedOrRawPtr<int, MyStruct>{ptr};
    EXPECT_EQ(fmt::format("{}", arangodb::inspection::json(variant)),
              fmt::format("{}", arangodb::inspection::json(*ptr)));
  }
  {
    auto ref = SharedPtr<MyStruct>{"abcde"};
    auto variant = AtomicSharedOrRawPtr<MyStruct, int>{ref};
    EXPECT_EQ(fmt::format("{}", arangodb::inspection::json(variant)),
              fmt::format("{}", arangodb::inspection::json(ref)));
  }
}
