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

#include "IResearchQueryCommon.h"

namespace arangodb::tests {
namespace {

class QueryAggregate : public QueryTest {
 protected:
  void queryTests() {
    // test grouping with counting
    {
      containers::FlatHashMap<double_t, size_t> expected{
          {100., 5}, {12., 2}, {95., 1},   {90.564, 1}, {1., 1},
          {0., 1},   {50., 1}, {-32.5, 1}, {3.14, 1}  // null
      };

      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH d.value <= 100"
                                 " COLLECT value = d.value WITH COUNT INTO size"
                                 " RETURN { 'value' : value, 'names' : size }");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator it{slice};
      for (; it.valid(); ++it) {
        auto const value = it.value().get("value");
        ASSERT_TRUE(value.isNumber<double_t>()) << value.toString();
        auto const key = value.getNumber<double_t>();

        auto expectedIt = expected.find(key);
        ASSERT_NE(expectedIt, expected.end());
        auto names = it.value().get("names");
        ASSERT_TRUE(names.isNumber<size_t>()) << names.toString();
        EXPECT_EQ(expectedIt->second, names.getNumber<size_t>());
        EXPECT_EQ(expected.erase(key), 1);
      }
      EXPECT_TRUE(expected.empty());
    }
    // test grouping
    {
      containers::FlatHashMap<double_t,
                              containers::FlatHashSet<std::string_view>>
          expected{
              {100., {"A", "E", "G", "I", "J"}},
              {12., {"D", "K"}},
              {95., {"L"}},
              {90.564, {"M"}},
              {1., {"N"}},
              {0., {"O"}},
              {50., {"P"}},
              {-32.5, {"Q"}},
              {3.14, {}}  // null
          };

      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH d.value <= 100"
                                 " COLLECT value = d.value INTO name = d.name"
                                 " RETURN { 'value' : value, 'names' : name }");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator it{slice};
      EXPECT_EQ(expected.size(), it.size());
      for (; it.valid(); ++it) {
        auto const value = it.value().get("value");
        ASSERT_TRUE(value.isNumber<double_t>()) << value.toString();
        auto const key = value.getNumber<double_t>();

        auto expectedIt = expected.find(key);
        ASSERT_NE(expectedIt, expected.end());

        VPackArrayIterator names{it.value().get("names")};
        auto& expectedNames = expectedIt->second;
        if (expectedNames.empty()) {
          // array must contain singe 'null' value
          EXPECT_EQ(names.size(), 1);
          ASSERT_TRUE(names.valid());
          EXPECT_TRUE(names.value().isNull());
          names.next();
          EXPECT_FALSE(names.valid());
        } else {
          EXPECT_EQ(expectedNames.size(), names.size());
          for (; names.valid(); ++names) {
            auto const actualName = names.value().stringView();
            auto expectedNameIt = expectedNames.find(actualName);
            ASSERT_NE(expectedNameIt, expectedNames.end());
            expectedNames.erase(expectedNameIt);
          }
        }

        EXPECT_TRUE(expectedNames.empty());
        EXPECT_EQ(expected.erase(key), 1);
      }
      EXPECT_TRUE(expected.empty());
    }
    // test aggregation
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.seq < 7"
                   " COLLECT AGGREGATE sumSeq = SUM(d.seq) RETURN sumSeq",
                   VPackValue{size_t{0}}));
    }
    // test aggregation without filter condition
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView"
                   " COLLECT AGGREGATE sumSeq = SUM(d.seq) RETURN sumSeq",
                   VPackValue{size_t{475}}));
    }
    // total number of documents in a view
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView COLLECT WITH COUNT INTO count RETURN count",
          VPackValue{size_t{38}}));
    }
  }
};

class QueryAggregateView : public QueryAggregate {
 protected:
  ViewType type() const final { return arangodb::ViewType::kView; }
};

class QueryAggregateSearch : public QueryAggregate {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearch; }
};

TEST_P(QueryAggregateView, Test) {
  createCollections();
  createView(R"("trackListPositions": true, "storeValues":"id",)",
             R"("storeValues":"id",)");
  queryTests();
}

TEST_P(QueryAggregateView, TestWithoutStoreValues) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryAggregateSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryAggregateView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryAggregateSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
