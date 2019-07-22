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

#include <map>

#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>

#include "Basics/StringBuffer.h"
#include "Cluster/MaintenanceRestHandler.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

// GeneralResponse only has a "protected" constructor.
class TestResponse : public arangodb::HttpResponse {
 public:
  TestResponse()
      : arangodb::HttpResponse(arangodb::rest::ResponseCode::OK, nullptr){};

};  // class TestResponse

// give access to some protected routines for more thorough unit tests
class TestHandler : public arangodb::MaintenanceRestHandler {
 public:
  TestHandler(arangodb::GeneralRequest* req, arangodb::GeneralResponse* res)
      : arangodb::MaintenanceRestHandler(req, res){};

  bool test_parsePutBody(VPackSlice const& parameters) {
    return parsePutBody(parameters);
  }

};  // class TestHandler

TEST(MaintenanceRestHandler, parse_rest_put) {
  VPackBuilder body;

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

  std::string json_str(body.toJson());

  std::unordered_map<std::string, std::string> x;
  arangodb::HttpRequest* dummyRequest =
      arangodb::HttpRequest::createHttpRequest(arangodb::rest::ContentType::JSON,
                                               json_str.c_str(), json_str.length(), x);
  dummyRequest->setRequestType(arangodb::rest::RequestType::PUT);
  TestResponse* dummyResponse = new TestResponse;
  TestHandler dummyHandler(dummyRequest, dummyResponse);

  ASSERT_TRUE(true == dummyHandler.test_parsePutBody(body.slice()));
  ASSERT_TRUE(dummyHandler.getActionDesc().has("name"));
  ASSERT_TRUE(dummyHandler.getActionDesc().get("name") == "CreateCollection");
  ASSERT_TRUE(dummyHandler.getActionDesc().has("collection"));
  ASSERT_TRUE(dummyHandler.getActionDesc().get("collection") == "a");
  ASSERT_TRUE(dummyHandler.getActionDesc().has("database"));
  ASSERT_TRUE(dummyHandler.getActionDesc().get("database") == "test");

  VPackObjectIterator it(dummyHandler.getActionProp().slice(), true);
  ASSERT_TRUE(it.key().copyString() == "journalSize");
  ASSERT_TRUE(it.value().getInt() == 1111);
}
