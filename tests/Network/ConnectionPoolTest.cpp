////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "Network/ConnectionPool.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::network;

namespace {

void doNothing(fuerte::Error, std::unique_ptr<fuerte::Request> req,
              std::unique_ptr<fuerte::Response> res) {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
};
}

namespace arangodb {
namespace tests {

struct NetworkConnectionPoolTest
    : public ::testing::Test {
  NetworkConnectionPoolTest() : server(false) {
    server.startFeatures();
  }
 protected:
  tests::mocks::MockMetricsServer server;
};

TEST_F(NetworkConnectionPoolTest, acquire_endpoint) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  auto conn = pool.leaseConnection("tcp://example.org:80", isFromPool);
  ASSERT_EQ(pool.numOpenConnections(), 1);
  auto req = fuerte::createRequest(fuerte::RestVerb::Get, fuerte::ContentType::Unset);
  auto res = conn->sendRequest(std::move(req));
  ASSERT_EQ(res->statusCode(), fuerte::StatusOK);
  ASSERT_TRUE(res->payloadSize() > 0);
}

TEST_F(NetworkConnectionPoolTest, acquire_multiple_endpoint) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
  
  conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                           fuerte::ContentType::Unset), doNothing);
  
  auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
  
  ASSERT_NE(conn1.get(), conn2.get());
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  auto conn3 = pool.leaseConnection("tcp://example.com:80", isFromPool);
  ASSERT_NE(conn1.get(), conn3.get());
  
  ASSERT_EQ(pool.numOpenConnections(), 3);
}

TEST_F(NetworkConnectionPoolTest, release_multiple_endpoints_one) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 5; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);

  {
    bool isFromPool;
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(15));
  pool.pruneConnections();
  
  ASSERT_EQ(pool.numOpenConnections(), 1); // keep one busy connection
}

TEST_F(NetworkConnectionPoolTest, release_multiple_endpoints_two) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 3;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(21));
  // this will only expire conn2 (conn1 is still in use)
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 1);

  pool.drainConnections();
  
  {

    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(21));
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);

  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(21));
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  pool.drainConnections();
}

TEST_F(NetworkConnectionPoolTest, checking_min_and_max_connections) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
   
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
    
    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn3.get());
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 3);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // this will only expire conn3 (conn1 and conn2 are still in use)
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 2);

  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    conn1->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
   
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    conn2->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
    
    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_NE(conn1.get(), conn3.get());
    ASSERT_NE(conn1.get(), conn2.get());
    ASSERT_EQ(pool.numOpenConnections(), 3);
    
    conn3->sendRequest(fuerte::createRequest(fuerte::RestVerb::Get,
                                             fuerte::ContentType::Unset), doNothing);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 3);

  pool.drainConnections();
}

TEST_F(NetworkConnectionPoolTest, checking_expiration) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
  }
  ASSERT_EQ(pool.numOpenConnections(), 1);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  
  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);

  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  
  pool.drainConnections();
}

TEST_F(NetworkConnectionPoolTest, checking_expiration_multiple_endpints) {
  ConnectionPool::Config config(server.getFeature<MetricsFeature>());
  config.numIOThreads = 1;
  config.maxOpenConnections = 2;
  config.idleConnectionMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  
  ConnectionPool pool(config);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  bool isFromPool;
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection(s)
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);

  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  ASSERT_EQ(pool.numOpenConnections(), 2);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connection
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);

  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    auto conn3 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  
  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    auto conn3 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
  }  
  {
    
    auto conn4 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 4);
    
    auto conn5 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 5);
    
    // 21ms > 2 * 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(21));

    // expires the connections
    pool.pruneConnections();
    ASSERT_EQ(pool.numOpenConnections(), 2);
  }
  
  pool.drainConnections();
  
  {
    auto conn1 = pool.leaseConnection("tcp://example.org:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 1);
    
    auto conn2 = pool.leaseConnection("tcp://example.com:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 2);
    
    auto conn3 = pool.leaseConnection("tcp://example.net:80", isFromPool);
    ASSERT_EQ(pool.numOpenConnections(), 3);
  }
  ASSERT_EQ(pool.numOpenConnections(), 3);
  
  // 21ms > 2 * 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(21));

  // expires the connections
  pool.pruneConnections();
  ASSERT_EQ(pool.numOpenConnections(), 0);
  
  pool.drainConnections();
}

}
}
