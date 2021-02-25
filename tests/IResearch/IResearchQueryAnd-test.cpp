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

class IResearchQueryAndTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchQueryAndTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        VPackParser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        VPackParser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        VPackParser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        VPackParser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        VPackParser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        VPackParser::fromJson(
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
    auto createJson = VPackParser::fromJson(
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
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true, \"storeValues\":\"id\" },"
        "\"testCollection1\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"storeValues\":\"id\" }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE(
        (TRI_ERROR_NO_ERROR ==
         arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 == 1 "
                                       "OPTIONS { waitForSync: true } RETURN d")
             .result.errorNumber()));  // commit
  }

  // field and missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.invalid == 2 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // two different fields
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['same'] == 'xyz' AND d.value == 100 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // not field and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[10].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT (d['same'] == 'abc') AND d.value == 100 "
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

  // field and phrase
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[13].slice(), insertedDocs[19].slice(),
        insertedDocs[22].slice(), insertedDocs[24].slice(),
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND "
        "ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') SORT BM25(d) "
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

  // not phrase and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[23].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[31].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[34].slice(),
        insertedDocs[35].slice(), insertedDocs[36].slice(),
        insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // not phrase and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[23].slice(), insertedDocs[25].slice(),
        insertedDocs[26].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[31].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[34].slice(),
        insertedDocs[35].slice(), insertedDocs[36].slice(),
        insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(NOT PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, "
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

  // field and prefix
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[36].slice(), insertedDocs[37].slice(),
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[26].slice(), insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND STARTS_WITH(d['prefix'], "
        "'abc') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // not prefix and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[27].slice(), insertedDocs[28].slice(),
        insertedDocs[29].slice(), insertedDocs[30].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT STARTS_WITH(d['prefix'], 'abc') AND "
        "d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[9].slice(),
        insertedDocs[14].slice(), insertedDocs[21].slice(),
        insertedDocs[26].slice(), insertedDocs[29].slice(),
        insertedDocs[31].slice(), insertedDocs[34].slice(),
        insertedDocs[36].slice(), insertedDocs[37].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' AND EXISTS(d['prefix']) SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[19].slice(), insertedDocs[20].slice(),
        insertedDocs[22].slice(), insertedDocs[23].slice(),
        insertedDocs[24].slice(), insertedDocs[25].slice(),
        insertedDocs[27].slice(), insertedDocs[28].slice(),
        insertedDocs[30].slice(), insertedDocs[32].slice(),
        insertedDocs[33].slice(), insertedDocs[35].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH NOT EXISTS(d['prefix']) AND d.same == 'xyz' "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;
    ASSERT_EQ(expected.size(), slice.length());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // phrase and not field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d['duplicated'], 'z'), "
        "'test_analyzer') AND NOT (d.same == 'abc') AND EXISTS(d['prefix']) "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;
    ASSERT_EQ(expected.size(), slice.length());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // prefix and not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[37].slice(),
        insertedDocs[9].slice(),
        insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') AND NOT "
        "EXISTS(d.duplicated) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;
    ASSERT_EQ(expected.size(), slice.length());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // prefix and not exists and field with limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {insertedDocs[37].slice(),
                                                         insertedDocs[9].slice()};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') AND NOT "
        "EXISTS(d.duplicated) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq LIMIT 2 RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // exists and not prefix and phrase and not field and range
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH EXISTS(d.name) AND NOT "
        "STARTS_WITH(d['prefix'], 'abc') AND ANALYZER(PHRASE(d['duplicated'], "
        "'z'), 'test_analyzer') AND NOT (d.same == 'abc') AND d.seq >= 23 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // exists and not prefix and phrase and not field and range
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH EXISTS(d.name) AND NOT "
        "STARTS_WITH(d['prefix'], 'abc') AND ANALYZER(PHRASE(d['duplicated'], "
        "'z'), 'test_analyzer') AND NOT (d.same == 'abc') AND d.seq >= 23 SORT "
        "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
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
