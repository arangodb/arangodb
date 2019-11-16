////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
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
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
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
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
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
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
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

  // test invalid input type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
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

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
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
        "FOR d IN testView SEARCH PHRASE(d['missing'], 'abc') SORT BM25(d) "
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
        "FOR d IN testView SEARCH PHRASE(d.seq, '0') SORT BM25(d) ASC, "
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
        "FOR d IN testView SEARCH PHRASE(d['seq'], '0') SORT BM25(d) ASC, "
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

  // test invalid input type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.value, [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid input type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['value'], [ ]) SORT BM25(d) ASC, "
        "TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
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

  // test custom analyzer with offsets (no match)
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 0, 'z'), "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, [ 'v', 0, 'z' "
        "]), 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test custom analyzer with offsets (no match) via [] with [ phrase ] arg
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d['duplicated'], [ 'v', 1, 'c' ], "
        "'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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
}
