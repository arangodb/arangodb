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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"

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

class IResearchQueryNGramMatchTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryNGramMatchTest, SysVocbase) {
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  auto& sysVocBaseFeature = server.getFeature<arangodb::SystemDatabaseFeature>();

  auto sysVocBasePtr = sysVocBaseFeature.use();
  auto& vocbase = *sysVocBasePtr;

  // 2-gram analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto res =
      analyzers.emplace(result, "_system::myngram", "ngram",
        VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", \"preserveOriginal\":false}")->slice(),
        irs::flags{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() }  // required for PHRASE
    );  // cache analyzer
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
        "\"testCollection0\": { \"analyzers\": [ \"::myngram\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true }}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
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
        "FOR d IN testView SEARCH NGRAM_MATCH(d.missing, 'abc', 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
        "FOR d IN testView SEARCH NGRAM_MATCH(d['missing'], 'abc', 0.5, 'myngram') SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
        "FOR d IN testView SEARCH NGRAM_MATCH(d.seq, '0', 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
        "FOR d IN testView SEARCH NGRAM_MATCH(d['seq'], '0', 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value, [ 1, \"abc\" ], 0.5, 'myngram') SORT BM25(d) "
        "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], [ 1, \"abc\" ], 0.5, 'myngram') SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value, true, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], false, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value, null, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], null, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 3.14, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 1234, 0.5, 'myngram') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value, { \"a\": 7, \"b\": \"c\" }, 0.5, 'myngram') "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], { \"a\": 7, \"b\": \"c\" "
        "}, 0.5, 'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test too much args
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 'test', 0.5, 'analyzer', 'too much') SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
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
        "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, { \"a\": "
        "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
        "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // can't access to local analyzer in other database
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
        "'testVocbase::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
        "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test via ANALYZER function
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(NGRAM_mATCH(d.value, 'Jack Daniels', 0.7), "
      "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }


  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }


  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[1].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),
        insertedDocs[4].slice(),
        insertedDocs[5].slice(),
        insertedDocs[1].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // test via default analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      // no results  will be found as identity analyzer has no positions        
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
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

}

TEST_F(IResearchQueryNGramMatchTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // 2-gram analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto res =
      analyzers.emplace(result, "testVocbase::myngram", "ngram",
        VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", \"preserveOriginal\":false}")->slice(),
        irs::flags{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() }  // required for PHRASE
    );  // cache analyzer
    EXPECT_TRUE(res.ok());
  }
  {
    // 2-gram analyzer in another DB
    {

      auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

      TRI_vocbase_t* vocbase2;
      auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
      dbFeature.createDatabase(testDBInfo(server.server(), "testVocbase2"), vocbase2); 
      std::shared_ptr<arangodb::LogicalCollection> unused;
      ASSERT_NE(nullptr, vocbase2);
      arangodb::OperationOptions options(arangodb::ExecContext::current());
      arangodb::methods::Collections::createSystem(*vocbase2, options,
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false, unused);

      auto res =
        analyzers.emplace(result, "testVocbase2::myngram", "ngram",
          VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", \"preserveOriginal\":false}")->slice(),
          irs::flags{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() }
      );  // cache analyzer
      EXPECT_TRUE(res.ok());
    }
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
      "\"testCollection0\": { \"analyzers\": [ \"myngram\", "
      "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
      "true }}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
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
      "FOR d IN testView SEARCH NGRAM_MATCH(d.missing, 'abc', 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
      "FOR d IN testView SEARCH NGRAM_MATCH(d['missing'], 'abc', 0.5, 'myngram') SORT BM25(d) "
      "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
      "FOR d IN testView SEARCH NGRAM_MATCH(d.seq, '0', 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
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
      "FOR d IN testView SEARCH NGRAM_MATCH(d['seq'], '0', 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // test invalid input type (array)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, [ 1, \"abc\" ], 0.5, 'myngram') SORT BM25(d) "
      "ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (array) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], [ 1, \"abc\" ], 0.5, 'myngram') SORT "
      "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, true, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], false, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, null, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (null) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], null, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 3.14, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], 1234, 0.5, 'myngram') SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, { \"a\": 7, \"b\": \"c\" }, 0.5, 'myngram') "
      "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (object) via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value'], { \"a\": 7, \"b\": \"c\" "
      "}, 0.5, 'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value) SORT BM25(d) ASC, TFIDF(d) "
      "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d['value']) SORT BM25(d) ASC, "
      "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test too much args
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.value, 'abs', 0.5, 'identity', 'too much') SORT BM25(d) ASC, TFIDF(d) "
      "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
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
      "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', 0.5, { \"a\": "
      "7, \"b\": \"c\" }) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test undefined analyzer
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
      "'invalid_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // can't access to local analyzer in other database
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH NGRAM_MATCH(d.duplicated, 'z', "
      "'testVocbase2::test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
      "RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test via ANALYZER function
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(NGRAM_mATCH(d.value, 'Jack Daniels', 0.7), "
      "'myngram') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }


  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // test via analyzer parameter with default threshold
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }


  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[1].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // test via analyzer parameter
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),
        insertedDocs[4].slice(),
        insertedDocs[5].slice(),
        insertedDocs[1].slice()
    };
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
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

  // test via default analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      // no results  will be found as identity analyzer has no positions        
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
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }

}
