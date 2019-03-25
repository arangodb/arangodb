////////////////////////////////////////////////////////////////////////////////
/// @brief test case for RestAqlHandler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/RestAqlHandler.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::traverser;

namespace arangodb {
namespace tests {
namespace rest_aql_handler_test {

class FakeResponse : public GeneralResponse {
  public:
   FakeResponse()
       : GeneralResponse(rest::ResponseCode::SERVER_ERROR),
         _transport(Endpoint::TransportType::VST) {}

   FakeResponse(Endpoint::TransportType transport)
       : GeneralResponse(rest::ResponseCode::SERVER_ERROR),
         _transport(transport) {}

   ~FakeResponse() {}

   arangodb::Endpoint::TransportType transportType() override {
     return _transport;
  };

   void reset(rest::ResponseCode code) override {
     _responseCode = code;
   }

  void addPayload(VPackSlice const&,
                  arangodb::velocypack::Options const* = nullptr,
                  bool resolveExternals = true) override {
    // TODO
  };
  void addPayload(VPackBuffer<uint8_t>&&,
                  arangodb::velocypack::Options const* = nullptr,
                  bool resolveExternals = true) override {
    // TODO
  };

  private:
    arangodb::Endpoint::TransportType const _transport;
};

/*
SCENARIO("Successful query setup", "[aql][restaqlhandler]") {

  // We always work on DBServer
  ServerState::instance()->setRole(ServerState::ROLE_DBSERVER);
  auto body = std::make_shared<VPackBuilder>();

  std::string dbName = "UnitTestDB";
  std::string user = "MyUser";
  std::unordered_map<std::string, std::string> req_headers;

  // only test setup
  std::vector<std::string> suffixes{"setup"};
  // setup only allows POST
  rest::RequestType reqType = rest::RequestType::POST;

  // Base setup of a request
  fakeit::Mock<GeneralRequest> reqMock;
  GeneralRequest& req = reqMock.get();
  fakeit::When(
      ConstOverloadedMethod(reqMock, header,
                            std::string const&(std::string const&, bool&)))
      .AlwaysDo([&](std::string const& key, bool& found) -> std::string const& {
        auto it = req_headers.find(key);
        if (it == req_headers.end()) {
          found = false;
          return StaticStrings::Empty;
        } else {
          found = true;
          return it->second;
        }
      });
  fakeit::When(Method(reqMock, databaseName)).AlwaysReturn(dbName);
  fakeit::When(Method(reqMock, user)).AlwaysReturn(user);
  fakeit::When(Method(reqMock, suffixes)).AlwaysDo([&] () -> std::vector<std::string> const& {
    return suffixes;
  });
  fakeit::When(Method(reqMock, requestType)).AlwaysDo([&] () -> rest::RequestType {
    return reqType;
  });
  fakeit::When(Method(reqMock, toVelocyPackBuilderPtr)).AlwaysDo([&] () -> std::shared_ptr<VPackBuilder> {
    return body;
  });

  fakeit::When(Dtor(reqMock)).Do([] () {} )
    .Throw(arangodb::basics::Exception(TRI_ERROR_DEBUG, __FILE__, __LINE__));


  fakeit::Mock<VocbaseContext> ctxtMock;
  VocbaseContext& ctxt = ctxtMock.get();;

  fakeit::Mock<TRI_vocbase_t> vocbaseMock;
  TRI_vocbase_t& vocbase = vocbaseMock.get();
  fakeit::When(Method(reqMock, requestContext)).AlwaysReturn(&ctxt);
  fakeit::When(Method(ctxtMock, vocbase)).AlwaysReturn(&vocbase);


  // Base setup of a response

  // Base setup of the registries
  fakeit::Mock<QueryRegistry> queryRegMock;
  QueryRegistry& queryReg = queryRegMock.get();

  fakeit::Mock<TraverserEngineRegistry> travRegMock;
  TraverserEngineRegistry& travReg = travRegMock.get();

  std::pair<QueryRegistry*, TraverserEngineRegistry*> engines{&queryReg, &travReg};

  // The testee takes ownership of response!
  // It stays valid until testee is destroyed
  FakeResponse* res = new FakeResponse();

  // Build the handler
  RestAqlHandler testee(&req, res, &engines);

  THEN("It should give the correct name") {
    REQUIRE(std::string(testee.name()) == "RestAqlHandler");
  }

  THEN("It should never be direct") {
    REQUIRE(testee.isDirect() == false);
  }

  GIVEN("A single query snippet") {

//  {
//    lockInfo: {
//      READ: [<collections to read-lock],
//      WRITE: [<collections to write-lock]
//    },
//    options: { < query options > },
//    snippets: {
//      <queryId: {nodes: [ <nodes>]}>
//    },
//    variables: [ <variables> ]
//  }


    body->openObject();
    body->add(VPackValue("lockInfo"));
    body->openObject();
    body->close();

    body->add(VPackValue("options"));
    body->openObject();
    body->close();

    body->add(VPackValue("snippets"));
    body->openObject();
    body->close();

    body->add(VPackValue("variables"));
    body->openArray();
    body->close();

    body->close();
    RestStatus status = testee.execute();

    THEN("It should succeed") {
      REQUIRE(!status.isFailed());
      REQUIRE(res->responseCode() == rest::ResponseCode::OK);
    }
  }

  GIVEN("A list of query snippets") {
  }

  GIVEN("A single traverser engine") {
  }

  GIVEN("A traverser engine and a query snippet") {
  }

}
*/

SCENARIO("Error in query setup", "[aql][restaqlhandler]") {
GIVEN("A single query snippet") {
}

GIVEN("A list of query snippets") {
}

GIVEN("A single traverser engine") {
}

GIVEN("A traverser engine and a query snippet") {
}


}

}
}
}
