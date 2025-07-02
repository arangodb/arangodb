////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////
///
#include "gtest/gtest.h"

#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

#include <Aql/QueryRegistry.h>
#include "Basics/VelocyPackHelper.h"
#include "Mocks/Servers.h"
#include "IResearch/RestHandlerMock.h"
#include "RestHandler/RestSchemaHandler.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/Methods.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"

#include <IResearch/common.h>

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::tests::mocks;
using namespace arangodb::aql;
using namespace arangodb::velocypack;

class RestSchemaHandlerTest : public ::testing::Test {
public:
  void SetUp() override {
    server = std::make_unique<MockRestAqlServer>();
    registry = QueryRegistryFeature::registry();
    vocbase = &server->getSystemDatabase();

    auto& vocbase = server->getSystemDatabase(); // "_system"
    std::shared_ptr<Builder> collectionJson;
    collectionJson = Parser::fromJson(R"({ "name": "testCustomers" })");
    vocbase.createCollection(collectionJson->slice());
    collectionJson = Parser::fromJson(R"({ "name": "testProducts" })");
    vocbase.createCollection(collectionJson->slice());
    collectionJson = Parser::fromJson(R"({ "name": "testEmpty" })");
    vocbase.createCollection(collectionJson->slice());

    auto customerQuery = R"(
      LET customers = [
        {name: "C1", age: 25, address: "San Francisco", isStudent: true},
        {name: "C2", age: "young", address: "Tokyo", isStudent: false},
        {name: "C3", address: {city: "San Francisco", country: "USA"}},
        {name: "C4", age: 35, address: "Cologne"}
      ]
      FOR c IN customers INSERT c INTO testCustomers
    )";
    tests::executeQuery(vocbase, customerQuery);

    auto productQuery = R"(
      LET products = [
        {_key: "P1", name: "P1", price: 499.98},
        {_key: "P2",name: "P2", price: 1299.98, version: 14.5},
        {_key: "P3",name: "P3", price: "expensive", color: "black"},
        {_key: "P4",name: "P4", price: 349, version: "5.5"}
      ]
      FOR p IN products INSERT p INTO testProducts
    )";
    tests::executeQuery(vocbase, productQuery);
  }
  void TearDown() override {
    server.reset();
  }

protected:
  static inline std::unique_ptr<MockRestAqlServer> server;
  static inline QueryRegistry* registry;
  static inline TRI_vocbase_t* vocbase;
};

TEST_F(RestSchemaHandlerTest, WrongHttpRequest) {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(RequestType::POST);

  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);
  testee->execute();

  EXPECT_EQ(testee->response()->responseCode(), ResponseCode::METHOD_NOT_ALLOWED);
}

TEST_F(RestSchemaHandlerTest, NotExistingCollectionReturns404) {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  fakeRequest->setRequestType(RequestType::GET);
  fakeRequest->addSuffix("collection");
  fakeRequest->addSuffix("notExistingCol");

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  EXPECT_EQ(testee->response()->responseCode(), ResponseCode::NOT_FOUND);
}

TEST_F(RestSchemaHandlerTest, TooManySuffixesReturns404) {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  fakeRequest->setRequestType(RequestType::GET);
  fakeRequest->addSuffix("collection");
  fakeRequest->addSuffix("testProducts");
  fakeRequest->addSuffix("extraCol");

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  EXPECT_EQ(testee->response()->responseCode(), ResponseCode::NOT_FOUND);
}

TEST_F(RestSchemaHandlerTest, CollectionProductReturnsOK) {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  fakeRequest->setRequestType(RequestType::GET);
  fakeRequest->addSuffix("collection");
  fakeRequest->addSuffix("testProducts");

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(
      dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  auto actualSlice = fakeResponse->_payload.slice();

  EXPECT_EQ(actualSlice.get("collectionName").copyString(), "testProducts");
  EXPECT_EQ(actualSlice.get("collectionType").copyString(), "document");
  EXPECT_EQ(actualSlice.get("numOfDocuments").getNumber<uint64_t>(), 4);

  auto schemaSlice = actualSlice.get("schema");
  ASSERT_TRUE(schemaSlice.isArray());
  EXPECT_EQ(schemaSlice.length(), 6);

  std::vector<std::tuple<std::string, std::vector<std::string>, bool>> expectedSchema = {
    {"_id", {"string"}, false},
    {"_key", {"string"}, false},
    {"color", {"string"}, true},
    {"name", {"string"}, false},
    {"price", {"string", "number"}, false},
    {"version", {"number", "string"}, true},
  };

  for (size_t i = 0; i < expectedSchema.size(); ++i) {
    auto entry = schemaSlice.at(i);
    EXPECT_EQ(entry.get("attribute").copyString(), std::get<0>(expectedSchema[i]));
    EXPECT_EQ(entry.get("optional").getBool(), std::get<2>(expectedSchema[i]));

    auto types = entry.get("types");
    ASSERT_TRUE(types.isArray());

    std::set<std::string> actualTypes;
    for (auto const& t : VPackArrayIterator(types)) {
      actualTypes.insert(t.copyString());
    }

    std::set<std::string> expectedTypes(std::get<1>(expectedSchema[i]).begin(), std::get<1>(expectedSchema[i]).end());
    EXPECT_EQ(actualTypes, expectedTypes);
  }

  auto examples = actualSlice.get("examples");
  ASSERT_TRUE(examples.isArray());
  ASSERT_EQ(examples.length(), 1);

  auto example = examples.at(0);
  ASSERT_TRUE(example.isObject());
  EXPECT_TRUE(example.hasKey("_id"));
  EXPECT_TRUE(example.hasKey("_key"));
  EXPECT_TRUE(example.hasKey("price"));
  EXPECT_TRUE(example.hasKey("name"));
}