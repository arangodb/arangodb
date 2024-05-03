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

#include <string_view>

namespace f = ::arangodb::fuerte;

// for testing connection failures, we need a free port that is not used by
// another service. because we can't be sure about high port numbers, we are
// using some from the very range. according to
// https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml?&page=2
// port 60 is unassigned and thus likely not taken by any service on the test
// host. previously we used port 8629 for testing connection failures, but this
// was flawed because it was possible that some service was running on port
// 8629, which then made the connection failure tests fail.
constexpr std::string_view urls[] = {
    "http://localhost:60", "h2://localhost:60",  "vst://localhost:60",
    "ssl://localhost:60",  "h2s://localhost:60",
};

// tryToConnectExpectFailure tries to make a connection to a host with given
// url. This is expected to fail.
static void tryToConnectExpectFailure(f::EventLoopService& eventLoopService,
                                      std::string_view url) {
  f::WaitGroup wg;

  wg.add();
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(250));
  cbuilder.endpoint(std::string{url});

  cbuilder.onFailure([&](f::Error errorCode, std::string const& errorMessage) {
    ASSERT_EQ(errorCode, f::Error::CouldNotConnect);
    wg.done();
  });
  auto connection = cbuilder.connect(eventLoopService);
  // Send a first request. (HTTP connection is only started upon first request)
  auto request = f::createRequest(f::RestVerb::Get, "/_api/version");
  connection->sendRequest(std::move(request),
                          [&](f::Error, std::unique_ptr<f::Request>,
                              std::unique_ptr<f::Response>) {});

  auto success = wg.wait_for(std::chrono::seconds(50));
  ASSERT_TRUE(success);
}

// CannotResolve tests try to make a connection to a host with a name
// that cannot be resolved.
TEST(ConnectionFailureTest, CannotResolveHttp) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop,
                            "http://thishostmustnotexist.arangodb.com:8529");
}

TEST(ConnectionFailureTest, CannotResolveVst) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop,
                            "vst://thishostmustnotexist.arangodb.com:8529");
}

// CannotConnect tests try to make a connection to a host with a valid name
// but a wrong port.
TEST(ConnectionFailureTest, CannotConnect) {
  for (auto const& url : urls) {
    f::EventLoopService loop;
    tryToConnectExpectFailure(loop, url);
  }
}
