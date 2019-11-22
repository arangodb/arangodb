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

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryTraversalTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchQueryTraversalTest, test) {
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
            "{ \"_id\": \"testCollection0/0\", \"_key\": \"0\", \"seq\": -6, "
            "\"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/1\", \"_key\": \"1\", \"seq\": -5, "
            "\"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/2\", \"_key\": \"2\", \"seq\": -4, "
            "\"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/3\", \"_key\": \"3\", \"seq\": -3, "
            "\"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/4\", \"_key\": \"4\", \"seq\": -2, "
            "\"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/5\", \"_key\": \"5\", \"seq\": -1, "
            "\"value\": { \"a\": 7, \"b\": \"c\" } }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_id\": \"testCollection0/6\", \"_key\": \"6\", \"seq\": 0, "
            "\"value\": { \"a\": 7, \"b\": \"c\" } }"),
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

  // create edge collection
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"edges\", \"type\": 3 }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    auto createIndexJson =
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"edge\" }");
    bool created = false;
    auto index = collection->createIndex(createIndexJson->slice(), created);
    EXPECT_TRUE(index);
    EXPECT_TRUE(created);

    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/0\", \"_to\": "
            "\"testCollection0/1\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/0\", \"_to\": "
            "\"testCollection0/2\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/0\", \"_to\": "
            "\"testCollection0/3\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/0\", \"_to\": "
            "\"testCollection0/4\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/0\", \"_to\": "
            "\"testCollection0/5\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"_from\": \"testCollection0/6\", \"_to\": "
            "\"testCollection0/0\" }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;

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
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
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

  // create view on edge collection
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testViewEdge\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"edges\": { \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testViewEdge SEARCH 1 ==1 "
                                       "OPTIONS { waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // check system attribute _from
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{insertedDocs.back().slice()};

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testViewEdge SEARCH d._from == 'testCollection0/6' RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator resultIt(slice);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
      ++expectedDoc;
    }
    EXPECT_FALSE(resultIt.valid());
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // check system attribute _to
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{insertedDocs.back().slice()};

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testViewEdge SEARCH d._to == 'testCollection0/0' RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator resultIt(slice);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
      ++expectedDoc;
    }
    EXPECT_FALSE(resultIt.valid());
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // shortest path traversal
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        insertedDocs[6].slice(),
        insertedDocs[7].slice(),
        insertedDocs[5].slice(),
        insertedDocs[0].slice(),
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR v, e IN OUTBOUND SHORTEST_PATH 'testCollection0/6' TO "
        "'testCollection0/5' edges FOR d IN testView SEARCH d.seq == v.seq "
        "SORT TFIDF(d) DESC, d.seq DESC, d._id RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator resultIt(slice);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
      ++expectedDoc;
    }
    EXPECT_FALSE(resultIt.valid());
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // simple traversal
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        insertedDocs[5].slice(), insertedDocs[4].slice(),
        insertedDocs[3].slice(), insertedDocs[2].slice(),
        insertedDocs[1].slice(),
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR v, e, p IN 1..2 OUTBOUND 'testCollection0/0' edges FOR d IN "
        "testView SEARCH d.seq == v.seq SORT TFIDF(d) DESC, d.seq DESC RETURN "
        "v");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator resultIt(slice);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
      ++expectedDoc;
    }
    EXPECT_FALSE(resultIt.valid());
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }
}
