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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "IResearch/RestHandlerMock.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Basics/StaticStrings.h"
#include "RestHandler/RestDocumentHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestDocumentHandlerTestBase
    : public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::WARN> {
 protected:
  arangodb::tests::mocks::MockRestServer server;

  RestDocumentHandlerTestBase() : server{} {}

  ~RestDocumentHandlerTestBase() = default;
};

class RestDocumentHandlerTest : public RestDocumentHandlerTestBase, public ::testing::Test {
 protected:
  RestDocumentHandlerTest() : RestDocumentHandlerTestBase{} {}

  ~RestDocumentHandlerTest() = default;
};

class RestDocumentHandlerLaneTest
    : public RestDocumentHandlerTestBase,
      public ::testing::TestWithParam<arangodb::rest::RequestType> {
 protected:
  RestDocumentHandlerLaneTest()
      : RestDocumentHandlerTestBase{}, _type(GetParam()) {}

  ~RestDocumentHandlerLaneTest() = default;

  arangodb::rest::RequestType _type;
};

TEST_P(RestDocumentHandlerLaneTest, test_request_lane_user) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(_type);

  arangodb::RestDocumentHandler testee(server.server(), fakeRequest.release(),
                                       fakeResponse.release());
  ASSERT_EQ(arangodb::RequestLane::CLIENT_SLOW, testee.lane());
}

TEST_P(RestDocumentHandlerLaneTest, test_request_lane_replication) {
  auto& vocbase = server.getSystemDatabase();
  auto fakeRequest = std::make_unique<GeneralRequestMock>(vocbase);
  auto fakeResponse = std::make_unique<GeneralResponseMock>();
  fakeRequest->setRequestType(_type);
  fakeRequest->values().emplace(arangodb::StaticStrings::IsSynchronousReplicationString,
                                "abc");

  arangodb::RestDocumentHandler testee(server.server(), fakeRequest.release(),
                                       fakeResponse.release());
  ASSERT_EQ(arangodb::RequestLane::SERVER_SYNCHRONOUS_REPLICATION, testee.lane());
}

INSTANTIATE_TEST_CASE_P(RequestTypeVariations, RestDocumentHandlerLaneTest,
                        ::testing::Values(arangodb::rest::RequestType::GET,
                                          arangodb::rest::RequestType::PUT,
                                          arangodb::rest::RequestType::POST,
                                          arangodb::rest::RequestType::DELETE_REQ,
                                          arangodb::rest::RequestType::PATCH));
