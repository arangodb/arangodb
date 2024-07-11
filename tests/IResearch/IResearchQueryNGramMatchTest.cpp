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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include <absl/strings/str_replace.h>

#include <velocypack/Iterator.h>

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

enum Analyzer : unsigned {
  kAnalyzerIdentity = 1 << 0,
  kAnalyzerMyNgram = 1 << 1,
  kAnalyzerMyNgramUser = 1 << 2,
};

std::string_view toString(Analyzer analyzer) noexcept {
  switch (analyzer) {
    case kAnalyzerIdentity:
      return "identity";
    case kAnalyzerMyNgram:
      return "::myngram";
    case kAnalyzerMyNgramUser:
      return "testVocbase::myngram";
  }
  return "";
}

class QueryNGramMatch : public QueryTest {
 protected:
  void create(bool system) {
    if (system) {
      auto& sysVocBaseFeature = server.getFeature<SystemDatabaseFeature>();
      auto sysVocBasePtr = sysVocBaseFeature.use();
      auto& vocbase = *sysVocBasePtr;
      // 2-gram analyzer
      {
        auto& analyzers =
            server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
        arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

        auto res = analyzers.emplace(
            result, "_system::myngram", "ngram",
            VPackParser::fromJson(
                "{\"min\":2, \"max\":2, \"streamType\":\"utf8\", "
                "\"preserveOriginal\":false}")
                ->slice(),
            arangodb::transaction::OperationOriginTestCase{},
            arangodb::iresearch::Features(
                irs::IndexFeatures::FREQ |
                irs::IndexFeatures::POS)  // required for PHRASE
        );                                // cache analyzer
        EXPECT_TRUE(res.ok());
      }
      // create collection0
      {
        auto createJson = arangodb::velocypack::Parser::fromJson(
            "{ \"name\": \"testCollection0\" }");
        auto collection = vocbase.createCollection(createJson->slice());
        ASSERT_NE(nullptr, collection);

        std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -6, \"value\": \"Jack Daniels\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -5, \"value\": \"Jack Sparrow\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -4, \"value\": \"Daniel Sorano\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -3, \"value\": \"Sinderella\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -2, \"value\": \"Jack the Ripper\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -1, \"value\": \"Jack Rabbit\" }"),
        };

        arangodb::OperationOptions options;
        options.returnNew = true;
        arangodb::SingleCollectionTransaction trx(
            arangodb::transaction::StandaloneContext::create(
                vocbase, arangodb::transaction::OperationOriginTestCase{}),
            *collection, arangodb::AccessMode::Type::WRITE);
        EXPECT_TRUE(trx.begin().ok());

        for (auto& entry : docs) {
          auto res = trx.insert(collection->name(), entry->slice(), options);
          EXPECT_TRUE(res.ok());
          _insertedDocs.emplace_back(res.slice().get("new"));
        }

        EXPECT_TRUE(trx.commit().ok());
      }
    } else {
      auto& vocbase = _vocbase;
      // 2-gram analyzer
      {
        auto& analyzers =
            server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
        arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

        auto res = analyzers.emplace(
            result, "testVocbase::myngram", "ngram",
            VPackParser::fromJson(
                "{\"min\":2, \"max\":2, \"streamType\":\"utf8\", "
                "\"preserveOriginal\":false}")
                ->slice(),
            arangodb::transaction::OperationOriginTestCase{},
            arangodb::iresearch::Features(
                irs::IndexFeatures::FREQ |
                irs::IndexFeatures::POS)  // required for PHRASE
        );                                // cache analyzer
        EXPECT_TRUE(res.ok());
      }
      // 2-gram analyzer in another DB
      {
        auto& analyzers =
            server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
        arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

        TRI_vocbase_t* vocbase2;
        auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
        dbFeature.createDatabase(testDBInfo(server.server(), "testVocbase2"),
                                 vocbase2);
        std::shared_ptr<arangodb::LogicalCollection> unused;
        ASSERT_NE(nullptr, vocbase2);
        arangodb::OperationOptions options(arangodb::ExecContext::current());
        arangodb::methods::Collections::createSystem(
            *vocbase2, options, arangodb::tests::AnalyzerCollectionName, false,
            unused);

        auto res = analyzers.emplace(
            result, "testVocbase2::myngram", "ngram",
            VPackParser::fromJson(
                "{\"min\":2, \"max\":2, \"streamType\":\"utf8\", "
                "\"preserveOriginal\":false}")
                ->slice(),
            arangodb::transaction::OperationOriginTestCase{},
            arangodb::iresearch::Features(
                irs::IndexFeatures::FREQ |
                irs::IndexFeatures::POS));  // cache analyzer
        EXPECT_TRUE(res.ok());
      }
      // create collection0
      {
        auto createJson = arangodb::velocypack::Parser::fromJson(
            "{ \"name\": \"testCollection0\" }");
        auto collection = vocbase.createCollection(createJson->slice());
        ASSERT_NE(nullptr, collection);

        std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -6, \"value\": \"Jack Daniels\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -5, \"value\": \"Jack Sparrow\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -4, \"value\": \"Daniel Sorano\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -3, \"value\": \"Sinderella\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -2, \"value\": \"Jack the Ripper\" }"),
            arangodb::velocypack::Parser::fromJson(
                "{ \"seq\": -1, \"value\": \"Jack Rabbit\" }"),
        };

        arangodb::OperationOptions options;
        options.returnNew = true;
        arangodb::SingleCollectionTransaction trx(
            arangodb::transaction::StandaloneContext::create(
                vocbase, arangodb::transaction::OperationOriginTestCase{}),
            *collection, arangodb::AccessMode::Type::WRITE);
        EXPECT_TRUE(trx.begin().ok());

        for (auto& entry : docs) {
          auto res = trx.insert(collection->name(), entry->slice(), options);
          EXPECT_TRUE(res.ok());
          _insertedDocs.emplace_back(res.slice().get("new"));
        }

        EXPECT_TRUE(trx.commit().ok());
      }
    }
  }

  void queryTestsSys(unsigned flags) {
    EXPECT_FALSE(flags & kAnalyzerMyNgramUser);
    auto& sysVocBaseFeature = server.getFeature<SystemDatabaseFeature>();
    auto sysVocBasePtr = sysVocBaseFeature.use();
    auto& vocbase = *sysVocBasePtr;
    // test missing field
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.missing, 'abc', 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test missing field via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['missing'], 'abc', 0.5, "
          "'myngram') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid column type
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.seq, '0', 0.5, 'myngram') "
          "SORT "
          "BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid column type via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['seq'], '0', 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid input type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, [ 1, \"abc\" ], 0.5, "
          "'myngram') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], [ 1, \"abc\" ], "
          "0.5, "
          "'myngram') SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, true, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], false, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, null, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], null, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 3.14, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 1234, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, { \"a\": 7, \"b\": "
          "\"c\" }, 0.5, 'myngram') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], { \"a\": 7, \"b\": "
          "\"c\" "
          "}, 0.5, 'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test missing value
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test too much args
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 'test', 0.5, "
          "'analyzer', 'too much') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test invalid threshold type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', '123') "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', { \"a\": "
          "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', null) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold value (> 1)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 1.1) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold value (0)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, true) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, null) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, 5) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, { "
          "\"a\": "
          "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test undefined analyzer
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
          "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // can't access to local analyzer in other database
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
          "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, "
          "d.seq "
          "RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test via ANALYZER function
    if (flags & kAnalyzerMyNgram) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(NGRAM_mATCH(d.value, 'Jack "
          "Daniels', 0.7), "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgram) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Daniels', 0.7, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgram) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[1].slice()};
      // Searching for Jack Arrow and we have Jack Sparrow which will be matched
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Arrow', 0.5, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgram) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice(), _insertedDocs[4].slice(),
          _insertedDocs[5].slice(), _insertedDocs[1].slice()};
      // Searching for Jack Arrow and set low threshold to match all Jack`s
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Arrow', 0.2, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via default analyzer
    if (flags & kAnalyzerIdentity) {
      std::vector<arangodb::velocypack::Slice> expected = {
          // exact match because fallback to term query in case of single ngram
          _insertedDocs[0].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Daniels', 1) "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size()) << slice.toJson();
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }
      EXPECT_EQ(i, expected.size());
    }
  }

  void queryTests(unsigned flags) {
    EXPECT_FALSE(flags & kAnalyzerMyNgram);
    auto& vocbase = _vocbase;
    // test missing field
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.missing, 'abc', 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test missing field via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['missing'], 'abc', 0.5, "
          "'myngram') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid column type
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.seq, '0', 0.5, 'myngram') "
          "SORT "
          "BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid column type via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['seq'], '0', 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

    // test invalid input type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, [ 1, \"abc\" ], 0.5, "
          "'myngram') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], [ 1, \"abc\" ], "
          "0.5, "
          "'myngram') SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, true, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], false, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, null, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], null, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 3.14, 0.5, 'myngram') "
          "SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 1234, 0.5, "
          "'myngram') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, { \"a\": 7, \"b\": "
          "\"c\" }, 0.5, 'myngram') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], { \"a\": 7, \"b\": "
          "\"c\" "
          "}, 0.5, 'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test missing value
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test missing value via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d['value']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test too much args
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 'abs', 0.5, "
          "'identity', "
          "'too much') SORT BM25(d) ASC, TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test invalid threshold type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', '123') "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', { \"a\": "
          "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', null) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold value (> 1)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 1.1) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid threshold value (0)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, true) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, null) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, 5) "
          " SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, { "
          "\"a\": "
          "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test undefined analyzer
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
          "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // can't access to local analyzer in other database
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
          "'testVocbase2::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, "
          "d.seq "
          "RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test via ANALYZER function
    if (flags & kAnalyzerMyNgramUser) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(NGRAM_mATCH(d.value, 'Jack "
          "Daniels', 0.7), "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgramUser) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Daniels', 0.7, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter with default threshold
    if (flags & kAnalyzerMyNgramUser) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Daniels', "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgramUser) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[1].slice()};
      // Searching for Jack Arrow and we have Jack Sparrow which will be matched
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Arrow', 0.5, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via analyzer parameter
    if (flags & kAnalyzerMyNgramUser) {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice(), _insertedDocs[4].slice(),
          _insertedDocs[5].slice(), _insertedDocs[1].slice()};
      // Searching for Jack Arrow and set low threshold to match all Jack`s
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Arrow', 0.2, "
          "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

    // test via default analyzer
    if (flags & kAnalyzerIdentity) {
      std::vector<arangodb::velocypack::Slice> expected = {
          // exact match because fallback to term query in case of single ngram
          _insertedDocs[0].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH nGrAm_MaTcH(d.value, 'Jack Daniels', 1) "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        ASSERT_TRUE(i < expected.size()) << slice.toJson();
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }
      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryNGramMatchView : public QueryNGramMatch {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView(bool system) {
    auto& sysVocBaseFeature = server.getFeature<SystemDatabaseFeature>();
    auto sysVocBasePtr = sysVocBaseFeature.use();
    auto& vocbase = system ? *sysVocBasePtr : _vocbase;
    // create view
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
      auto logicalView = vocbase.createView(createJson->slice(), false);
      ASSERT_FALSE(!logicalView);

      auto* view = logicalView.get();
      auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
      ASSERT_FALSE(!impl);

      auto viewDefinitionTemplate = system ? R"({ "links": {
        "testCollection0": {
          "analyzers": [ "::myngram", "identity" ],
          "includeAllFields": true,
          "version": $0,
          "trackListPositions": true }
      }})"
                                           : R"({ "links": {
        "testCollection0": {
          "analyzers": [ "myngram", "identity" ],
          "includeAllFields": true,
          "version": $0,
          "trackListPositions": true }
      }})";

      auto viewDefinition = absl::Substitute(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()));

      auto updateJson = VPackParser::fromJson(viewDefinition);

      EXPECT_TRUE(impl->properties(updateJson->slice(), true, true).ok());
      std::set<arangodb::DataSourceId> cids;
      impl->visitCollections(
          [&cids](arangodb::DataSourceId cid, arangodb::LogicalView::Indexes*) {
            cids.emplace(cid);
            return true;
          });
      EXPECT_EQ(1U, cids.size());
      EXPECT_TRUE((arangodb::tests::executeQuery(
                       vocbase,
                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                       "{ waitForSync: true } RETURN d")
                       .result.ok()));  // commit
    }
  }
};

