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
  SharedReference<int> ref_copy;
  EXPECT_EQ(ref_copy.ref_count(), 1);
  {
    auto initial_ref = SharedReference<int>{435};
    EXPECT_EQ(initial_ref.ref_count(), 1);
    EXPECT_EQ(ref_copy.ref_count(), 1);

    ref_copy = initial_ref;
    EXPECT_EQ(ref_copy.ref_count(), 2);
    EXPECT_EQ(initial_ref.ref_count(), ref_copy.ref_count());
  }
  EXPECT_EQ(ref_copy.ref_count(), 1);
  EXPECT_EQ(ref_copy.get_ref(), 435);
}

TEST(SharedTest, inspection_of_shared_reference_gives_shared_object) {
  auto ref = SharedReference<int>{4};
  EXPECT_EQ(fmt::format("{}", arangodb::inspection::json(ref)), "4");
}

struct Bla {
  Bla(){};
  std::string a;
};
TEST(SharedTest, variant_ptr_works_like_a_variant) {
  auto first_type = VariantPtr<int, Bla>::first(18);
  EXPECT_TRUE(first_type.get_ref().has_value());
  EXPECT_TRUE(std::holds_alternative<std::reference_wrapper<int>>(
      first_type.get_ref().value()));
  EXPECT_EQ(std::get<std::reference_wrapper<int>>(first_type.get_ref().value()),
            18);

  auto second_type = VariantPtr<Shared<Bla>, int>::second(22);
  EXPECT_TRUE(second_type.get_ref().has_value());
  EXPECT_TRUE(std::holds_alternative<std::reference_wrapper<int>>(
      second_type.get_ref().value()));
  EXPECT_EQ(
      std::get<std::reference_wrapper<int>>(second_type.get_ref().value()), 22);
}
