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

class QueryComplexBool : public QueryTest {
 protected:
  void queryTestsIdentity() {
    // (A && B && !C)
    // field && prefix && !exists
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[36].slice(),  // STARTS_WITH matches (duplicated term)
          _insertedDocs[37].slice(),  // STARTS_WITH matches (duplicated term)
          _insertedDocs[26].slice(),  // STARTS_WITH matches (unique term short)
          _insertedDocs[31].slice(),  // STARTS_WITH matches (unique term long)
      };
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH d.same == 'xyz'"
                                 "  && STARTS_WITH(d['prefix'], 'abc')"
                                 "  && NOT EXISTS(d.value)"
                                 " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                 " RETURN d");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      size_t i = 0;
      for (VPackArrayIterator it{slice}; it.valid(); ++it) {
        auto const resolved = it.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expected[i++],
                                                           resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
  }

  void queryTestsMulti() {
    // (A || B || C || !D)
    // (prefix || phrase || exists || !field)
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[0].slice(), _insertedDocs[1].slice(),
          _insertedDocs[2].slice(), _insertedDocs[4].slice(),
          _insertedDocs[5].slice(), _insertedDocs[10].slice(),
          _insertedDocs[11].slice(), _insertedDocs[12].slice(),
          _insertedDocs[14].slice(), _insertedDocs[15].slice(),
          _insertedDocs[16].slice(), _insertedDocs[17].slice(),
          _insertedDocs[18].slice(), _insertedDocs[20].slice(),
          _insertedDocs[21].slice(), _insertedDocs[23].slice(),
          _insertedDocs[25].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[30].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[7]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[8]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[13]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[19]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[22]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[24]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[29]
              .slice(),  // STARTS_WITH does not match, PHRASE matches
          _insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term),
                                      // PHRASE does not match
          _insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term),
                                      // PHRASE does not match
          _insertedDocs[6].slice(),   // STARTS_WITH matches (unique term),
                                      // PHRASE does not match
          _insertedDocs[9].slice(),   // STARTS_WITH matches (unique term),
                                      // PHRASE does not match
          _insertedDocs[26].slice(),  // STARTS_WITH matches (unique term),
                                      // PHRASE does not match
          _insertedDocs[31].slice(),  // STARTS_WITH matches (unique term),
                                      // PHRASE does not match
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.prefix, 'abc')"
          "  || ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "  || EXISTS(d.same) || d['value'] != 3.14"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      size_t i = 0;
      for (VPackArrayIterator it{slice}; it.valid(); ++it) {
        auto const resolved = it.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expected[i++],
                                                           resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    // (A && B) || (C && D)
    // (field && prefix) || (phrase && exists)
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[7].slice(),   // PHRASE matches
          _insertedDocs[8].slice(),   // PHRASE matches
          _insertedDocs[13].slice(),  // PHRASE matches
          _insertedDocs[19].slice(),  // PHRASE matches
          _insertedDocs[22].slice(),  // PHRASE matches
          _insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term)
          _insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term)
          _insertedDocs[6].slice(),   // STARTS_WITH matches (unique term 4char)
          _insertedDocs[9].slice(),   // STARTS_WITH matches (unique term 5char)
          _insertedDocs[26].slice(),  // STARTS_WITH matches (unique term 3char)
          _insertedDocs[31].slice(),  // STARTS_WITH matches (unique term 7char)
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH (d['same'] == 'xyz' && "
          "STARTS_WITH(d.prefix, "
          "'abc')) || (ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') "
          "&& EXISTS(d.value)) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();
      size_t i = 0;

      for (VPackArrayIterator it(slice); it.valid(); ++it) {
        auto const resolved = it.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(expected[i++],
                                                            resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }
    // (A && B) || (C && D)
    // (field && prefix) || (phrase && exists)
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[7].slice(),   // PHRASE matches
          _insertedDocs[8].slice(),   // PHRASE matches
          _insertedDocs[13].slice(),  // PHRASE matches
          _insertedDocs[19].slice(),  // PHRASE matches
          _insertedDocs[22].slice(),  // PHRASE matches
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH (d['same'] == 'xyz' && "
          "STARTS_WITH(d.prefix, "
          "'abc')) || (ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') "
          "&& EXISTS(d.value)) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq limit 5 "
          "RETURN d");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      size_t i = 0;
      for (VPackArrayIterator it{slice}; it.valid(); ++it) {
        auto const resolved = it.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expected[i++],
                                                           resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    // (A || B) && (C || D || E)
    // (field || exists) && (starts_with || phrase || range)
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[3].slice(), _insertedDocs[4].slice(),
          _insertedDocs[5].slice(), _insertedDocs[10].slice(),
          _insertedDocs[11].slice(), _insertedDocs[12].slice(),
          _insertedDocs[14].slice(), _insertedDocs[15].slice(),
          _insertedDocs[16].slice(), _insertedDocs[17].slice(),
          _insertedDocs[18].slice(), _insertedDocs[20].slice(),
          _insertedDocs[21].slice(), _insertedDocs[23].slice(),
          _insertedDocs[25].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[30].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[24].slice(),  // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS does not match
          _insertedDocs[29].slice(),  // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS does not match
          _insertedDocs[7].slice(),   // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS matches
          _insertedDocs[8].slice(),   // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS matches
          _insertedDocs[13].slice(),  // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS matches
          _insertedDocs[19].slice(),  // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS matches
          _insertedDocs[22].slice(),  // STARTS_WITH does not match, PHRASE
                                      // matches, EXISTS matches
          _insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term),
                                      // PHRASE does not match, EXISTS does not
                                      // match
          _insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term),
                                      // PHRASE does not match, EXISTS does not
                                      // match
          _insertedDocs[26]
              .slice(),  // STARTS_WITH matches (unique term), PHRASE
                         // does not match, EXISTS does not match
          _insertedDocs[31]
              .slice(),  // STARTS_WITH matches (unique term), PHRASE
                         // does not match, EXISTS does not match
          _insertedDocs[6].slice(),  // STARTS_WITH matches (unique term),
                                     // PHRASE does not match, EXISTS matches
          _insertedDocs[9].slice(),  // STARTS_WITH matches (unique term),
                                     // PHRASE does not match, EXISTS matches
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH (d.same == 'xyz' || EXISTS(d['value']))"
          "  && (STARTS_WITH(d.prefix, 'abc')"
          "      || ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "      || d.seq >= -3)"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
      auto slice = result.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      size_t i = 0;
      for (VPackArrayIterator it{slice}; it.valid(); ++it) {
        auto const resolved = it.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expected[i++],
                                                           resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryComplexBoolView : public QueryComplexBool {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }
};

class QueryComplexBoolSearch : public QueryComplexBool {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }
};

TEST_P(QueryComplexBoolView, Test) {
  createCollections();
  createView(
      R"("trackListPositions": true, "storeValues": "id",)",
      R"("analyzers": [ "test_analyzer", "identity" ], "storeValues": "id",)");
  queryTestsIdentity();
  queryTestsMulti();
}

TEST_P(QueryComplexBoolSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true, "storeValues": "id",)",
                R"("analyzer": "identity", "storeValues": "id",)");
  createSearch();
  queryTestsIdentity();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryComplexBoolView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryComplexBoolSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
