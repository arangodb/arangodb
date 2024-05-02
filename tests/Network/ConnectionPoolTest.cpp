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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "Mocks/Servers.h"

#include "Metrics/MetricsFeature.h"
#include "Metrics/Gauge.h"
#include "Network/ConnectionPool.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

#include <atomic>

using namespace arangodb;
using namespace arangodb::network;

namespace {

void doNothing(fuerte::Error, std::unique_ptr<fuerte::Request> req,
               std::unique_ptr<fuerte::Response> res) {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
};
}  // namespace

namespace arangodb {
namespace tests {

struct NetworkConnectionPoolTest : public ::testing::Test {
  NetworkConnectionPoolTest() : server(false) { server.startFeatures(); }

 protected:
  metrics::MetricsFeature& metrics() {
    return server.getFeature<metrics::MetricsFeature>();
  }

  uint64_t extractCurrentMetric() {
    auto m = metrics().get(currentConnectionsMetric);
    TRI_ASSERT(m != nullptr) << "Metric not registered";
    auto gauge = dynamic_cast<metrics::Gauge<uint64_t>*>(m);
    return gauge->load();
  }
  tests::mocks::MockMetricsServer server;

  static constexpr metrics::MetricKeyView currentConnectionsMetric{
      "arangodb_connection_pool_connections_current", "pool=\"\""};
};

TEST_F(NetworkConnectionPoolTest, prune_while_in_flight) {
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 5;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  std::atomic<bool> done1 = false;
  std::atomic<bool> done2 = false;

  auto waiter = [&done1, &done2](fuerte::Error,
                                 std::unique_ptr<fuerte::Request> req,
                                 std::unique_ptr<fuerte::Response> res) {
    done2.store(true);
    done2.notify_one();

    done1.wait(false);
  };

  {
    bool isFromPool;
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       waiter);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  pool.pruneConnections();

  ASSERT_EQ(pool.numOpenConnections(), 1);
  EXPECT_EQ(extractCurrentMetric(), 1ull);

  // wake up blocked connection
  done1.store(true);
  done1.notify_one();

  done2.wait(false);

  // let it wake up and finish
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  pool.pruneConnections();

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, acquire_endpoint) {
  ConnectionPool::Config config(metrics());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  ConnectionPool pool(config);

  bool isFromPool;
  auto conn = pool.leaseConnection("tcp://example.org:80", isFromPool);
  ASSERT_EQ(pool.numOpenConnections(), 1);
  EXPECT_EQ(extractCurrentMetric(), 1ull);
  auto req =
      fuerte::createRequest(fuerte::RestVerb::Get, fuerte::ContentType::Unset);
  auto res = conn->sendRequest(std::move(req));
  ASSERT_EQ(res->statusCode(), fuerte::StatusOK);
  ASSERT_TRUE(res->payloadSize() > 0);
}

TEST_F(NetworkConnectionPoolTest, acquire_multiple_endpoint) {
  ConnectionPool::Config config(metrics());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);

  conn1->sendRequest(
      fuerte::createRequest(fuerte::RestVerb::Get, fuerte::ContentType::Unset),
      doNothing);

  auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);

  ASSERT_NE(conn1.get(), conn2.get());
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  auto conn3 = pool.leaseConnection("tcp://example.com:80", isFromPool);
  ASSERT_NE(conn1.get(), conn3.get());

  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);
}

