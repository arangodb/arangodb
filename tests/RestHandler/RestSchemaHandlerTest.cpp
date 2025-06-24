//
// Created by koichi on 6/17/25.
//
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
  static void SetUpTestCase() {
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
        {name: "Gilberto", age: 25, address: "San Francisco", isStudent: true},
        {name: "Victor", age: "young", address: "Tokyo", isStudent: false},
        {name: "Koichi", address: {city: "San Francisco", country: "USA"}},
        {name: "Michael", age: 35, address: "Cologne"}
      ]
      FOR c IN customers INSERT c INTO testCustomers
    )";
    tests::executeQuery(vocbase, customerQuery);

    auto productQuery = R"(
      LET products = [
        {name: "drone", price: 499.98},
        {name: "macBook", price: 1299.98, version: 14.5},
        {name: "glasses", price: "expensive", color: "black"},
        {name: "MS surface", price: 349, version: "5.5"}
      ]
      FOR p IN products INSERT p INTO testProducts
    )";
    tests::executeQuery(vocbase, productQuery);
  }
  static void TearDownTestCase() {
    server.reset();
    delete registry;
  }

protected:
  static inline std::unique_ptr<MockRestAqlServer> server;
  static inline QueryRegistry* registry;
  static inline TRI_vocbase_t* vocbase;
};

// namespace {
// arangodb::velocypack::SharedSlice operator"" _vpack(const char* json,
//                                                     size_t len) {
//   VPackOptions options;
//   options.checkAttributeUniqueness = true;
//   options.validateUtf8Strings = true;
//   VPackParser parser(&options);
//   parser.parse(json, len);
//   return parser.steal()->sharedSlice();
// }
// }

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
  fakeRequest->addSuffix("notExistingCol"); // _api/schema/testProducts

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
  fakeRequest->addSuffix("testProducts"); // _api/schema/testProducts
  fakeRequest->addSuffix("extraCol"); // _api/schema/testProducts

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  EXPECT_EQ(testee->response()->responseCode(), ResponseCode::NOT_FOUND);
}

TEST_F(RestSchemaHandlerTest, SampleNum1ReturnsAllOptionalFalse) {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  fakeRequest->setRequestType(RequestType::GET);
  fakeRequest->addSuffix("testCustomers");
  fakeRequest->setRequestPath("sampleNum=1");

  std::cout << fakeRequest->requestUrl() << std::endl;

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(
      dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));
  std::cout << fakeResponse->_payload.toString() << std::endl;

  EXPECT_EQ(fakeResponse->responseCode(), ResponseCode::OK);
}

TEST_F(RestSchemaHandlerTest, CollectionProductReturnsOK) {
  //auto& vocbase = server->getSystemDatabase(); // "_system"

  auto fakeRequest = std::make_unique<GeneralRequestMock>(*vocbase);
  fakeRequest->setRequestType(RequestType::GET);
  fakeRequest->addSuffix("testProducts"); // _api/schema/testProducts

  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(),
      fakeResponse.release(), registry);

  testee->execute();

  fakeResponse.reset(
      dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  auto expected = Parser::fromJson(R"json(
    [
      {"attribute":"_id","types":["string"],"optional":false},
      {"attribute":"_key","types":["string"],"optional":false},
      {"attribute":"color","types":["string"],"optional":true},
      {"attribute":"name","types":["string"],"optional":false},
      {"attribute":"price","types":["string","number"],"optional":false},
      {"attribute":"version","types":["number","string"],"optional":true}
    ]
    )json");

  EXPECT_EQUAL_SLICES(fakeResponse->_payload.slice(), expected->slice());
}