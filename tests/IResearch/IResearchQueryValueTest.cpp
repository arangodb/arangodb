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

#include <velocypack/Iterator.h>

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

class QueryValue : public QueryTest {
 protected:
  void queryTests() {
    // test empty array (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH [ ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
          "RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-empty array (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto result =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH [ 'abc', "
                                        "'def' ] SORT BM25(d) ASC, TFIDF(d) "
                                        "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test numeric range (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto result =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH [ 1 .. 42 ] "
                                        "SORT BM25(d) ASC, TFIDF(d) DESC, "
                                        "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test boolean (false)
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH false SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test boolean (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH true SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test numeric (false)
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH 0 SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test numeric (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH 3.14 SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test null
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH null SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test null via bind parameter
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
          "d.seq RETURN d",
          arangodb::velocypack::Parser::fromJson("{ \"param\" : null }"));
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test null via bind parameter
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH 1 - @param SORT BM25(d) ASC, TFIDF(d) "
          "DESC, "
          "d.seq RETURN d",
          arangodb::velocypack::Parser::fromJson("{ \"param\" : 1 }"));
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test empty object (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH { } SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-empty object (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH { 'a': 123, 'b': 'cde' } SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test empty string (false)
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH '' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-empty string (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH 'abc' SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-empty string (true), LIMIT 5
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice(), _insertedDocs[1].slice(),
          _insertedDocs[2].slice(), _insertedDocs[3].slice(),
          _insertedDocs[4].slice(),
      };
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH 'abc' SORT "
                                        "BM25(d) ASC, TFIDF(d) DESC, d.seq "
                                        "LIMIT 5 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test non-empty string (true), LIMIT 5
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice(), _insertedDocs[1].slice(),
          _insertedDocs[2].slice(), _insertedDocs[3].slice(),
          _insertedDocs[4].slice(),
      };
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
          "d.seq LIMIT 5 RETURN d",
          arangodb::velocypack::Parser::fromJson("{ \"param\" : \"abc\" }"));
      ASSERT_TRUE(queryResult.result.ok());
      auto slice = queryResult.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test bind parameter (true)
    {
      std::vector<arangodb::velocypack::Slice> expected = {
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
          _insertedDocs[22].slice(), _insertedDocs[23].slice(),
          _insertedDocs[24].slice(), _insertedDocs[25].slice(),
          _insertedDocs[26].slice(), _insertedDocs[27].slice(),
          _insertedDocs[28].slice(), _insertedDocs[29].slice(),
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[32].slice(), _insertedDocs[33].slice(),
          _insertedDocs[34].slice(), _insertedDocs[35].slice(),
          _insertedDocs[36].slice(), _insertedDocs[37].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH @param SORT BM25(d) ASC, TFIDF(d) DESC, "
          "d.seq RETURN d",
          arangodb::velocypack::Parser::fromJson("{ \"param\" : [] }"));
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryValueView : public QueryValue {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }
};

class QueryValueSearch : public QueryValue {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
};

TEST_P(QueryValueView, Test) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryValueSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryValueView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryValueSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
