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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "IResearch/RestHandlerMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Result.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;

static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class PhysicalCollectionTest
    : public ::testing::Test,
      arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::WARN> {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::reference_wrapper<arangodb::application_features::ApplicationFeature>> features;

  PhysicalCollectionTest() : engine(server), server(nullptr, nullptr) {
    // setup required application features
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>());  // required for VocbaseContext
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>());
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>());  
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>());  // required for TRI_vocbase_t

#if USE_ENTERPRISE
    features.emplace_back(server.addFeature<arangodb::LdapFeature>());
#endif

    for (auto& f : features) {
      f.get().prepare();
    }
  }

  ~PhysicalCollectionTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);

    for (auto& f : features) {
      f.get().unprepare();
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(PhysicalCollectionTest, test_new_object_for_insert) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));

  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"test\" }");
  auto collection = vocbase.createCollection(json->slice());

  auto physical = engine.createPhysicalCollection(*collection, json->slice());

  auto doc = arangodb::velocypack::Parser::fromJson(
      "{ \"doc1\":\"test1\", \"doc100\":\"test2\", \"doc2\":\"test3\", "
      "\"z\":1, \"b\":2, \"a\":3, \"Z\":1, \"B\":2, \"A\": 3, \"_foo\":1, "
      "\"_bar\":2, \"_zoo\":3 }");

  arangodb::RevisionId revisionId = arangodb::RevisionId::none();
  arangodb::velocypack::Builder builder;
  Result res = physical->newObjectForInsert(nullptr, doc->slice(), false,
                                            builder, false, revisionId);
  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(revisionId.isSet());

  auto slice = builder.slice();

  EXPECT_TRUE(slice.hasKey("_key"));
  EXPECT_TRUE(slice.get("_key").isString());
  EXPECT_TRUE(slice.hasKey("_id"));
  EXPECT_TRUE(slice.get("_id").isCustom());
  EXPECT_TRUE(slice.hasKey("_rev"));
  EXPECT_TRUE(slice.get("_rev").isString());

  EXPECT_TRUE(slice.get("doc1").isString());
  EXPECT_EQ("test1", slice.get("doc1").copyString());
  EXPECT_TRUE(slice.get("doc100").isString());
  EXPECT_EQ("test2", slice.get("doc100").copyString());
  EXPECT_TRUE(slice.get("doc2").isString());
  EXPECT_EQ("test3", slice.get("doc2").copyString());

  EXPECT_TRUE(slice.hasKey("z"));
  EXPECT_TRUE(slice.get("z").isNumber());
  EXPECT_EQ(1, slice.get("z").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("b"));
  EXPECT_TRUE(slice.get("b").isNumber());
  EXPECT_EQ(2, slice.get("b").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("a"));
  EXPECT_TRUE(slice.get("a").isNumber());
  EXPECT_EQ(3, slice.get("a").getNumber<int>());

  EXPECT_TRUE(slice.hasKey("Z"));
  EXPECT_TRUE(slice.get("Z").isNumber());
  EXPECT_EQ(1, slice.get("Z").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("B"));
  EXPECT_TRUE(slice.get("B").isNumber());
  EXPECT_EQ(2, slice.get("B").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("A"));
  EXPECT_TRUE(slice.get("A").isNumber());
  EXPECT_EQ(3, slice.get("A").getNumber<int>());

  EXPECT_TRUE(slice.hasKey("_foo"));
  EXPECT_TRUE(slice.get("_foo").isNumber());
  EXPECT_EQ(1, slice.get("_foo").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("_bar"));
  EXPECT_TRUE(slice.get("_bar").isNumber());
  EXPECT_EQ(2, slice.get("_bar").getNumber<int>());
  EXPECT_TRUE(slice.hasKey("_zoo"));
  EXPECT_TRUE(slice.get("_zoo").isNumber());
  EXPECT_EQ(3, slice.get("_zoo").getNumber<int>());

  EXPECT_TRUE(slice.isObject());
  EXPECT_EQ(slice.head(), 0xbU);

  // iterate over the data in the order that is stored in the builder
  {
    velocypack::ObjectIterator it(slice, true);
    std::vector<std::string> expected{"_key", "_id", "_rev", "doc1", "doc100",
                                      "doc2", "z",   "b",    "a",    "Z",
                                      "B",    "A",   "_foo", "_bar", "_zoo"};

    for (auto const& key : expected) {
      EXPECT_TRUE(it.valid());
      EXPECT_EQ(key, it.key().copyString());
      it.next();
    }
  }

  // iterate over the data in the order that is stored in the index table
  {
    velocypack::ObjectIterator it(slice, false);
    std::vector<std::string> expected{"A",   "B",    "Z",      "_bar", "_foo",
                                      "_id", "_key", "_rev",   "_zoo", "a",
                                      "b",   "doc1", "doc100", "doc2", "z"};

    for (auto const& key : expected) {
      EXPECT_TRUE(it.valid());
      EXPECT_EQ(key, it.key().copyString());
      it.next();
    }
  }
}

class MockIndex : public Index {
 public:
  MockIndex(Index::IndexType type, bool needsReversal, arangodb::IndexId id,
            LogicalCollection& collection, const std::string& name,
            std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
            bool unique, bool sparse)
      : Index(id, collection, name, fields, unique, sparse),
        _type(type),
        _needsReversal(needsReversal) {}

  bool needsReversal() const override { return _needsReversal; }
  IndexType type() const override { return _type; }
  char const* typeName() const override { return "IndexMock"; }
  bool canBeDropped() const override { return true; }
  bool isSorted() const override { return false; }
  bool isHidden() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; }
  size_t memory() const override { return 0; }
  void load() override {}
  void unload() override {}

 private:
  Index::IndexType _type;
  bool _needsReversal;
};

TEST_F(PhysicalCollectionTest, test_index_ordeing) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"test\" }");
  auto collection = vocbase.createCollection(json->slice());
  std::vector<std::vector<arangodb::basics::AttributeName>> dummyFields;
  PhysicalCollection::IndexContainerType test_container;
  // also regular index but no need to be reversed
  test_container.insert(std::make_shared<MockIndex>(Index::TRI_IDX_TYPE_HASH_INDEX,
                                                    false, arangodb::IndexId{2}, *collection,
                                                    "4", dummyFields, false, false));
  // Edge index- should go right after primary and after all other non-reversable edge indexes
  test_container.insert(std::make_shared<MockIndex>(Index::TRI_IDX_TYPE_EDGE_INDEX,
                                                    true, arangodb::IndexId{3}, *collection,
                                                    "3", dummyFields, false, false));
  // Edge index- non-reversable should go right after primary
  test_container.insert(std::make_shared<MockIndex>(Index::TRI_IDX_TYPE_EDGE_INDEX,
                                                    false, arangodb::IndexId{4}, *collection,
                                                    "2", dummyFields, false, false));
  // Primary index. Should be first!
  test_container.insert(std::make_shared<MockIndex>(Index::TRI_IDX_TYPE_PRIMARY_INDEX,
                                                    true, arangodb::IndexId{5}, *collection,
                                                    "1", dummyFields, true, false));
  // should execute last - regular index with reversal possible
  test_container.insert(std::make_shared<MockIndex>(Index::TRI_IDX_TYPE_HASH_INDEX,
                                                    true, arangodb::IndexId{1}, *collection,
                                                    "5", dummyFields, false, false));

  arangodb::IndexId prevId{5};
  for (auto idx : test_container) {
    ASSERT_EQ(prevId, idx->id());
    prevId = arangodb::IndexId{prevId.id() - 1};
  }
}
