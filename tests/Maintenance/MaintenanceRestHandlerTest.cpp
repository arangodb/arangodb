////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for MaintenanceRestHandler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/MaintenanceRestHandler.h"
#include "Endpoint/ConnectionInfo.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

// give access to some protected routines for more thorough unit tests
class TestHandler : public arangodb::MaintenanceRestHandler {
 public:
  TestHandler(arangodb::application_features::ApplicationServer& server,
              arangodb::GeneralRequest* req, arangodb::GeneralResponse* res)
      : arangodb::MaintenanceRestHandler(server, req, res){};

  bool test_parsePutBody(VPackSlice const& parameters) {
    return parsePutBody(parameters);
  }

};  // class TestHandler

TEST(MaintenanceRestHandler, parse_rest_put) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder body(buffer);

  // intentionally building this in non-alphabetic order, and name not first
  //  {"name":"CreateCollection","collection":"a","database":"test","properties":{"journalSize":1111}}
  {
    VPackObjectBuilder b(&body);
    body.add("database", VPackValue("test"));
    body.add("name", VPackValue("CreateCollection"));
    body.add(VPackValue("properties"));
    {
      VPackObjectBuilder bb(&body);
      body.add("journalSize", VPackValue(1111));
    }
    body.add("collection", VPackValue("a"));
  }

  auto* dummyRequest = new arangodb::HttpRequest(arangodb::ConnectionInfo(), 1, false);
  dummyRequest->setDefaultContentType(); // JSON
  dummyRequest->setPayload(buffer);
  dummyRequest->setRequestType(arangodb::rest::RequestType::PUT);

  auto* dummyResponse = new arangodb::HttpResponse(arangodb::rest::ResponseCode::OK, 1, nullptr);
  arangodb::application_features::ApplicationServer dummyServer{nullptr, nullptr};
  TestHandler dummyHandler(dummyServer, dummyRequest, dummyResponse);
  
  ASSERT_TRUE(dummyHandler.test_parsePutBody(body.slice()));
  ASSERT_TRUE(dummyHandler.getActionDesc().has("name"));
  ASSERT_EQ(dummyHandler.getActionDesc().get("name"), "CreateCollection");
  ASSERT_TRUE(dummyHandler.getActionDesc().has("collection"));
  ASSERT_EQ(dummyHandler.getActionDesc().get("collection"), "a");
  ASSERT_TRUE(dummyHandler.getActionDesc().has("database"));
  ASSERT_EQ(dummyHandler.getActionDesc().get("database"), "test");

  VPackObjectIterator it(dummyHandler.getActionProp().slice(), true);
  ASSERT_EQ(it.key().copyString(), "journalSize");
  ASSERT_EQ(it.value().getInt(), 1111);
}
