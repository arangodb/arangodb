////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "../IResearch/RestHandlerMock.h"
#include "../Mocks/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Result.h"
#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class PhysicalCollectionTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<arangodb::application_features::ApplicationFeature*> features;

  PhysicalCollectionTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server));  // required for VocbaseContext
    features.emplace_back(new arangodb::DatabaseFeature(server));
    features.emplace_back(new arangodb::QueryRegistryFeature(server));  // required for TRI_vocbase_t

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server));
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f);
    }

    for (auto& f : features) {
      f->prepare();
    }
  }

  ~PhysicalCollectionTest() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    for (auto& f : features) {
      f->unprepare();
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(PhysicalCollectionTest, test_new_object_for_insert) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"test\" }");
  auto collection = vocbase.createCollection(json->slice());

  auto physical = engine.createPhysicalCollection(*collection, json->slice());

  auto doc = arangodb::velocypack::Parser::fromJson("{ \"doc1\":\"test1\", \"doc100\":\"test2\", \"doc2\":\"test3\", \"z\":1, \"b\":2, \"a\":3, \"Z\":1, \"B\":2, \"A\": 3, \"_foo\":1, \"_bar\":2, \"_zoo\":3 }");

  TRI_voc_rid_t revisionId = 0;
  arangodb::velocypack::Builder builder;
  Result res = physical->newObjectForInsert(nullptr, doc->slice(), false, builder, false, revisionId);
  EXPECT_TRUE(res.ok());
  EXPECT_TRUE(revisionId > 0);

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
  EXPECT_TRUE(slice.head() == 0xbU);

  // iterate over the data in the order that is stored in the builder
  {
    velocypack::ObjectIterator it(slice, true);
    std::vector<std::string> expected{ "_key", "_id", "_rev", "doc1", "doc100", "doc2", "z", "b", "a", "Z", "B", "A", "_foo", "_bar", "_zoo" };

    for (auto const& key : expected) {
      EXPECT_TRUE(it.valid());
      EXPECT_EQ(key, it.key().copyString());
      it.next();
    }
  }

  // iterate over the data in the order that is stored in the index table
  {
    velocypack::ObjectIterator it(slice, false);
    std::vector<std::string> expected{ "A", "B", "Z", "_bar", "_foo", "_id", "_key", "_rev", "_zoo", "a", "b", "doc1", "doc100", "doc2", "z" };

    for (auto const& key : expected) {
      EXPECT_TRUE(it.valid());
      EXPECT_EQ(key, it.key().copyString());
      it.next();
    }
    }
  }
