////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/fuerte.h>
#include <fuerte/loop.h>
#include <fuerte/helper.h>

#include "gtest/gtest.h"

namespace f = ::arangodb::fuerte;

// tryToConnectExpectFailure tries to make a connection to a host with given
// url. This is expected to fail.
static void tryToConnectExpectFailure(f::EventLoopService& eventLoopService,
                                      std::string const& url, bool useRetries) {
  f::WaitGroup wg;

  wg.add();
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(250));
  cbuilder.connectRetryPause(std::chrono::milliseconds(100));
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (useRetries) {
    cbuilder.failConnectAttempts(2);
  }
  cbuilder.maxConnectRetries(3);
#endif
  cbuilder.endpoint(url);

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
  tryToConnectExpectFailure(
      loop, "http://thishostmustnotexist.arangodb.com:8529", false);
}

// CannotConnect tests try to make a connection to a host with a valid name
// but a wrong port.
TEST(ConnectionFailureTest, CannotConnectHttp) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "http://localhost:8629", false);
}

TEST(ConnectionFailureTest, CannotConnectHttp2) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "h2://localhost:8629", false);
}

TEST(ConnectionFailureTest, CannotConnectHttpAndSsl) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "ssl://localhost:8629", false);
}

TEST(ConnectionFailureTest, CannotConnectHttp2AndSsl) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "h2s://localhost:8629", false);
}

TEST(ConnectionFailureTest, CannotConnectHttpRetries) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "http://localhost:8629", true);
}

TEST(ConnectionFailureTest, CannotConnectHttp2Retries) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "h2://localhost:8629", true);
}

TEST(ConnectionFailureTest, CannotConnectHttpAndSslRetries) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "ssl://localhost:8629", true);
}

TEST(ConnectionFailureTest, CannotConnectHttp2AndSslRetries) {
  f::EventLoopService loop;
  tryToConnectExpectFailure(loop, "h2s://localhost:8629", true);
}
