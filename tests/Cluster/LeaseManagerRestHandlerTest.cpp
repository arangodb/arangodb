////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////


// NOTE: Unfortunately we cannot activate this test properly.
// For this test to work we need the following features to be completely started:
// - ClusterFeature
// - NetworkFeature
// - SchedulerFeature
// The mocks thus far do setup those features, but they do not start the SchedulerFeature.
// Therefore the RebootTracker get's a nullptr Scheduler and then the LeaseManager will fail
// as soon as it uses the RebootTracker.
// The test now is to be covered by integration tests in JS.

// Keeping this file though in case we can get the above mentioned features started properly.
#if 1
#include "gtest/gtest.h"

#include "IResearch/RestHandlerMock.h"
#include "Mocks/Servers.h"

#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/LeaseManager/LeaseManagerRestHandler.h"
#include "Network/NetworkFeature.h"


using namespace arangodb;
using namespace arangodb::cluster;

namespace {
auto emptyPrint() -> std::string { return "Dummy Details"; }
}  // namespace

class LeaseManagerRestHandlerTest : public ::testing::Test {

  struct LeaseResponse {
    VPackSlice leasedFromRemote;
    VPackSlice leasedToRemote;
  };

 protected:
  LeaseManagerRestHandlerTest() : server{"CRDN_0001"} {}


  tests::mocks::MockCoordinator server;

  static ServerID const serverA;
  static ServerID const serverB;
  static ServerID const serverC;

  containers::FlatHashMap<ServerID, ServerHealthState> state;

  void SetUp() override {
    state = containers::FlatHashMap<ServerID, ServerHealthState>{
        {serverA, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}},
        {serverB, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}},
        {serverC, ServerHealthState{.rebootId = RebootId{1},
                                    .status = ServerHealth::kGood}}};
    auto& cf = server.getFeature<ClusterFeature>();
    cf.clusterInfo().rebootTracker().updateServerState(state);
  }

  auto getLeaseManager() -> LeaseManager& {
    auto& nf = server.getFeature<NetworkFeature>();
    return nf.leaseManager();
  }

  auto extractResultBody(VPackSlice response) -> LeaseResponse {
    EXPECT_TRUE(response.isObject()) << "Did not respond with an object";

    EXPECT_EQ(basics::VelocyPackHelper::getNumericValue(response, "code", 42),
              static_cast<int>(rest::ResponseCode::OK));
    EXPECT_FALSE(response.hasKey("errorNum"));
    EXPECT_FALSE(response.hasKey("errorMessage"));
    EXPECT_TRUE(response.hasKey("error"));
    EXPECT_TRUE(response.get("error").isBool());
    EXPECT_FALSE(response.get("error").getBool());

    auto result = response.get("result");
    EXPECT_TRUE(result.isObject()) << "Did not respond with a result entry of type object";

    auto leasedFromRemote = result.get("leasedFromRemote");
    EXPECT_TRUE(leasedFromRemote.isObject()) << "Did not respond with a leasedFromRemote entry of type object";
    auto leasedToRemote = result.get("leasedToRemote");
    EXPECT_TRUE(leasedToRemote.isObject()) << "Did not respond with a leasedToRemote entry of type object";
    return {.leasedFromRemote = leasedFromRemote,
            .leasedToRemote = leasedToRemote};
  }

  auto assertResponseIsNotFound(VPackSlice response) {
    ASSERT_TRUE(response.isObject()) << "Did not respond with an object";
    EXPECT_EQ(basics::VelocyPackHelper::getNumericValue(response, "code", 42),
              TRI_ERROR_HTTP_NOT_FOUND.value());
    EXPECT_EQ(basics::VelocyPackHelper::getNumericValue(response, "errorNum", 42),
              TRI_ERROR_HTTP_NOT_FOUND.value());
    EXPECT_TRUE(response.hasKey("errorMessage"));
    EXPECT_TRUE(response.get("errorMessage").isString());
    EXPECT_TRUE(response.hasKey("error"));
    EXPECT_TRUE(response.get("error").isBool());
    EXPECT_TRUE(response.get("error").getBool());
  }

  auto getPeerState(ServerID const& peerName) -> PeerState {
    auto it = state.find(peerName);
    TRI_ASSERT(it != state.end())
        << "Test setup incorrect, tried to getPeerState for a server that does "
           "not participate in the test.";

    return PeerState{.serverId = peerName, .rebootId = it->second.rebootId};
  }
};

