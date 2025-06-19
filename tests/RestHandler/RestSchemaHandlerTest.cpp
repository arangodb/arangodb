//
// Created by koichi on 6/17/25.
//
#include "gtest/gtest.h"

#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

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

#include <IResearch/common.h>

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::tests::mocks;
using namespace arangodb::aql;
using namespace arangodb::velocypack;

class RestSchemaHandlerTest : public ::testing::Test {
protected:
  MockAqlServer server;
  QueryRegistry* _queryRegistry;

  void SetUp() override {
    auto& qrFeature = server.server().getFeature<arangodb::QueryRegistryFeature>();
    _queryRegistry = qrFeature.queryRegistry();
  }

  std::unique_ptr<RestSchemaHandler> makeHandler(
      GeneralRequest* req, GeneralResponse* res) {
    return std::make_unique<RestSchemaHandler>(
      server.server(), req, res, _queryRegistry
    );
  }
};

TEST_F(RestSchemaHandlerTest, WrongMethodReturns405) {
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto req = std::make_unique<GeneralRequestMock>(vocbase);
  req->setRequestType(RequestType::POST);
  req->addSuffix("test");
  auto res = std::make_unique<GeneralResponseMock>();

  auto handler = makeHandler(req.get(), res.get());
  EXPECT_EQ(handler->execute(), RestStatus::DONE);
  EXPECT_EQ(res->responseCode(), ResponseCode::METHOD_NOT_ALLOWED);
}
