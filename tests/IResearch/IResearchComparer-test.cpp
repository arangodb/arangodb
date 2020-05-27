////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "common.h"
#include "gtest/gtest.h"

#include "Basics/VelocyPackHelper.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewSort.h"

#include <velocypack/Iterator.h>
#include <utils/utf8_path.hpp>

TEST(IResearchComparerTest, test_comparer_single_entry) {
  arangodb::tests::init(true);

  irs::utf8_path resource;
  resource /= irs::string_ref(arangodb::tests::testResourceDir);
  resource /= irs::string_ref("simple_sequential.json");

  auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
  auto docsSlice = builder.slice();
  ASSERT_TRUE(docsSlice.isArray());
  ASSERT_NE(0, docsSlice.length());

  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({{"name", false}}, false);  // name DESC

  std::vector<irs::bytes_ref> expected_values;
  expected_values.reserve(docsSlice.length());
  std::vector<irs::bytes_ref> actual_values;
  actual_values.reserve(docsSlice.length());

  for (auto doc : arangodb::velocypack::ArrayIterator(docsSlice)) {
    auto slice = doc.get("name");
    EXPECT_TRUE(slice.isString());
    actual_values.emplace_back(irs::bytes_ref(slice.start(), slice.byteSize()));
    expected_values.emplace_back(irs::bytes_ref(slice.start(), slice.byteSize()));
  }

  // sorted expected docs
  auto expectedComparer = [](irs::bytes_ref const& lhs, irs::bytes_ref& rhs) {
    return arangodb::basics::VelocyPackHelper::compare(VPackSlice(lhs.c_str()),
                                                       VPackSlice(rhs.c_str()), true) > 0;
  };
  EXPECT_FALSE(std::is_sorted(expected_values.begin(), expected_values.end(), expectedComparer));
  std::sort(expected_values.begin(), expected_values.end(), expectedComparer);

  // sort actual docs
  arangodb::iresearch::VPackComparer comparer;
  EXPECT_TRUE(comparer.empty());
  comparer.reset(sort);
  EXPECT_FALSE(comparer.empty());
  EXPECT_FALSE(std::is_sorted(actual_values.begin(), actual_values.end(), comparer));
  std::sort(actual_values.begin(), actual_values.end(), comparer);
  EXPECT_EQ(expected_values, actual_values);
}

TEST(IResearchComparerTest, test_comparer_multiple_entries) {
  arangodb::tests::init(true);

  irs::utf8_path resource;
  resource /= irs::string_ref(arangodb::tests::testResourceDir);
  resource /= irs::string_ref("simple_sequential.json");

  auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
  auto docsSlice = builder.slice();
  ASSERT_TRUE(docsSlice.isArray());
  ASSERT_NE(0, docsSlice.length());

  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({{"same", false}}, true);  // same ASC
  sort.emplace_back({{"seq", false}}, false);  // seq DESC
  sort.emplace_back({{"name", false}}, true);  // name ASC

  std::vector<irs::bstring> expected_values;
  expected_values.reserve(docsSlice.length());
  std::vector<irs::bstring> actual_values;
  actual_values.reserve(docsSlice.length());

  for (auto doc : arangodb::velocypack::ArrayIterator(docsSlice)) {
    irs::bstring value;
    for (size_t i = 0, size = sort.size(); i < size; ++i) {
      EXPECT_EQ(1, sort.field(i).size());
      auto slice = doc.get(sort.field(i)[0].name);

      value.append(slice.start(), slice.byteSize());
    }
    actual_values.emplace_back(value);
    expected_values.emplace_back(value);
  }

  // sorted expected docs
  auto expectedComparer = [](irs::bytes_ref const& lhs, irs::bytes_ref const& rhs) {
    VPackSlice const lhsSlice(lhs.c_str());
    VPackSlice const rhsSlice(rhs.c_str());

    // 2nd bucket defines the order
    return arangodb::basics::VelocyPackHelper::compare(
               VPackSlice(lhsSlice.start() + lhsSlice.byteSize()),
               VPackSlice(rhsSlice.start() + rhsSlice.byteSize()), true) > 0;
  };
  EXPECT_FALSE(std::is_sorted(expected_values.begin(), expected_values.end(), expectedComparer));
  std::sort(expected_values.begin(), expected_values.end(), expectedComparer);

  // sort actual docs
  arangodb::iresearch::VPackComparer comparer;
  EXPECT_TRUE(comparer.empty());
  comparer.reset(sort);
  EXPECT_FALSE(comparer.empty());
  EXPECT_FALSE(std::is_sorted(actual_values.begin(), actual_values.end(), comparer));
  std::sort(actual_values.begin(), actual_values.end(), comparer);
  EXPECT_EQ(expected_values, actual_values);
}
