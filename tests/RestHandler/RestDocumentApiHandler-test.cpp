////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "IResearch/RestHandlerMock.h"
#include "Mocks/LogLevels.h"
#include "Mocks/PreparedResponseConnectionPool.h"
#include "Mocks/Servers.h"

#include "Basics/StaticStrings.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestServer/VocbaseContext.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;

template <typename T>
using Future = futures::Future<T>;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestDocumentAPITest : public ::testing::Test {
 protected:
  RestDocumentAPITest() {}

  ~RestDocumentAPITest() = default;

  void executeOnDBServer(std::function<void(arangodb::tests::mocks::MockDBServer& server)> const& toRun) {
    arangodb::tests::mocks::MockDBServer server{};
    toRun(server);
  }

  void executeOnCoordinator(
      std::function<void(arangodb::tests::mocks::MockCoordinator& server)> const& toRun) {
    arangodb::tests::mocks::MockCoordinator server{false};

    _dbEndpoint = server.registerFakedDBServer("PRMR_0001");

    toRun(server);
  }

 protected:
 std::pair<std::string, std::string> _dbEndpoint;
};

TEST_F(RestDocumentAPITest, test_roundtrip_api_document_read) {
  std::string shardName = "s10080";
  std::string collectionName = "_graphs";
  std::string keyName = "123";
  std::vector<arangodb::tests::PreparedRequestResponse> preparedResponses;
  executeOnDBServer([&](tests::mocks::MockDBServer& server) {
    auto& vocbase = server.getSystemDatabase();
    {
      // Setup block
      OperationOptions options(ExecContext::current());
      VPackBuilder properties;
      properties.openObject();
      properties.add(StaticStrings::DataSourcePlanId, VPackValue("123"));
      properties.close();
      std::shared_ptr<LogicalCollection> col{};
      methods::Collections::create(vocbase, options, "s10080", TRI_col_type_e::TRI_COL_TYPE_DOCUMENT,
                                   properties.slice(), false, false, false, col);
      ASSERT_TRUE(col);
      std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         EMPTY, EMPTY, EMPTY,
                                         arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());
      arangodb::OperationOptions opt;
      arangodb::ManagedDocumentResult mmdoc;
      auto jsonDocument = arangodb::velocypack::Parser::fromJson(
          R"({"_key": "123", "obj": {"a": "a_val", "b": "b_val"}})");
      auto const res = col->insert(&trx, jsonDocument->slice(), mmdoc, opt);
      EXPECT_TRUE(res.ok());
      auto commitRes = trx.commit();
      EXPECT_TRUE(commitRes.ok());
    }

    arangodb::tests::PreparedRequestResponse prep{vocbase};
    prep.setRequestType(arangodb::rest::RequestType::GET);
    prep.addSuffix(shardName);
    prep.addSuffix(keyName);

    // Run request, collect fake Response
    auto fakeResponse = std::make_unique<GeneralResponseMock>();
    auto fakeRequest = prep.generateRequest();

    arangodb::RestDocumentHandler testee(server.server(), fakeRequest.release(),
                                         fakeResponse.release());
    try {
      auto status = testee.execute();
      auto res = testee.stealResponse();
      LOG_DEVEL << "Status: " << (int)status;
      LOG_DEVEL << "Response "
                << static_cast<GeneralResponseMock*>(res.get())->_payload.toJson();
      prep.rememberResponse(std::move(res));
      preparedResponses.emplace_back(std::move(prep));
    } catch (...) {
      LOG_DEVEL << "Error thrown";
    }
  });
  executeOnCoordinator([&](tests::mocks::MockCoordinator& server) {
    ClusterInfo& ci = server.getFeature<ClusterFeature>().clusterInfo();
    ci.flush();
    auto& vocbase = server.getSystemDatabase();
    auto pool = server.getPool();
    static_cast<arangodb::tests::PreparedResponseConnectionPool*>(pool)->addPreparedResponses(_dbEndpoint, std::move(preparedResponses));

    std::vector<std::string> collections{collectionName};
    std::vector<std::string> noCollections{};
    transaction::Options opts;
    auto ctx = transaction::StandaloneContext::Create(vocbase);
    auto trx = std::make_shared<arangodb::transaction::Methods>(ctx, collections, noCollections,
                                                                noCollections, opts);
    OperationOptions options(ExecContext::superuser());
    options.ignoreRevs = true;
    VPackBuilder builder{};
    builder.openObject();
    builder.add(StaticStrings::KeyString, VPackValue(keyName));
    builder.close();
    auto res = trx->begin();
    ASSERT_TRUE(res.ok());
    // Run request, should get correct response
    // TODO need to inject DBServer responses
    Future<OperationResult> future =
        trx->documentAsync(collectionName, builder.slice(), options);
    try {
      OperationResult const& opRes = future.get();
      LOG_DEVEL << "Got Result: " << opRes.ok();
      if (opRes.hasSlice()) {
        LOG_DEVEL << opRes.slice().toJson();
      } else {
        LOG_DEVEL << "NONO slice";
      }

    } catch (std::exception const& e) {
      LOG_DEVEL << "Got Error: " << e.what();
    }
    res = trx->commit();
    ASSERT_TRUE(res.ok());
    LOG_DEVEL << "SHUTDOWN";
    /*

        auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
        auto fakeResponse = std::make_unique<GeneralResponseMock>();
        fakeRequest->setRequestType(arangodb::rest::RequestType::GET);
        fakeRequest->addSuffix(collectionName);
        fakeRequest->addSuffix(keyName);
        VocbaseContext* vc = static_cast<VocbaseContext*>(fakeRequest->requestContext());
        vc->forceSuperuser();

        arangodb::RestDocumentHandler testee(server.server(), fakeRequest.release(),
                                             fakeResponse.release());
        testee.runHandler(
            [](rest::RestHandler* handler) { LOG_DEVEL << "internal call!"; });

        auto status = testee.execute();
        auto res = testee.stealResponse();
        LOG_DEVEL << "Status: " << (int)status;
        LOG_DEVEL << "Response "
                  << static_cast<GeneralResponseMock*>(res.get())->_payload.toJson();
                  */
  });
}
