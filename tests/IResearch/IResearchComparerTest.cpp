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

#include "common.h"
#include "gtest/gtest.h"

#include "Basics/VelocyPackHelper.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewSort.h"

#include <velocypack/Iterator.h>
#include <filesystem>

TEST(IResearchComparerTest, test_comparer_single_entry) {
  arangodb::tests::init(true);

  std::filesystem::path resource;
  resource /= std::string_view(arangodb::tests::testResourceDir);
  resource /= std::string_view("simple_sequential.json");

  auto builder =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.string());
  auto docsSlice = builder.slice();
  ASSERT_TRUE(docsSlice.isArray());
  ASSERT_NE(0, docsSlice.length());

  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({{std::string_view("name"), false}}, false);  // name DESC

  std::vector<irs::bytes_view> expected_values;
  expected_values.reserve(docsSlice.length());
  std::vector<irs::bytes_view> actual_values;
  actual_values.reserve(docsSlice.length());

  for (auto doc : arangodb::velocypack::ArrayIterator(docsSlice)) {
    auto slice = doc.get("name");
    EXPECT_TRUE(slice.isString());
    actual_values.emplace_back(
        irs::bytes_view(slice.start(), slice.byteSize()));
    expected_values.emplace_back(
        irs::bytes_view(slice.start(), slice.byteSize()));
  }

  // sorted expected docs
  auto expectedComparer = [](irs::bytes_view const& lhs, irs::bytes_view& rhs) {
    return arangodb::basics::VelocyPackHelper::compare(
               VPackSlice(lhs.data()), VPackSlice(rhs.data()), true) > 0;
  };
  EXPECT_FALSE(std::is_sorted(expected_values.begin(), expected_values.end(),
                              expectedComparer));
  std::sort(expected_values.begin(), expected_values.end(), expectedComparer);

  // sort actual docs
  arangodb::iresearch::VPackComparer<arangodb::iresearch::IResearchViewSort>
      comparer;
  EXPECT_TRUE(comparer.empty());
  comparer.reset(sort);
  EXPECT_FALSE(comparer.empty());
  auto cmp = [&](const auto& lhs, const auto& rhs) {
    return comparer.Compare(lhs, rhs) < 0;
  };
  EXPECT_FALSE(std::is_sorted(actual_values.begin(), actual_values.end(), cmp));
  std::sort(actual_values.begin(), actual_values.end(), cmp);
  EXPECT_EQ(expected_values, actual_values);
}

TEST(IResearchComparerTest, test_comparer_multiple_entries) {
  arangodb::tests::init(true);

  std::filesystem::path resource;
  resource /= std::string_view(arangodb::tests::testResourceDir);
  resource /= std::string_view("simple_sequential.json");

  auto builder =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.string());
  auto docsSlice = builder.slice();
  ASSERT_TRUE(docsSlice.isArray());
  ASSERT_NE(0, docsSlice.length());

  arangodb::iresearch::IResearchViewSort sort;
  sort.emplace_back({{std::string_view("same"), false}}, true);  // same ASC
  sort.emplace_back({{std::string_view("seq"), false}}, false);  // seq DESC
  sort.emplace_back({{std::string_view("name"), false}}, true);  // name ASC

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
  auto expectedComparer = [](irs::bytes_view const& lhs,
                             irs::bytes_view const& rhs) {
    VPackSlice const lhsSlice(lhs.data());
    VPackSlice const rhsSlice(rhs.data());

    // 2nd bucket defines the order
    return arangodb::basics::VelocyPackHelper::compare(
               VPackSlice(lhsSlice.start() + lhsSlice.byteSize()),
               VPackSlice(rhsSlice.start() + rhsSlice.byteSize()), true) > 0;
  };
  EXPECT_FALSE(std::is_sorted(expected_values.begin(), expected_values.end(),
                              expectedComparer));
  std::sort(expected_values.begin(), expected_values.end(), expectedComparer);

  // sort actual docs
  arangodb::iresearch::VPackComparer<arangodb::iresearch::IResearchViewSort>
      comparer;
  EXPECT_TRUE(comparer.empty());
  comparer.reset(sort);
  EXPECT_FALSE(comparer.empty());
  auto cmp = [&](const auto& lhs, const auto& rhs) {
    return comparer.Compare(lhs, rhs) < 0;
  };
  EXPECT_FALSE(std::is_sorted(actual_values.begin(), actual_values.end(), cmp));
  std::sort(actual_values.begin(), actual_values.end(), cmp);
  EXPECT_EQ(expected_values, actual_values);
}
