////////////////////////////////////////////////////////////////////////////////
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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/fuerte.h>
#include <fuerte/helper.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "connection_test.h"

namespace fu = ::arangodb::fuerte;

TEST_P(ConnectionTestF, ApiVersionSync) {
  for (size_t rep = 0; rep < repeat(); rep++) {
    auto request = fu::createRequest(fu::RestVerb::Get, "/_api/version");
    auto result = _connection->sendRequest(std::move(request));
    ASSERT_EQ(result->statusCode(), fu::StatusOK);
    auto slice = result->slices().front();
    auto version = slice.get("version").copyString();
    auto server = slice.get("server").copyString();
    ASSERT_EQ(server, "arango");
    ASSERT_EQ(version[0], _major_arango_version);
  }
}

TEST_P(ConnectionTestF, ApiVersionASync) {
  fu::WaitGroup wg;
  auto cb = [&](fu::Error error, std::unique_ptr<fu::Request> req, std::unique_ptr<fu::Response> res) {
     fu::WaitGroupDone done(wg);
     if (error != fu::Error::NoError) {
       ASSERT_TRUE(false) << fu::to_string(error);
     } else {
       ASSERT_EQ(res->statusCode(), fu::StatusOK);
       auto slice = res->slices().front();
       auto version = slice.get("version").copyString();
       auto server = slice.get("server").copyString();
       ASSERT_EQ(server, "arango");
       ASSERT_EQ(version[0], _major_arango_version);
     }
   };
  for (size_t rep = 0; rep < repeat(); rep++) {
    auto request = fu::createRequest(fu::RestVerb::Get, "/_api/version");
   
    wg.add();
    _connection->sendRequest(std::move(request), cb);
    if (wg.counter() >= 32) {
      wg.wait();
    }
  }
  wg.wait();
}

TEST_P(ConnectionTestF, SimpleCursorSync){
  auto request = fu::createRequest(fu::RestVerb::Post, "/_api/cursor");
  VPackBuilder builder;
  builder.openObject();
  builder.add("query", VPackValue("FOR x IN 1..5 RETURN x"));
  builder.close();
  request->addVPack(builder.slice());
  auto response = _connection->sendRequest(std::move(request));
  ASSERT_EQ(response->statusCode(), fu::StatusCreated);
  auto slice = response->slices().front();

  ASSERT_TRUE(slice.isObject());
  auto result = slice.get("result");
  ASSERT_TRUE(result.isArray());
  ASSERT_TRUE(result.length() == 5);
}

TEST_P(ConnectionTestF, CreateDocumentSync){
  dropCollection("test");
  createCollection("test");

  auto request = fu::createRequest(fu::RestVerb::Post, "/_api/document/test");
  request->addVPack(VPackSlice::emptyObjectSlice());
  auto response = _connection->sendRequest(std::move(request));
  ASSERT_EQ(response->statusCode(), fu::StatusAccepted);
  auto slice = response->slices().front();

  ASSERT_TRUE(slice.get("_id").isString());
  ASSERT_TRUE(slice.get("_key").isString());
  ASSERT_TRUE(slice.get("_rev").isString());
  
  dropCollection("test");
}

TEST_P(ConnectionTestF, ShortAndLongASync){
  fu::WaitGroup wg;
  fu::RequestCallback cb = [&](fu::Error error, std::unique_ptr<fu::Request> req, std::unique_ptr<fu::Response> res) {
    fu::WaitGroupDone done(wg);
    if (error != fu::Error::NoError) {
      ASSERT_TRUE(false) << fu::to_string(error);
    } else {
      ASSERT_EQ(res->statusCode(), fu::StatusCreated);
      auto slice = res->slices().front();
      ASSERT_TRUE(slice.isObject());
      ASSERT_TRUE(slice.get("code").isInteger());
      //std::cout << "messageID: " << req->messageID << " " << slice.toJson() << std::endl;
    }
  };

  auto requestShort = fu::createRequest(fu::RestVerb::Post, "/_api/cursor");
  {
    VPackBuilder builder;
    builder.openObject();
    builder.add("query", VPackValue("RETURN SLEEP(1)"));
    builder.close();
    requestShort->addVPack(builder.slice());
  }

  auto requestLong = fu::createRequest(fu::RestVerb::Post, "/_api/cursor");
  {
    VPackBuilder builder;
    builder.openObject();
    builder.add("query", VPackValue("RETURN SLEEP(2)"));
    builder.close();
    requestLong->addVPack(builder.slice());
  }

  wg.add();
  _connection->sendRequest(std::move(requestLong), cb);
  wg.add();
  _connection->sendRequest(std::move(requestShort), cb);
  wg.wait();
}

// threads parameter has no effect in this testsuite
static const ConnectionTestParams connectionTestBasicParams[] = {
  {/*._protocol = */fu::ProtocolType::Http,/*._threads=*/1, /*._repeat=*/100},
  {/*._protocol = */fu::ProtocolType::Http2,/*._threads=*/1, /*._repeat=*/100},
  {/*._protocol = */fu::ProtocolType::Vst,/*._threads=*/1, /*._repeat=*/100},
  {/*._protocol = */fu::ProtocolType::Http,/*._threads=*/1, /*._repeat=*/2500},
  {/*._protocol = */fu::ProtocolType::Http2,/*._threads=*/1, /*._repeat=*/2500},
  {/*._protocol = */fu::ProtocolType::Vst,/*._threads=*/1, /*._repeat=*/2500}
};

INSTANTIATE_TEST_CASE_P(BasicConnectionTests, ConnectionTestF,
  ::testing::ValuesIn(connectionTestBasicParams));

