//
// Created by koichi on 6/17/25.
//
#include "gtest/gtest.h"

#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

#include <Aql/QueryRegistry.h>
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
  }
  static void TearDownTestCase() { server.reset(); }

protected:
  static inline std::unique_ptr<MockRestAqlServer> server;
};

namespace {
arangodb::velocypack::SharedSlice operator"" _vpack(const char* json,
                                                    size_t len) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  options.validateUtf8Strings = true;
  VPackParser parser(&options);
  parser.parse(json, len);
  return parser.steal()->sharedSlice();
}
}

TEST_F(RestSchemaHandlerTest, WrongMethodReturns405) {
  auto& vocbase = server->getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);
  fakeRequest->_payload.add(R"json(
    {
      "query": "FOR i IN 1..1000 RETURN CONCAT('', i)"
    }
  )json"_vpack);

  auto* registry = arangodb::QueryRegistryFeature::registry();

  auto testee = std::make_shared<RestSchemaHandler>(
      server->server(), fakeRequest.release(), fakeResponse.release(),
      registry);

  testee->execute();

  fakeResponse.reset(
      dynamic_cast<GeneralResponseMock*>(testee->stealResponse().release()));

  std::cout << "RestSchemaHandlerTest place 8" << std::endl;
  std::cout << fakeResponse->_payload.slice() << std::endl;
  EXPECT_EQ(fakeResponse->responseCode(),
            ResponseCode::METHOD_NOT_ALLOWED);
}
