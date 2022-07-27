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

class QueryIn : public QueryTest {
 protected:
  void queryTests() {
    // test array
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN [ [ -1, 0 ], [ 1, \"abc\" ] ] "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test array via []
    {
      auto result =
          executeQuery(_vocbase,
                       "FOR d IN testView SEARCH d['value'] IN [ [ -1, 0 ], [ "
                       "1, \"abc\" ] ] "
                       "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test bool
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH d.value IN [ true ] SORT BM25(d) ASC, "
            "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(expected[i++],
                                                              resolved, true)));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value NOT IN [ "
                                   "true ] SORT BM25(d) ASC, "
                                   "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test bool via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d['value'] IN [ "
                                   "true, false ] SORT BM25(d) "
                                   "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d['value'] "
                                   "NOT IN [ true, false ] SORT BM25(d) "
                                   "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test numeric
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[8].slice(),
          _insertedDocs[11].slice(),
          _insertedDocs[13].slice(),
      };
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value IN [ 123, "
                                   "1234 ] SORT BM25(d) ASC, "
                                   "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value NOT "
                                   "IN [ 123, 1234 ] SORT BM25(d) ASC, "
                                   "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test numeric, limit 2
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[8].slice(),
          _insertedDocs[11].slice(),
      };
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value IN [ 123, "
                                   "1234 ] SORT BM25(d) ASC, "
                                   "TFIDF(d) DESC, d.seq LIMIT 2 RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value NOT IN [ "
                                   "123, 1234 ] SORT BM25(d) "
                                   "ASC, "
                                   "TFIDF(d) DESC, d.seq LIMIT 2 RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          // this also must not be there (it is not in expected due to LIMIT
          // clause)
          EXPECT_NE(0, basics::VelocyPackHelper::compare(
                           _insertedDocs[13].slice(), resolved, true));
          ++i;
        }
        EXPECT_EQ(i, 2);
      }
    }

    // test numeric via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[8].slice(),
          _insertedDocs[11].slice(),
          _insertedDocs[13].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH d['value'] IN [ 123, 1234 ] SORT BM25(d) "
            "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d['value'] "
                                   "NOT IN [ 123, 1234 ] SORT BM25(d) "
                                   "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test null
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH d.value IN [ null ] SORT BM25(d) ASC, "
            "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d.value NOT IN [ "
                                   "null ] SORT BM25(d) ASC, "
                                   "TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test null via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
      };
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d['value'] IN [ "
                                   "null, null ] SORT BM25(d) "
                                   "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }

        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(_vocbase,
                                   "FOR d IN testView SEARCH d['value'] "
                                   "NOT IN [ null, null ] SORT BM25(d) "
                                   "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test object
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN [ { \"a\": 7, \"b\": \"c\" } ] "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test object via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),
      };
      auto result =
          executeQuery(_vocbase,
                       "FOR d IN testView SEARCH d['value'] IN [ {}, { \"a\": "
                       "7, \"b\": \"c\" "
                       "} ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test string
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      {
        auto r = executeQuery(_vocbase,
                              "FOR d IN testView SEARCH d.value IN [ "
                              "\"abc\", \"xyz\" ] SORT BM25(d) "
                              "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
        auto slice = r.data->slice();
        ASSERT_TRUE(slice.isArray()) << slice.toString();

        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto r = executeQuery(_vocbase,
                              "FOR d IN testView SEARCH d.value NOT "
                              "IN [ \"abc\", \"xyz\" ] SORT BM25(d) "
                              "ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
        auto slice = r.data->slice();
        ASSERT_TRUE(slice.isArray()) << slice.toString();
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // test string via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH d['value'] IN [ \"abc\", \"xyz\" ] SORT "
            "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }

        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH d['value'] NOT IN [ \"abc\", \"xyz\" ] "
            "SORT "
            "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }
  }
};

class QueryInView : public QueryIn {
 protected:
  ViewType type() const final { return ViewType::kView; }
};

class QueryInSearch : public QueryIn {
 protected:
  ViewType type() const final { return ViewType::kSearch; }
};

TEST_P(QueryInView, Test) {
  createCollections();
  createView(R"("trackListPositions": true, "storeValues":"id",)",
             R"("storeValues":"id",)");
  queryTests();
}

TEST_P(QueryInView, TestWithoutStoreValues) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryInSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryInView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryInSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
