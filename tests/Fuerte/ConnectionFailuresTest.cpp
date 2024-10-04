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

#include <atomic>
#include <string_view>
#include <utility>

#include "Logger/LogMacros.h"

namespace f = ::arangodb::fuerte;

extern std::string myEndpoint;

// for testing connection failures, we need a free port that is not used by
// another service. because we can't be sure about high port numbers, we are
// using some from the very range. according to
// https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml?&page=2
// port 60 is unassigned and thus likely not taken by any service on the test
// host. previously we used port 8629 for testing connection failures, but this
// was flawed because it was possible that some service was running on port
// 8629, which then made the connection failure tests fail.
constexpr std::string_view urls[] = {
    "http://localhost:60",
    "h2://localhost:60",
    "ssl://localhost:60",
    "h2s://localhost:60",
};

static std::pair<int, int> runTimeoutTest(f::ConnectionBuilder& cbuilder,
                                          int n) {
  cbuilder.verifyHost(false);

  f::WaitGroup wg;
  f::EventLoopService loop;

  std::atomic<int> callbacksCalled = 0;
  std::atomic<int> failureCallbacksCalled = 0;

  cbuilder.onFailure([&](f::Error errorCode, std::string const& errorMessage) {
    ASSERT_EQ(errorCode, f::Error::CouldNotConnect);
    ++failureCallbacksCalled;
    wg.done();
  });

  for (int i = 0; i < n; ++i) {
    wg.add();
    auto connection = cbuilder.connect(loop);
    // Send a first request. (HTTP connection is only started upon first
    // request)
    auto request = f::createRequest(f::RestVerb::Get, "/_api/version");
    connection->sendRequest(std::move(request),
                            [&](f::Error error, std::unique_ptr<f::Request>,
                                std::unique_ptr<f::Response>) {
                              ++callbacksCalled;
                              if (error != f::Error::CouldNotConnect) {
                                wg.done();
                              }
                            });
  }
  auto success = wg.wait_for(std::chrono::seconds(60));
  EXPECT_TRUE(success);

  return {callbacksCalled.load(), failureCallbacksCalled.load()};
}

// tryToConnectExpectFailure tries to make a connection to a host with given
// url. This is expected to fail.
static void tryToConnectExpectFailure(f::EventLoopService& eventLoopService,
                                      std::string_view url, bool useRetries) {
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
  tryToConnectExpectFailure(
      loop, "http://thishostmustnotexist.arangodb.com:8529", false);
}

// CannotConnect tests try to make a connection to a host with a valid name
// but a wrong port.
TEST(ConnectionFailureTest, CannotConnect) {
  for (auto const& url : urls) {
    f::EventLoopService loop;
    tryToConnectExpectFailure(loop, url, /*useRetries*/ false);
  }
}

TEST(ConnectionFailureTest, CannotConnectForceRetries) {
  for (auto const& url : urls) {
    f::EventLoopService loop;
    tryToConnectExpectFailure(loop, url, /*useRetries*/ true);
  }
}

TEST(ConnectionFailureTest, LowTimeouts) {
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(1));
  cbuilder.connectRetryPause(std::chrono::milliseconds(1));
  cbuilder.maxConnectRetries(15);
  cbuilder.endpoint("ssl://localhost:60");

  int n = 100;
  auto [callbacksCalled, failureCallbacksCalled] = runTimeoutTest(cbuilder, n);
  ASSERT_EQ(n, callbacksCalled);
  ASSERT_LE(failureCallbacksCalled, n);
}

TEST(ConnectionFailureTest, LowTimeoutsActualBackend) {
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(1));
  cbuilder.connectRetryPause(std::chrono::milliseconds(5));
  cbuilder.maxConnectRetries(15);
  cbuilder.endpoint(myEndpoint);

  int n = 100;
  auto [callbacksCalled, failureCallbacksCalled] = runTimeoutTest(cbuilder, n);
  ASSERT_EQ(n, callbacksCalled);
  ASSERT_LE(failureCallbacksCalled, n);
}

TEST(ConnectionFailureTest, BorderlineTimeoutsActualBackend) {
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(5));
  cbuilder.connectRetryPause(std::chrono::milliseconds(5));
  cbuilder.maxConnectRetries(15);
  cbuilder.endpoint(myEndpoint);

  int n = 100;
  auto [callbacksCalled, failureCallbacksCalled] = runTimeoutTest(cbuilder, n);
  ASSERT_EQ(n, callbacksCalled);
  ASSERT_LE(failureCallbacksCalled, n);
}

TEST(ConnectionFailureTest, HighEnoughTimeoutsActualBackend) {
  f::ConnectionBuilder cbuilder;
  cbuilder.connectTimeout(std::chrono::milliseconds(60000));
  cbuilder.connectRetryPause(std::chrono::milliseconds(5));
  cbuilder.maxConnectRetries(15);
  cbuilder.endpoint(myEndpoint);

  int n = 100;
  auto [callbacksCalled, failureCallbacksCalled] = runTimeoutTest(cbuilder, n);
  ASSERT_EQ(n, callbacksCalled);
  ASSERT_EQ(0, failureCallbacksCalled);
}