class QueryNGramMatchSearch : public QueryNGramMatch {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }

  void createSearch(bool system, Analyzer analyzer) {
    auto& sysVocBaseFeature = server.getFeature<SystemDatabaseFeature>();
    auto sysVocBasePtr = sysVocBaseFeature.use();
    auto& vocbase = system ? *sysVocBasePtr : _vocbase;
    // create indexes
    bool created = false;
    auto createJson = VPackParser::fromJson(absl::Substitute(
        R"({ "name": "testIndex0", "type": "inverted",
               "version": $0,
               "analyzer": "$1",
               "trackListPositions": true,
               "includeAllFields": true })",
        version(), toString(analyzer)));
    auto collection = vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(collection);
    collection->createIndex(createJson->slice(), created).waitAndGet();
    ASSERT_TRUE(created);

    // add view
    createJson = velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"search-alias\" }");

    auto view = std::dynamic_pointer_cast<iresearch::Search>(
        vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);

    // add link to collection
    {
      auto const viewDefinition = R"({
      "indexes": [
        { "collection": "testCollection0", "index": "testIndex0"}
      ]})";
      auto updateJson = velocypack::Parser::fromJson(viewDefinition);
      auto r = view->properties(updateJson->slice(), true, true);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
    EXPECT_TRUE(tests::executeQuery(vocbase,
                                    "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                    "{ waitForSync: true } RETURN d")
                    .result.ok());  // commit
  }
};

TEST_P(QueryNGramMatchView, TestSys) {
  create(true);
  createView(true);
  queryTestsSys(kAnalyzerIdentity | kAnalyzerMyNgram);
}

TEST_P(QueryNGramMatchView, Test) {
  create(false);
  createView(false);
  queryTests(kAnalyzerIdentity | kAnalyzerMyNgramUser);
}

TEST_P(QueryNGramMatchSearch, TestSys) {
  create(true);
  createSearch(true, kAnalyzerIdentity);
  queryTestsSys(kAnalyzerIdentity);
}

TEST_P(QueryNGramMatchSearch, TestSysNgram) {
  create(true);
  createSearch(true, kAnalyzerMyNgram);
  queryTestsSys(kAnalyzerMyNgram);
}

TEST_P(QueryNGramMatchSearch, Test) {
  create(false);
  createSearch(false, kAnalyzerIdentity);
  queryTests(kAnalyzerIdentity);
}

TEST_P(QueryNGramMatchSearch, TestNgram) {
  create(false);
  createSearch(false, kAnalyzerMyNgramUser);
  queryTests(kAnalyzerMyNgramUser);
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryNGramMatchView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryNGramMatchSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
