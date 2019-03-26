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

#include "catch.hpp"

#include <map>

#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringBuffer.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Cluster/MaintenanceRestHandler.h"

// GeneralResponse only has a "protected" constructor.
class TestResponse : public arangodb::HttpResponse {
public:
  TestResponse() : arangodb::HttpResponse(arangodb::rest::ResponseCode::OK, new arangodb::basics::StringBuffer(false)) {};

}; // class TestResponse


// give access to some protected routines for more thorough unit tests
class TestHandler : public arangodb::MaintenanceRestHandler {
public:
  TestHandler(arangodb::GeneralRequest* req, arangodb::GeneralResponse* res)
    : arangodb::MaintenanceRestHandler(req,res) {};

  bool test_parsePutBody(VPackSlice const & parameters) {
    return parsePutBody(parameters);
  }

}; // class TestHandler


TEST_CASE("MaintenanceRestHandler", "[cluster][maintenance][devel]") {

  SECTION("Parse REST PUT") {
    VPackBuilder body;

    // intentionally building this in non-alphabetic order, and name not first
    //  {"name":"CreateCollection","collection":"a","database":"test","properties":{"journalSize":1111}}
    { VPackObjectBuilder b(&body);
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
    arangodb::HttpRequest * dummyRequest = arangodb::HttpRequest::createHttpRequest(arangodb::rest::ContentType::JSON,
                                                                                    json_str.c_str(), json_str.length(), x);
    dummyRequest->setRequestType(arangodb::rest::RequestType::PUT);
    TestResponse * dummyResponse = new TestResponse;
    TestHandler dummyHandler(dummyRequest,  dummyResponse);

    REQUIRE(true==dummyHandler.test_parsePutBody(body.slice()));
    REQUIRE(dummyHandler.getActionDesc().has("name"));
    REQUIRE(dummyHandler.getActionDesc().get("name") == "CreateCollection");
    REQUIRE(dummyHandler.getActionDesc().has("collection"));
    REQUIRE(dummyHandler.getActionDesc().get("collection") == "a");
    REQUIRE(dummyHandler.getActionDesc().has("database"));
    REQUIRE(dummyHandler.getActionDesc().get("database") == "test");

    VPackObjectIterator it(dummyHandler.getActionProp().slice(), true);
    REQUIRE(it.key().copyString() == "journalSize");
    REQUIRE(it.value().getInt() == 1111);

  }

  SECTION("Local databases one more empty database should be dropped") {

  }
}
