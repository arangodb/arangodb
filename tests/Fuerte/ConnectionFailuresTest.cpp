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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/fuerte.h>
#include <fuerte/loop.h>
#include <fuerte/helper.h>

#include "gtest/gtest.h"

namespace f = ::arangodb::fuerte;

// tryToConnectExpectFailure tries to make a connection to a host with given url.
// This is expected to fail.
static void tryToConnectExpectFailure(f::EventLoopService& eventLoopService,
                                      const std::string& url) {
  f::WaitGroup wg;

  wg.add();
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(250));
  cbuilder.connectRetryPause(std::chrono::milliseconds(100));
  cbuilder.endpoint(url);
  
  cbuilder.onFailure([&](f::Error errorCode, const std::string& errorMessage){
    ASSERT_EQ(errorCode, f::Error::CouldNotConnect);
    wg.done();
  });
  auto connection = cbuilder.connect(eventLoopService);
  // Send a first request. (HTTP connection is only started upon first request)
  auto request = f::createRequest(f::RestVerb::Get, "/_api/version");
  connection->sendRequest(std::move(request), [&](f::Error, std::unique_ptr<f::Request>, std::unique_ptr<f::Response>){});

  auto success = wg.wait_for(std::chrono::seconds(5));
  ASSERT_TRUE(success);
}

// CannotResolve tests try to make a connection to a host with a name
// that cannot be resolved.
TEST(ConnectionFailureTest, CannotResolveHttp) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "http://thishostmustnotexist.arangodb.com:8529");
}

TEST(ConnectionFailureTest, CannotResolveVst) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "vst://thishostmustnotexist.arangodb.com:8529");
}

// CannotConnect tests try to make a connection to a host with a valid name 
// but a wrong port.
TEST(ConnectionFailureTest, CannotConnectHttp) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "http://localhost:8629");
}

TEST(ConnectionFailureTest, CannotConnectHttp2) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "h2://localhost:8629");
}

TEST(ConnectionFailureTest, CannotConnectVst) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "vst://localhost:8629");
}

