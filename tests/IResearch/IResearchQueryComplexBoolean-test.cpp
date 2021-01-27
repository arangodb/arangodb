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

class IResearchQueryComplexBooleanTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryComplexBooleanTest, test) {
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
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"nestListValues\": true, \"storeValues\":\"id\" },"
        "\"testCollection1\": { \"includeAllFields\": true, \"analyzers\": [ "
        "\"test_analyzer\", \"identity\" ], \"storeValues\":\"id\" }"
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

  // (A || B || C || !D)
  // (prefix || phrase || exists || !field)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[4].slice(),
        insertedDocs[5].slice(),  insertedDocs[10].slice(),
        insertedDocs[11].slice(), insertedDocs[12].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[20].slice(),
        insertedDocs[21].slice(), insertedDocs[23].slice(),
        insertedDocs[25].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[7].slice(),   // STARTS_WITH does not match, PHRASE matches
        insertedDocs[8].slice(),   // STARTS_WITH does not match, PHRASE matches
        insertedDocs[13].slice(),  // STARTS_WITH does not match, PHRASE matches
        insertedDocs[19].slice(),  // STARTS_WITH does not match, PHRASE matches
        insertedDocs[22].slice(),  // STARTS_WITH does not match, PHRASE matches
        insertedDocs[24].slice(),  // STARTS_WITH does not match, PHRASE matches
        insertedDocs[29].slice(),  // STARTS_WITH does not match, PHRASE matches
        insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term), PHRASE does not match
        insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term), PHRASE does not match
        insertedDocs[6].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match
        insertedDocs[9].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match
        insertedDocs[26].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match
        insertedDocs[31].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d.prefix, 'abc') || "
        "ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') || "
        "EXISTS(d.same) || d['value'] != 3.14 SORT BM25(d) ASC, TFIDF(d) DESC, "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // (A && B && !C)
  // field && prefix && !exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[36].slice(),  // STARTS_WITH matches (duplicated term)
        insertedDocs[37].slice(),  // STARTS_WITH matches (duplicated term)
        insertedDocs[26].slice(),  // STARTS_WITH matches (unique term short)
        insertedDocs[31].slice(),  // STARTS_WITH matches (unique term long)
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' && STARTS_WITH(d['prefix'], "
        "'abc') && NOT EXISTS(d.value) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq "
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

  // (A && B) || (C && D)
  // (field && prefix) || (phrase && exists)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),   // PHRASE matches
        insertedDocs[8].slice(),   // PHRASE matches
        insertedDocs[13].slice(),  // PHRASE matches
        insertedDocs[19].slice(),  // PHRASE matches
        insertedDocs[22].slice(),  // PHRASE matches
        insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term)
        insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term)
        insertedDocs[6].slice(),   // STARTS_WITH matches (unique term 4char)
        insertedDocs[9].slice(),   // STARTS_WITH matches (unique term 5char)
        insertedDocs[26].slice(),  // STARTS_WITH matches (unique term 3char)
        insertedDocs[31].slice(),  // STARTS_WITH matches (unique term 7char)
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH (d['same'] == 'xyz' && STARTS_WITH(d.prefix, "
        "'abc')) || (ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') "
        "&& EXISTS(d.value)) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // (A && B) || (C && D)
  // (field && prefix) || (phrase && exists)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),   // PHRASE matches
        insertedDocs[8].slice(),   // PHRASE matches
        insertedDocs[13].slice(),  // PHRASE matches
        insertedDocs[19].slice(),  // PHRASE matches
        insertedDocs[22].slice(),  // PHRASE matches
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH (d['same'] == 'xyz' && STARTS_WITH(d.prefix, "
        "'abc')) || (ANALYZER(PHRASE(d['duplicated'], 'z'), 'test_analyzer') "
        "&& EXISTS(d.value)) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq limit 5 "
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

  // (A || B) && (C || D || E)
  // (field || exists) && (starts_with || phrase || range)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[3].slice(),  insertedDocs[4].slice(),
        insertedDocs[5].slice(),  insertedDocs[10].slice(),
        insertedDocs[11].slice(), insertedDocs[12].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[20].slice(),
        insertedDocs[21].slice(), insertedDocs[23].slice(),
        insertedDocs[25].slice(), insertedDocs[27].slice(),
        insertedDocs[28].slice(), insertedDocs[30].slice(),
        insertedDocs[32].slice(), insertedDocs[33].slice(),
        insertedDocs[34].slice(), insertedDocs[35].slice(),
        insertedDocs[24].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS does not match
        insertedDocs[29].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS does not match
        insertedDocs[7].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS matches
        insertedDocs[8].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS matches
        insertedDocs[13].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS matches
        insertedDocs[19].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS matches
        insertedDocs[22].slice(),  // STARTS_WITH does not match, PHRASE matches, EXISTS matches
        insertedDocs[36].slice(),  // STARTS_WITH matches (duplicate term), PHRASE does not match, EXISTS does not match
        insertedDocs[37].slice(),  // STARTS_WITH matches (duplicate term), PHRASE does not match, EXISTS does not match
        insertedDocs[26].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match, EXISTS does not match
        insertedDocs[31].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match, EXISTS does not match
        insertedDocs[6].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match, EXISTS matches
        insertedDocs[9].slice(),  // STARTS_WITH matches (unique term), PHRASE does not match, EXISTS matches
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH (d.same == 'xyz' || EXISTS(d['value'])) && "
        "(STARTS_WITH(d.prefix, 'abc') || ANALYZER(PHRASE(d['duplicated'], "
        "'z'), 'test_analyzer') || d.seq >= -3) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
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
