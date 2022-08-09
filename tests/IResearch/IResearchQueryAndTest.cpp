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

class QueryAnd : public QueryTest {
 protected:
  void queryTestsIdentity() {
    // field and missing field
    {
      std::vector<VPackSlice> expected = {};
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.invalid == 2"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // two different fields
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[12].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.value == 100"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // not field and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[12].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH NOT (d['same'] == 'abc') AND d.value == 100"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // field and prefix
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
          _insertedDocs[6].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[26].slice(), _insertedDocs[31].slice(),
      };
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.same == 'xyz'"
                   "  AND STARTS_WITH(d['prefix'], 'abc')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }
    // not prefix and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[10].slice(), _insertedDocs[11].slice(),
          _insertedDocs[12].slice(), _insertedDocs[13].slice(),
          _insertedDocs[14].slice(), _insertedDocs[15].slice(),
          _insertedDocs[16].slice(), _insertedDocs[17].slice(),
          _insertedDocs[18].slice(), _insertedDocs[19].slice(),
          _insertedDocs[20].slice(), _insertedDocs[21].slice(),
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[27].slice(), _insertedDocs[28].slice(),
          _insertedDocs[29].slice(), _insertedDocs[30].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH NOT STARTS_WITH(d['prefix'], 'abc')"
          "  AND d.same == 'xyz'"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // field and exists
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[14].slice(), _insertedDocs[21].slice(),
          _insertedDocs[26].slice(), _insertedDocs[29].slice(),
          _insertedDocs[31].slice(), _insertedDocs[34].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.same == 'xyz' AND EXISTS(d['prefix'])"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // not exists and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[10].slice(), _insertedDocs[11].slice(),
          _insertedDocs[12].slice(), _insertedDocs[13].slice(),
          _insertedDocs[15].slice(), _insertedDocs[16].slice(),
          _insertedDocs[17].slice(), _insertedDocs[18].slice(),
          _insertedDocs[19].slice(), _insertedDocs[20].slice(),
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[27].slice(), _insertedDocs[28].slice(),
          _insertedDocs[30].slice(), _insertedDocs[32].slice(),
          _insertedDocs[33].slice(), _insertedDocs[35].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH NOT EXISTS(d['prefix']) AND d.same == 'xyz'"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // prefix and not exists and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[37].slice(),
          _insertedDocs[9].slice(),
          _insertedDocs[31].slice(),
      };
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc')"
                   "  AND NOT EXISTS(d.duplicated) AND d.same == 'xyz'"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }
    // prefix and not exists and field with limit
    {
      std::vector<VPackSlice> expected = {_insertedDocs[37].slice(),
                                          _insertedDocs[9].slice()};
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc')"
                   "  AND NOT EXISTS(d.duplicated) AND d.same == 'xyz'"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq LIMIT 2 RETURN d",
                   expected));
    }
  }
  void queryTestsMulti() {
    // field and phrase
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[13].slice(), _insertedDocs[19].slice(),
          _insertedDocs[22].slice(), _insertedDocs[24].slice(),
          _insertedDocs[29].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.same == 'xyz'"
          "  AND ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // not phrase and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[10].slice(), _insertedDocs[11].slice(),
          _insertedDocs[12].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(), _insertedDocs[16].slice(),
          _insertedDocs[17].slice(), _insertedDocs[18].slice(),
          _insertedDocs[20].slice(), _insertedDocs[21].slice(),
          _insertedDocs[23].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[30].slice(),
          _insertedDocs[31].slice(), _insertedDocs[32].slice(),
          _insertedDocs[33].slice(), _insertedDocs[34].slice(),
          _insertedDocs[35].slice(), _insertedDocs[36].slice(),
          _insertedDocs[37].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH"
          "  NOT ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "  AND d.same == 'xyz'"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // not phrase and field
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[10].slice(), _insertedDocs[11].slice(),
          _insertedDocs[12].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(), _insertedDocs[16].slice(),
          _insertedDocs[17].slice(), _insertedDocs[18].slice(),
          _insertedDocs[20].slice(), _insertedDocs[21].slice(),
          _insertedDocs[23].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[30].slice(),
          _insertedDocs[31].slice(), _insertedDocs[32].slice(),
          _insertedDocs[33].slice(), _insertedDocs[34].slice(),
          _insertedDocs[35].slice(), _insertedDocs[36].slice(),
          _insertedDocs[37].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH"
          "  ANALYZER(NOT PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "  AND d.same == 'xyz'"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // phrase and not field and exists
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[29].slice(),
      };
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH "
                   "  ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
                   "  AND NOT (d.same == 'abc') AND EXISTS(d['prefix'])"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }
    // exists and not prefix and phrase and not field and range
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[29].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.name)"
          "  AND NOT STARTS_WITH(d['prefix'], 'abc')"
          "  AND ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "  AND NOT (d.same == 'abc') AND d.seq >= 23"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
    // exists and not prefix and phrase and not field and range
    {
      std::vector<VPackSlice> expected = {
          _insertedDocs[29].slice(),
      };
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.name)"
          "  AND NOT STARTS_WITH(d['prefix'], 'abc')"
          "  AND ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer')"
          "  AND NOT (d.same == 'abc') AND d.seq >= 23"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          expected));
    }
  }
};

class QueryAndView : public QueryAnd {
 protected:
  ViewType type() const final { return arangodb::ViewType::kView; }
};

class QueryAndSearch : public QueryAnd {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearch; }
};

TEST_P(QueryAndView, Test) {
  createCollections();
  createView(R"("analyzers": [ "test_analyzer",  "identity" ],
                "trackListPositions": true,
                "storeValues": "id",)",
             R"("analyzers": [ "test_analyzer",  "identity" ],
                "storeValues":"id",)");
  queryTestsIdentity();
  queryTestsMulti();
}

TEST_P(QueryAndSearch, TestIdentity) {
  createCollections();
  createIndexes(R"("analyzer": "identity",
                   "trackListPositions": true,)",
                R"("analyzer": "identity",)");
  createSearch();
  queryTestsIdentity();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryAndView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryAndSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