ServerID const LeaseManagerRestHandlerTest::serverA = "PRMR-srv-A";
ServerID const LeaseManagerRestHandlerTest::serverB = "PRMR-srv-B";
ServerID const LeaseManagerRestHandlerTest::serverC = "PRMR-srv-C";

TEST_F(LeaseManagerRestHandlerTest, test_get_request) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto resp = fakeResponse.get();
  fakeRequest->setRequestType(arangodb::rest::RequestType::GET);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto res = testee.execute();
  EXPECT_EQ(res, RestStatus::DONE);
  EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
  EXPECT_EQ(resp->responseCode(), ResponseCode::OK);
  auto response = resp->_payload.slice();
  auto result = extractResultBody(response);
  EXPECT_TRUE(result.leasedToRemote.isEmptyObject()) << "Leased to Remote is not empty";
  EXPECT_TRUE(result.leasedFromRemote.isEmptyObject()) << "Leased From Remote is not empty";
}

TEST_F(LeaseManagerRestHandlerTest, test_get_request_including_leases) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto resp = fakeResponse.get();
  fakeRequest->setRequestType(arangodb::rest::RequestType::GET);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto& leaseManager = getLeaseManager();
  auto leaseIsForA = getPeerState(serverA);
  auto leaseGuardA =
      leaseManager.requireLease(leaseIsForA, ::emptyPrint, []() noexcept {});

  auto leaseIsForB = getPeerState(serverA);
  auto leaseGuardB1 =
      leaseManager.requireLease(leaseIsForB, ::emptyPrint, []() noexcept {});
  auto leaseGuardB2 =
      leaseManager.requireLease(leaseIsForB, ::emptyPrint, []() noexcept {});
  {
    ScopeGuard freeAllTheLeases{[&]() noexcept {
      // Make sure we cancel the leases before they go
      // out of scope, so we avoid trying to inform remote
      // peers
      leaseGuardA.cancel();
      leaseGuardB1.cancel();
      leaseGuardB2.cancel();
    }};
    auto res = testee.execute();
    EXPECT_EQ(res, RestStatus::DONE);
    EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
    auto response = resp->_payload.slice();
    auto result = extractResultBody(response);
    LOG_DEVEL << "Leased to Remote: " << result.leasedToRemote.toJson();
    LOG_DEVEL << "Leased from Remote: " << result.leasedFromRemote.toJson();
    EXPECT_TRUE(result.leasedToRemote.isEmptyObject())
        << "Leased to Remote is not empty";
    EXPECT_TRUE(result.leasedFromRemote.isEmptyObject())
        << "Leased From Remote is not empty";
  }
}

TEST_F(LeaseManagerRestHandlerTest, test_get_request_including_leases_some_removed) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::GET);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto res = testee.execute();
  EXPECT_EQ(res, RestStatus::DONE);
  EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
  // TODO: Assert that we return a non empty body
}

TEST_F(LeaseManagerRestHandlerTest, test_put_request) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto resp = fakeResponse.get();
  fakeRequest->setRequestType(arangodb::rest::RequestType::PUT);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto res = testee.execute();
  EXPECT_EQ(res, RestStatus::DONE);
  EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
  EXPECT_EQ(resp->responseCode(), ResponseCode::NOT_FOUND);
  auto response = resp->_payload.slice();
  assertResponseIsNotFound(response);
}

TEST_F(LeaseManagerRestHandlerTest, test_post_request) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  auto resp = fakeResponse.get();
  fakeRequest->setRequestType(arangodb::rest::RequestType::POST);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto res = testee.execute();
  EXPECT_EQ(res, RestStatus::DONE);
  EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
  EXPECT_EQ(resp->responseCode(), ResponseCode::NOT_FOUND);
  auto response = resp->_payload.slice();
  assertResponseIsNotFound(response);
}

TEST_F(LeaseManagerRestHandlerTest, test_delete_request) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(arangodb::rest::RequestType::GET);

  LeaseManagerRestHandler testee(server.server(), fakeRequest.release(),
                                 fakeResponse.release());
  auto res = testee.execute();
  EXPECT_EQ(res, RestStatus::DONE);
  EXPECT_EQ(RequestLane::CLIENT_FAST, testee.lane());
  // TODO: Assert that we return 200
  // TODO: Assert that we return an empty body
  // TODO: Assert that leases are dest
}

/// TODO: Add authentication tests!

#endif