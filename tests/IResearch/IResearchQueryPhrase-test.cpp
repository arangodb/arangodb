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
#include "common.h"

#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryPhraseTest : public IResearchQueryTest {};

void testTerm(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for term (object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong interger in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: [1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong integer in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: [1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: [1.2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: [1.2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong bool in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: [true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong bool in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: [true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong null in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: [null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong null in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: [null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number of arguments)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {term: ['1', 1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for term (wrong number of arguments) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{term: ['1', 1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with term
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {term: 'a'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with term via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{term: 'a'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with tErm
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {tErm: 'a'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with tErm via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{tErm: 'a'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with term (in an array)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {term: ['a']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with term (in an array) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{term: ['a']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

void testStartsWith(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for starts_with (object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong integer in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: [1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong integer in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: [1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: [1.2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: [1.2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong bool in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: [true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong bool in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: [true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong null in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: [null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong null in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: [null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number of arguments)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {starts_with: ['1', 1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for starts_with (wrong number of arguments) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{starts_with: ['1', 1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with starts_with
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {starts_with: 'a'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with starts_with via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{starts_with: 'a'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with stArts_wIth
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {stArts_wIth: 'a'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with stArts_wIth via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{stArts_wIth: 'a'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with starts_with (in an array)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {starts_with: ['a']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with starts_with (in an array) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{starts_with: ['a']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

void testWildcard(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for wildcard (object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (object) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({wildcard: {a: '1'}})"
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong integer in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: [1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong interger in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: [1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: [1.2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: [1.2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong bool in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: [true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong bool in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: [true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong null in an array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: [null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong null in an array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: [null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number of arguments)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {wildcard: ['1', 1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for wildcard (wrong number of arguments) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{wildcard: ['1', 1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with wildcard
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {wildcard: '_'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wildcard via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{wildcard: '_'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wilDCard
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {wilDCard: '_'}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wilDCard via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{wilDCard: '_'}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wilDCard via [] via vriable
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{wilDCard: '_'}, 'b', 'c', 'd'])"
      "FOR d IN testView SEARCH PHRASE(d.duplicated, phraseStruct, "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wildcard (in an array)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {wildcard: ['_']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with wildcard (in an array) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{wildcard: ['_']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

void testLevenshteinMatch(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for levenshtein_match (no array object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array object) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({levenshtein_match: {a: '1'}})"
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong string)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: '1'}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong string) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: '1'}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (no array wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [{t: '1'}, 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [{t: '1'}, 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [[1], 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [[1], 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong integer 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [1, 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong integer 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [1, 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [1.2, 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [1.2, 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [true, 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [true, 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: [null, 2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: [null, 2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', {t: 2}]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', {t: 2}]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', [2]]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', [2]]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', '2']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', '2']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, {t: true}]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, {t: true}]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, [true]]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, [true]]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, 'true']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, 'true']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong integer 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, 3]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong integer 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, 3]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, 3.1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, 3.1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, {t: 42}]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong object 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, {t: 42}]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, [42]]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong array 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, [42]]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, '42']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong string 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, '42']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong bool 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong null 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number of arguments 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number of arguments 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number of arguments 5)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 2, true, 4, false]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (wrong number of arguments 5) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 2, true, 4, 42]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (negative max distance)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', -1, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (negative max distance) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', -1, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (out of range max distance)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 5, false]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (out of range max distance) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 5, false]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (out of range max distance)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {levenshtein_match: ['1', 4, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for levenshtein_match (out of range max distance) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{levenshtein_match: ['1', 4, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with levenshtein_match
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {levenshtein_match: ['f', 1]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match via [] via vaiable
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{levenshtein_match: ['f', 1]}, 'b', 'c', 'd'])"
      "FOR d IN testView SEARCH PHRASE(d.duplicated, phraseStruct, "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{levenshtein_match: ['f', 1]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with LEVenshtein_match
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {LEVenshtein_match: ['f', 1]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with LEVenshtein_match via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{LEVenshtein_match: ['f', 1]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with LEVenshtein_match via [] + limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[36].slice(), insertedDocs[37].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, ['a', 'b', 'c', {LEVenshtein_match: ['y', 1, false, 1]}], "
      "'test_analyzer') SORT BM25(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_EQUAL_SLICES(expected[i], resolved);
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with LEVenshtein_match via [] + default limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[36].slice(), insertedDocs[37].slice(),
      insertedDocs[6].slice(), insertedDocs[9].slice(),
      insertedDocs[31].slice(),

    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, ['a', 'b', 'c', {LEVenshtein_match: ['y', 1, false]}], "
      "'test_analyzer') SORT BM25(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_EQUAL_SLICES(expected[i], resolved);
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with LEVenshtein_match via [] + no limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[36].slice(), insertedDocs[37].slice(),
      insertedDocs[6].slice(), insertedDocs[9].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, ['a', 'b', 'c', {LEVenshtein_match: ['y', 1, false, 0]}], "
      "'test_analyzer') SORT BM25(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_EQUAL_SLICES(expected[i], resolved);
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match (not Damerau-Levenshtein)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {levenshtein_match: ['f', 1, false]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match via [] (not Damerau-Levenshtein)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{levenshtein_match: ['f', 1, false]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match (Damerau-Levenshtein)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {levenshtein_match: ['f', 1, true]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match via [] (Damerau-Levenshtein)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{levenshtein_match: ['f', 1, true]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with levenshtein_match via [] (Damerau-Levenshtein) via variable
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({levenshtein_match: ['f', 1, true]})"
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [phraseStruct, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

void testTerms(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for terms (no array object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong string)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: '1'}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong string) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: '1'}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (no array wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [{t: '1'}]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [{t: 1}]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [['1']]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [['1']]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong array) via [] via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{terms: [['1']]}])"
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [1.2]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [1.2]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {terms: [null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{terms: [null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong object) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[{t: '1'}]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong array) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[['1']]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong integer) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[1]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong number) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[1.2]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong bool) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[true]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for terms (wrong null) via [[]]
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [[null]]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with terms
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {terms: ['a']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terms
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {terms: ['a', 'b']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terms via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{terms: ['a', 'b']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terMs
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {terMs: ['a', 'b']}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terMs via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{terMs: ['a', 'b']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terms via [[]]
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [['a', 'b'], 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terms via [[]] with analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [['ab', 'bb'], 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with terms without analyzer
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{terms: ['ab', 'bb']}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    ASSERT_TRUE(slice.isEmptyArray());
  }

  // test parameters via reference in TERMS object
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),  insertedDocs[8].slice(),
      insertedDocs[13].slice(), insertedDocs[19].slice(),
      insertedDocs[22].slice(), insertedDocs[24].slice(),
      insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT(['v','c']) FOR d IN testView "
      "SEARCH ANALYZER(PHRASE(d['duplicated'], [{ TERMS: phraseStruct}, 2, 'c']), 'test_analyzer') "
      "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test parameters via reference full object
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),  insertedDocs[8].slice(),
      insertedDocs[13].slice(), insertedDocs[19].slice(),
      insertedDocs[22].slice(), insertedDocs[24].slice(),
      insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{ TERMS: ['v', ';']}, 2, { TERMS: ['c', ';']}]) FOR d IN testView "
      "SEARCH ANALYZER(PHRASE(d['duplicated'], phraseStruct), 'test_analyzer') "
      "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

void testInRange(TRI_vocbase_t& vocbase, const std::vector<arangodb::velocypack::Builder>& insertedDocs) {
  // test invalid input for in_range (no array object) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: {a: '1'}}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: {a: '1'}}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: {a: '1'}}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (empty array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: []}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (empty array) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: []}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }


  // test invalid input for in_range (empty array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: []}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong string) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: '1'}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong string)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: '1'}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong string) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: '1'}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong integer) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: 1}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: 1}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: 1}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong number) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: 1.2}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: 1.2}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: 1.2}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong bool) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: true}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: true}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: true}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong null) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: null}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: null}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (no array wrong null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: null}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 1) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: [{t: '1'}, '2', true, true]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [{t: '1'}, '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [{t: '1'}, '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [['1'], '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 1) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{in_range: [['1'], '2', true, true]}]) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [['1'], '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: [1, '2', true, true]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [1, '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [1, '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [1.2, '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [1.2, '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong bool 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [true, '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong bool 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [true, '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [null, '2', true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [null, '2', true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', {t: '2'}, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', {t: '2'}, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 2) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['1', ['2'], true, true]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }


  // test invalid input for in_range (wrong array 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', ['2'], true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', ['2'], true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 2) via [] and variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['1', ['2'], true, true]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], [phraseStruct]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', 2, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', 2, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', 2.1, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', 2.1, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong bool 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', true, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong bool 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', true, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', null, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', null, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', {t: true}, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', {t: true}, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 3) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['1', '2', [true], true]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', [true], true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', [true], true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong string 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', 'true', true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong string 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', 'true', true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', 3, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', 3, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', 3.1, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', 3.1, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', null, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', null, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 4) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['1', '2', true, {t: true}]}) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, {t: true}]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 4) via [] and variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT([{in_range: ['1', '2', true, {t: true}]}]) "
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong object 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, {t: true}]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, [true]]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong array 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, [true]]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong string 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, 'true']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong string 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, 'true']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, 4]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong integer 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, 4]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, 4.1]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, 4.1]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 4)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, null]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong null 4) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, null]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 1) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2']}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2']}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 5)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: ['1', '2', true, true, 5]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong number of arguments 5) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: ['1', '2', true, true, 5]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type object) via variable
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: [{t: '1'}, {t: '2'}, true, true]})"
      "FOR d IN testView SEARCH PHRASE(d['value'], phraseStruct) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [{t: '1'}, {t: '2'}, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [{t: '1'}, {t: '2'}, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [['1'], ['2'], true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [['1'], ['2'], true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type integer)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [1, 2, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type integer) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [1, 2, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type number)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [1.2, 2.1, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type number) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [1.2, 2.1, true,true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type bool)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [true, true, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type bool) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [true, true, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], {in_range: [null, null, true, true]}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input for in_range (wrong same type null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [{in_range: [null, null, true, true]}]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer with in_range (true, true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {in_range: ['a', 'b', true, true]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (true, true) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{in_range: ['a', 'b', true, true]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (true, true) via variable
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['a', 'b', true, true]})"
      "FOR d IN testView SEARCH PHRASE(d.duplicated, phraseStruct, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_rAnge
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {in_rAnge: ['a', 'b', true, true]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_rAnge via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{in_rAnge: ['a', 'b', true, true]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }


  // test custom analyzer with in_range (false, false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {in_range: ['!', 'b', false, false]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (false, false) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{in_range: ['!', 'b', false, false]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (false, true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {in_range: ['!', 'b', false, true]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range via (false, true) []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{in_range: ['!', 'b', false, true]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (true, false) via variable
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT({in_range: ['a', 'b', true, false]})"
      "FOR d IN testView SEARCH PHRASE(d.duplicated, phraseStruct, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (true, false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, {in_range: ['a', 'b', true, false]}, 0, 'b', 0, 'c', 0, 'd', "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with in_range (true, false) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [{in_range: ['a', 'b', true, false]}, 'b', 'c', 'd'], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryPhraseTest, SysVocbase) {
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  auto& sysVocBaseFeature = server.getFeature<arangodb::SystemDatabaseFeature>();

  auto sysVocBasePtr = sysVocBaseFeature.use();
  auto& vocbase = *sysVocBasePtr;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true },"
        "\"testCollection1\": { \"analyzers\": [ \"::test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.missing, 'abc') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test missing field via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid column type
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid column type via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ 1, \"abc\" ]) SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ 1, \"abc\" ]) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
  // test invalid input type (array) via ref
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET input = NOOPT([ 1, \"abc\" ])"
      "FOR d IN testView SEARCH PHRASE(d['value'], input) SORT "
      "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, true) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], false) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via ref
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET input = NOOPT(true)"
      "FOR d IN testView SEARCH PHRASE(d.value, input) SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, 3.14) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], 1234) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, { \"a\": 7, \"b\": \"c\" }) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], { \"a\": 7, \"b\": \"c\" "
        "}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value']) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), true) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "false) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), 3.14) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), 1234) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), { \"a\": "
        "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), { "
        "\"a\": 7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // can't access to local analyzer in other database
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // constexpr ANALYZER function (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(1==1, 'test_analyzer') && ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // constexpr ANALYZER function (false)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(1==2, 'test_analyzer') && ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', '::test_analyzer') "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', "
        "'_system::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'v', 1, 'z', "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test parameters via reference
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),  insertedDocs[8].slice(),
      insertedDocs[13].slice(), insertedDocs[19].slice(),
      insertedDocs[22].slice(), insertedDocs[24].slice(),
      insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "LET phraseStruct = NOOPT(['v', 2, 'c']) FOR d IN testView "
      "SEARCH ANALYZER(PHRASE(d['duplicated'], phraseStruct), 'test_analyzer') "
      "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c', 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 1, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 1, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 2, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ], 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}

TEST_F(IResearchQueryPhraseTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"::test_analyzer\", \"identity\" ], \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"analyzers\": [ \"test_analyzer\", "
        "\"_system::test_analyzer\", \"identity\" ], \"includeAllFields\": "
        "true }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.missing, 'abc') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test missing field via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid column type
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid column type via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ 1, \"abc\" ]) SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ 1, \"abc\" ]) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, true) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], false) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], null) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, 3.14) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], 1234) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, { \"a\": 7, \"b\": \"c\" }) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], { \"a\": 7, \"b\": \"c\" "
        "}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], 1, '12312', '12313') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 2)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], '12312', '12313', 2 ) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 3)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], '12312', 2, 2, '12313') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms) [] args
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], 1, ['12312'], ['12313']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 2) [] args
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], ['12312'], ['12313'], 2 ) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 3) [] args
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], ['12312'], 2, 2, ['12313']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], [1, '12312', '12313']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 2) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], ['12312', '12313', 2] ) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (invalid order of terms 3) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d['value'], ['12312', 2, 2, '12313']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value']) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), [ 1, "
        "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), true) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "false) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), null) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), 3.14) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), 1234) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), { \"a\": "
        "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), { "
        "\"a\": 7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d.duplicated, 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test custom analyzer (local)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer (local)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer (system)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer (system)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'z'), "
        "'_system::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'z', 'test_analyzer') "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH analyzer(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'v', 1, 'z', "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 2, "
        "'c', 'test_analyzer'), 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'v', 1, "
        "'c'), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
        "d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'z' ]), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 1, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 2, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, "
        "'c' ], 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, ['a', 'b'], 1, ['d'], "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, ['a', 'b', 1, 'd'], "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, ['a', 1, 'c'], 0, 'd', "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, ['a', 'b', 'c'], 0, 'd', "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, ['a', 1, 'c', 'd']), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // testarray at first arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, ['a', 1, 'c', 'd']), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test empty array at first arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, []), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }
  // test array with empty string
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, ['']), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }
  // test with empty string
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, ''), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }
  // test custom analyzer with multiple mixed offsets with empty array
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, 'a', 0, 'b', 0, 'c', 0, [], 0, 'd', "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // test custom analyzer with multiple mixed offsets with empty array check accumulating offset
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, 'b', 1, [], 2, 'r', " // bateradsfsfasdf
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }
  // testarray at first arg with analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[16].slice(), insertedDocs[26].slice(),
        insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, ['a', 1, 'c', 'd'], "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
  // array recursion simple
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, ['b', 1, ['t', 'e', 1, 'a']], "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
  // array recursion
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, ['b', 1, ['t', 'e', 1, 'a']], 0, ['d'], 0, ['s', 0, 'f', 's'], 1, [[['a', 1, 'd']]], 0, 'f', "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
  // array recursion via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.prefix, [['b', 1, ['t', 'e', 1, 'a']], 0, ['d'], 0, ['s', 0, 'f', 's'], 1, [[['a', 1, 'd']]], 0, 'f'], "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
  // array recursion without analyzer
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.prefix, ['b', 1, ['t', 'e', 1, 'a']], 0, ['d'], 0, ['s', 0, 'f', 's'], 1, [[['a', 1, 'd']]], 0, 'f'), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
  // array recursion without analyzer via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.prefix, [['b', 1, ['t', 'e', 1, 'a']], 0, ['d'], 0, ['s', 0, 'f', 's'], 1, [[['a', 1, 'd']]], 0, 'f']), "
      "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  {
    testTerm(vocbase, insertedDocs);
    testStartsWith(vocbase, insertedDocs);
    testWildcard(vocbase, insertedDocs);
    testLevenshteinMatch(vocbase, insertedDocs);
    testTerms(vocbase, insertedDocs);
    testInRange(vocbase, insertedDocs);
  }

  // test custom analyzer with mixed terms, starts_with, wildcard, levenshtein_match
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(), insertedDocs[10].slice(),
      insertedDocs[16].slice(), insertedDocs[26].slice(),
      insertedDocs[32].slice(), insertedDocs[36].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, [['a', 'b'], {starts_with: ['b']}, 0, {wildcard: '%'}, {levenshtein_match: ['f', 1, true]}], "
      "'test_analyzer') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }
}
