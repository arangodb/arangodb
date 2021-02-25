////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/fuerte.h>
#include <fuerte/requests.h>
#include <velocypack/velocypack-aliases.h>

#include "gtest/gtest.h"
#include "common.h"

namespace fu = ::arangodb::fuerte;

namespace {

std::unique_ptr<fu::Request> sleepRequest(double sleep) {
  auto req = fu::createRequest(fu::RestVerb::Post, "/_api/cursor");
  {
    VPackBuilder builder;
    builder.openObject();
    builder.add("query", VPackValue("RETURN SLEEP(@timeout)"));
    builder.add("bindVars", VPackValue(VPackValueType::Object));
    builder.add("timeout", VPackValue(sleep));
    builder.close();
    builder.close();
    req->addVPack(builder.slice());
  }
  return req;
}

void performRequests(fu::ProtocolType pt) {
  fu::EventLoopService loop;
  // Set connection parameters
  fu::ConnectionBuilder cbuilder;
  setupEndpointFromEnv(cbuilder);
  setupAuthenticationFromEnv(cbuilder);
  
  cbuilder.protocolType(pt);
  
  // make connection
  auto connection = cbuilder.connect(loop);
  
  // should fail after 1s
  auto req = ::sleepRequest(10.0);
  req->timeout(std::chrono::seconds(1));
  
  fu::WaitGroup wg;
  wg.add();
  connection->sendRequest(std::move(req), [&](fu::Error e, std::unique_ptr<fu::Request> req,
                                              std::unique_ptr<fu::Response> res) {
    fu::WaitGroupDone done(wg);
    ASSERT_EQ(e, fu::Error::RequestTimeout);
  });
  ASSERT_TRUE(wg.wait_for(std::chrono::seconds(5)));
  
  if (pt == fu::ProtocolType::Http) {
    ASSERT_EQ(connection->state(), fu::Connection::State::Closed);
    return;
  }
  // http 1.1 connection is broken after timeout, others must still work
  ASSERT_EQ(connection->state(), fu::Connection::State::Connected);
  
  req = fu::createRequest(fu::RestVerb::Post, "/_api/version");
  wg.add();
  connection->sendRequest(std::move(req), [&](fu::Error e, std::unique_ptr<fu::Request> req,
                                              std::unique_ptr<fu::Response> res) {
    fu::WaitGroupDone done(wg);
    if (e != fu::Error::NoError) {
      ASSERT_TRUE(false) << fu::to_string(e);
    } else {
      ASSERT_EQ(res->statusCode(), fu::StatusOK);
      auto slice = res->slices().front();
      auto version = slice.get("version").copyString();
      auto server = slice.get("server").copyString();
      ASSERT_EQ(server, "arango");
      ASSERT_EQ(version[0], '3'); // major version
    }
  });
  wg.wait();
  
  for (int i = 0; i < 8; i++) {
    // should not fail
    req = ::sleepRequest(4.0);
    req->timeout(std::chrono::seconds(60));
    
    wg.add();
    connection->sendRequest(std::move(req), [&](fu::Error e, std::unique_ptr<fu::Request> req,
                                                std::unique_ptr<fu::Response> res) {
      fu::WaitGroupDone done(wg);
      ASSERT_EQ(e, fu::Error::NoError);
      ASSERT_TRUE(res != nullptr);
    });
    
    // should fail
    req = ::sleepRequest(4.0);
    req->timeout(std::chrono::milliseconds(100));
    
    wg.add();
    connection->sendRequest(std::move(req), [&](fu::Error e, std::unique_ptr<fu::Request> req,
                                                std::unique_ptr<fu::Response> res) {
      fu::WaitGroupDone done(wg);
      ASSERT_EQ(e, fu::Error::RequestTimeout);
      ASSERT_EQ(res, nullptr);
    });
  }
  
  ASSERT_TRUE(wg.wait_for(std::chrono::seconds(120)));
}

}

TEST(RequestTimeout, VelocyStream){
  ::performRequests(fu::ProtocolType::Vst);
}

TEST(RequestTimeout, HTTP) {
  ::performRequests(fu::ProtocolType::Http);
}

TEST(RequestTimeout, HTTP2) {
  ::performRequests(fu::ProtocolType::Http2);
}
