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

class QueryExists : public QueryTest {
 protected:
  void queryTests() {
    std::vector<velocypack::Slice> empty;
    // test non-existent (any)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.missing) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (any) via []
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d['missing']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (bool)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.name, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (bool) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['name'], 'bool')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (boolean)
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.name, 'boolean')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (boolean) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['name'], 'boolean')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (numeric)
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.name, 'numeric')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (numeric) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['name'], 'numeric')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (null)
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.name, 'null')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (null) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['name'], 'null')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (string)
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.seq, 'string')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (string) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['seq'], 'string')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (text analyzer)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.seq, 'analyzer', 'text_en')"
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (text analyzer)
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.seq, 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (analyzer) via []
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['seq'], 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (array)
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.value[2])"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (array) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['value'][2])"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test non-existent (object)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH EXISTS(d.value.d) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          empty));
    }
    // test non-existent (object) via []
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['value']['d'])"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   empty));
    }
    // test existent (any)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),  _insertedDocs[1].slice(),
          _insertedDocs[2].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[4].slice(),  _insertedDocs[5].slice(),
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
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.value)"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }

    // test existent (any) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),  _insertedDocs[1].slice(),
          _insertedDocs[2].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[4].slice(),  _insertedDocs[5].slice(),
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
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d['value'])"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }
    // test existent (bool)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH EXISTS(d.value, 'bool')"
                   " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d",
                   expected));
    }
    // test existent (bool) with bound params
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, @type) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson("{ \"type\" : \"bool\" }"));
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (bool) with bound view name
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"testView\" }"));

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (bool) with invalid bound view name
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"invlaidViewName\" }"));

      ASSERT_TRUE(result.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }

    // test existent (bool) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'bool') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (boolean)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'boolean') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (boolean) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'boolean') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[3].slice(),  _insertedDocs[6].slice(),
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[11].slice(), _insertedDocs[12].slice(),
          _insertedDocs[13].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(), _insertedDocs[16].slice(),
          _insertedDocs[17].slice(), _insertedDocs[18].slice(),
          _insertedDocs[19].slice(), _insertedDocs[20].slice(),
          _insertedDocs[21].slice(), _insertedDocs[22].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'numeric') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[3].slice(),  _insertedDocs[6].slice(),
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[11].slice(), _insertedDocs[12].slice(),
          _insertedDocs[13].slice(), _insertedDocs[14].slice(),
          _insertedDocs[15].slice(), _insertedDocs[16].slice(),
          _insertedDocs[17].slice(), _insertedDocs[18].slice(),
          _insertedDocs[19].slice(), _insertedDocs[20].slice(),
          _insertedDocs[21].slice(), _insertedDocs[22].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric) via [], limit  5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[3].slice(), _insertedDocs[6].slice(),
          _insertedDocs[7].slice(), _insertedDocs[8].slice(),
          _insertedDocs[9].slice()};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq LIMIT 5 RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (null)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'null') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (null) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'null') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer', 'identity') "
          "SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (string)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any string)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'string'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any string)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'string') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any string) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'string') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer', 'identity') "
          "SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (identity analyzer) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[2].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'analyzer', 'identity') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (array)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[4].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value[1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (array) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[4].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'][1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (object)
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[5].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value.b) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (object) via []
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[5].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']['b']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }
  }

  void queryTests2() {
    // test non-existent (any)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.missing) "
                                 "SORT BM25(d) ASC, TFIDF(d) "
                                 "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (any) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['missing']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (bool)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (bool) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d['name'], "
                                 "'bool') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (boolean)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.name, "
                                 "'boolean') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (boolean) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'boolean') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (numeric)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.name, "
                                 "'numeric') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (numeric) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (null)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'null') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (null) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d['name'], "
                                 "'null') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (string)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.seq, 'string') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (string) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d['seq'], "
                                 "'string') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (text analyzer)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.seq, 'analyzer', 'text_en') SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (text analyzer)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.seq, 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (analyzer) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['seq'], 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (array)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value[2]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (array) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'][2]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (object)
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value.d) "
                                 "SORT BM25(d) ASC, TFIDF(d) "
                                 "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-existent (object) via []
    {
      std::vector<velocypack::Slice> expected = {};
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']['d']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any)
    {
      std::vector<velocypack::Slice> expected = {
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
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value) SORT BM25(d) ASC, TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any) via []
    {
      std::vector<velocypack::Slice> expected = {
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
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (bool)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (bool) with bound params
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, @type) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson("{ \"type\" : \"bool\" }"));
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (bool) with bound view name
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"testView\" }"));

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (bool) with invalid bound view name
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"invlaidViewName\" }"));

      ASSERT_TRUE(result.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }

    // test existent (bool) via []
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d['value'], "
                                 "'bool') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (boolean)
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value, "
                                 "'boolean') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (boolean) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'boolean') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (numeric)
    {
      std::vector<velocypack::Slice> expected = {
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
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value, "
                                 "'numeric') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric) via []
    {
      std::vector<velocypack::Slice> expected = {
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
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric) via [], limit  5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[10].slice(),
      };
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq LIMIT 5 RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (null)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'null') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (null) via []
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d['value'], "
                                 "'null') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer)
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value, "
                                 "'analyzer', 'identity') SORT "
                                 "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (string)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (any string)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'string'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (any string)
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value, "
                                 "'string') SORT BM25(d) ASC, "
                                 "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (any string) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'string') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer)
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value, "
                                 "'analyzer', 'identity') SORT "
                                 "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (identity analyzer) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'analyzer', 'identity') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (array)
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value[1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (array) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'][1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (object)
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView SEARCH EXISTS(d.value.b) "
                                 "SORT BM25(d) ASC, TFIDF(d) "
                                 "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (object) via []
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']['b']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }
  }
};

class QueryExistsView : public QueryExists {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }
};

class QueryExistsSearch : public QueryExists {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }
};

TEST_P(QueryExistsView, Test) {
  createCollections();
  createView(R"("trackListPositions": true, "storeValues": "id",)",
             R"("storeValues": "id",)");
  queryTests();
}

TEST_P(QueryExistsSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true, "storeValues": "id",)",
                R"("storeValues": "id",)");
  createSearch();
  queryTests();
}

TEST_P(QueryExistsView, StoreMaskPartially) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"("storeValues": "id",)");
  queryTests2();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryExistsView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryExistsSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