TEST_F(NetworkConnectionPoolTest, release_multiple_endpoints_one) {
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 5;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  {
    bool isFromPool;
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  // number of connections should go down quickly, as we are calling
  // pruneConnections() and the TTL for connections is just 5 ms
  int tries = 0;
  while (++tries < 1'000) {
    if (pool.numOpenConnections() == 0) {
      break;
    }
    pool.pruneConnections();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, release_multiple_endpoints_two) {
  ConnectionPool::Config config(metrics());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  std::this_thread::sleep_for(std::chrono::milliseconds(21));
  // this will only expire conn2 (conn1 is still in use)

  // number of connections should go down quickly, as we are calling
  // pruneConnections() and the TTL for connections is just 5 ms
  int tries = 0;
  while (++tries < 1'000) {
    if (pool.numOpenConnections() == 0) {
      break;
    }
    pool.pruneConnections();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();

  // Drain needs to erase all connections
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  std::this_thread::sleep_for(std::chrono::milliseconds(21));
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  tries = 0;
  while (++tries < 1'000) {
    if (pool.numOpenConnections() == 0) {
      break;
    }
    pool.pruneConnections();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, force_drain) {
  ConnectionPool::Config config(metrics());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  pool.drainConnections();
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, checking_min_and_max_connections) {
  ConnectionPool::Config config(metrics());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn3.get());
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  int tries = 0;
  while (++tries < 1'000) {
    if (pool.numOpenConnections() == 0) {
      break;
    }
    pool.pruneConnections();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);

    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn3.get());
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);

    conn3->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset),
                       doNothing);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  tries = 0;
  while (++tries < 1'000) {
    if (pool.numOpenConnections() == 0) {
      break;
    }
    pool.pruneConnections();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, checking_expiration) {
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 1);
  EXPECT_EQ(extractCurrentMetric(), 1ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, checking_expiration_multiple_endpints) {
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection(s)
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  {
    auto conn4 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 4);
    EXPECT_EQ(extractCurrentMetric(), 4ull);

    auto conn5 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 5);
    EXPECT_EQ(extractCurrentMetric(), 5ull);

    // 21ms > 2 * 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(21));

    // expires the connections
    pool.pruneConnections();
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);
  }

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection("tcp://example.net:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);

  pool.drainConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, test_cancel_endpoint_all) {
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);
  }
  EXPECT_EQ(pool.numOpenConnections(), 3);
  EXPECT_EQ(extractCurrentMetric(), 3ull);

  // cancel all connections
  pool.cancelConnections("tcp://example.org:80");
  EXPECT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

TEST_F(NetworkConnectionPoolTest, test_cancel_endpoint_some) {
  std::string endpointA = "tcp://example.org:80";
  std::string endpointB = "tcp://example.org:800";
  ConnectionPool::Config config(server.getFeature<metrics::MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10;  // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;

  ConnectionPool pool(config);

  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection(endpointA, isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 1);
    EXPECT_EQ(extractCurrentMetric(), 1ull);

    auto conn2 = pool.leaseConnection(endpointB, isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 2);
    EXPECT_EQ(extractCurrentMetric(), 2ull);

    auto conn3 = pool.leaseConnection(endpointA, isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 3);
    EXPECT_EQ(extractCurrentMetric(), 3ull);

    auto conn4 = pool.leaseConnection(endpointB, isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 4);
    EXPECT_EQ(extractCurrentMetric(), 4ull);

    auto conn5 = pool.leaseConnection(endpointA, isFromPool);
    EXPECT_FALSE(isFromPool);
    EXPECT_EQ(pool.numOpenConnections(), 5);
    EXPECT_EQ(extractCurrentMetric(), 5ull);
  }
  // 3 from A, 2 from B
  EXPECT_EQ(pool.numOpenConnections(), 5);
  EXPECT_EQ(extractCurrentMetric(), 5ull);

  // cancel all connections from endpointA
  pool.cancelConnections(endpointA);
  // The connections to endpointB stay intact
  EXPECT_EQ(pool.numOpenConnections(), 2);
  EXPECT_EQ(extractCurrentMetric(), 2ull);

  // cancel all connections from endpointB
  pool.cancelConnections(endpointB);
  // No connections left
  EXPECT_EQ(pool.numOpenConnections(), 0);
  EXPECT_EQ(extractCurrentMetric(), 0ull);
}

}  // namespace tests
}  // namespace arangodb
