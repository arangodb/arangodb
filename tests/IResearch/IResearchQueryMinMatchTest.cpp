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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

class QueryMinMatch : public QueryTest {
 protected:
  void queryTests() {
    // same as term query
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', 1) RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // same as disjunction
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, 1) "
          "SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // same as disjunction
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, 1.0) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // non-deterministic conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, "
          "CEIL(RAND())) SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, '1') "
          "SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, {}) "
          "SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, []) "
          "SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, null) "
          "SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid conditions count type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, true) "
          "SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // missing conditions count argument
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1) SORT "
          "d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // missing conditions count argument
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A') SORT d.seq RETURN "
          "d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // missing arguments
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH MIN_MATCH() SORT d.seq RETURN d");
      ASSERT_FALSE(result.result.ok());
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // constexpr min match (true)
    {
      std::string const query =
          "FOR d IN testView SEARCH MIN_MATCH(1==1, 2==2, 3==3, 2) "
          "SORT d.seq "
          "RETURN d";
      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      ASSERT_TRUE(slice.isArray());
      ASSERT_EQ(_insertedDocs.size(), slice.length());
    }

    // constexpr min match (false)
    {
      std::string const query =
          "FOR d IN testView SEARCH MIN_MATCH(1==5, 2==6, 3==3, 2) "
          "SORT d.seq "
          "RETURN d";
      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      ASSERT_TRUE(slice.isArray());
      ASSERT_EQ(0U, slice.length());
    }

    // same as disjunction
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, 1) "
          "SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // same as conjunction
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 0, 2) "
          "SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // unreachable condition (conjunction)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, 2) "
          "SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      ASSERT_EQ(0U, slice.length());
    }

    // unreachable condition
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, 3) "
          "SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      ASSERT_EQ(0U, slice.length());
    }

    // 2 conditions
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, "
          "d.value "
          ">= 100 || d.value <= 150, 2) SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // 2 conditions
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, d.seq "
          "== 'xxx', d.value >= 100 || d.value <= 150, 2) SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // 2 conditions
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[10].slice(), _insertedDocs[11].slice(),
          _insertedDocs[12].slice(), _insertedDocs[13].slice(),
          _insertedDocs[14].slice(), _insertedDocs[15].slice(),
          _insertedDocs[16].slice(), _insertedDocs[17].slice(),
          _insertedDocs[18].slice(), _insertedDocs[19].slice(),
          _insertedDocs[20].slice(), _insertedDocs[21].slice(),
          _insertedDocs[22].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, "
          "d.same "
          "== 'xyz', d.value >= 100 || d.value <= 150, 2) SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // 3 conditions
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[6].slice(), _insertedDocs[7].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH MIN_MATCH(d.name == 'A', d.seq == 1, "
          "d.same "
          "== 'xyz', d.value >= 100 || d.value <= 150, 3) SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryMinMatchView : public QueryMinMatch {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }
};

class QueryMinMatchSearch : public QueryMinMatch {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
};

TEST_P(QueryMinMatchView, Test) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryMinMatchSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryMinMatchView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryMinMatchSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
