////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Endpoint/Endpoint.h"
#include "SimpleHttpClient/ConnectionCache.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

using namespace arangodb;
using namespace arangodb::httpclient;

TEST(ConnectionCacheTest, testEmpty) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  auto const& connections = cache.connections();
  EXPECT_EQ(0, connections.size());
}

TEST(ConnectionCacheTest, testAcquireInvalidEndpoint) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  ConnectionLease lease;
  EXPECT_EQ(nullptr, lease._connection);

  try {
    lease = cache.acquire("piff", 10.0, 30.0, 10, 0);
    ASSERT_FALSE(true);
  } catch (...) {
    // must throw
  }

  EXPECT_EQ(nullptr, lease._connection);

  auto const& connections = cache.connections();
  EXPECT_EQ(0, connections.size());
}

TEST(ConnectionCacheTest, testAcquireAndReleaseClosedConnection) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint = Endpoint::unifiedForm("tcp://127.0.0.1:9999");

  {
    ConnectionLease lease;
    EXPECT_EQ(nullptr, lease._connection);

    lease = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
    EXPECT_NE(nullptr, lease._connection);

    auto const& connections = cache.connections();
    EXPECT_EQ(0, connections.size());
  }
  // lease will automatically return connection to cache, but
  // connection is still closed, so dropped

  auto const& connections = cache.connections();
  EXPECT_EQ(0, connections.size());
}

TEST(ConnectionCacheTest, testAcquireAndReleaseClosedConnectionForce) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint = Endpoint::unifiedForm("tcp://127.0.0.1:9999");

  {
    ConnectionLease lease;
    EXPECT_EQ(nullptr, lease._connection);

    lease = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
    EXPECT_NE(nullptr, lease._connection);

    auto const& connections = cache.connections();
    EXPECT_EQ(0, connections.size());

    cache.release(std::move(lease._connection), true);
  }

  auto const& connections = cache.connections();
  EXPECT_EQ(1, connections.size());

  EXPECT_NE(connections.find(endpoint), connections.end());
  EXPECT_EQ(1, connections.find(endpoint)->second.size());
}

TEST(ConnectionCacheTest, testAcquireAndReleaseRepeat) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint = Endpoint::unifiedForm("tcp://127.0.0.1:9999");

  httpclient::GeneralClientConnection* gc1 = nullptr;
  {
    ConnectionLease lease = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
    EXPECT_NE(nullptr, lease._connection);

    {
      auto const& connections = cache.connections();
      EXPECT_EQ(0, connections.size());
    }

    gc1 = lease._connection.get();

    cache.release(std::move(lease._connection), true);

    {
      auto const& connections = cache.connections();
      EXPECT_EQ(1, connections.size());

      EXPECT_NE(connections.find(endpoint), connections.end());
      EXPECT_EQ(1, connections.find(endpoint)->second.size());
    }
  }

  httpclient::GeneralClientConnection* gc2 = nullptr;
  {
    ConnectionLease lease = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
    EXPECT_NE(nullptr, lease._connection);

    {
      auto const& connections = cache.connections();
      EXPECT_EQ(1, connections.size());
    }

    gc2 = lease._connection.get();

    cache.release(std::move(lease._connection), true);

    {
      auto const& connections = cache.connections();
      EXPECT_EQ(1, connections.size());

      EXPECT_NE(connections.find(endpoint), connections.end());
      EXPECT_EQ(1, connections.find(endpoint)->second.size());
    }
  }

  EXPECT_NE(gc1, nullptr);
  EXPECT_EQ(gc1, gc2);
}

TEST(ConnectionCacheTest, testSameEndpointMultipleLeases) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint = Endpoint::unifiedForm("tcp://127.0.0.1:9999");

  ConnectionLease lease1 = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
  EXPECT_NE(nullptr, lease1._connection);
  httpclient::GeneralClientConnection* gc1 = lease1._connection.get();

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(0, connections.size());
  }

  ConnectionLease lease2 = cache.acquire(endpoint, 10.0, 30.0, 10, 0);
  EXPECT_NE(nullptr, lease2._connection);
  httpclient::GeneralClientConnection* gc2 = lease2._connection.get();

  EXPECT_NE(gc1, gc2);

  cache.release(std::move(lease1._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(1, connections.size());

    EXPECT_NE(connections.find(endpoint), connections.end());
    EXPECT_EQ(1, connections.find(endpoint)->second.size());
    EXPECT_EQ(gc1, connections.find(endpoint)->second[0].get());
  }

  cache.release(std::move(lease2._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(1, connections.size());

    EXPECT_NE(connections.find(endpoint), connections.end());
    EXPECT_EQ(2, connections.find(endpoint)->second.size());
    EXPECT_EQ(gc1, connections.find(endpoint)->second[0].get());
    EXPECT_EQ(gc2, connections.find(endpoint)->second[1].get());
  }
}

TEST(ConnectionCacheTest, testDifferentEndpoints) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint1 = Endpoint::unifiedForm("tcp://127.0.0.1:9999");
  std::string endpoint2 = Endpoint::unifiedForm("tcp://127.0.0.1:12345");

  ConnectionLease lease = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  cache.release(std::move(lease._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(1, connections.size());

    EXPECT_NE(connections.find(endpoint1), connections.end());
    EXPECT_EQ(1, connections.find(endpoint1)->second.size());

    EXPECT_EQ(connections.find(endpoint2), connections.end());
  }

  lease = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);
  cache.release(std::move(lease._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(2, connections.size());

    EXPECT_NE(connections.find(endpoint1), connections.end());
    EXPECT_EQ(1, connections.find(endpoint1)->second.size());

    EXPECT_NE(connections.find(endpoint2), connections.end());
    EXPECT_EQ(1, connections.find(endpoint2)->second.size());
  }
}

TEST(ConnectionCacheTest, testSameEndpointDifferentProtocols) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{5});

  std::string endpoint1 = Endpoint::unifiedForm("tcp://127.0.0.1:9999");
  std::string endpoint2 = Endpoint::unifiedForm("ssl://127.0.0.1:9999");

  ConnectionLease lease1 = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  cache.release(std::move(lease1._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(1, connections.size());

    EXPECT_NE(connections.find(endpoint1), connections.end());
    EXPECT_EQ(1, connections.find(endpoint1)->second.size());

    EXPECT_EQ(connections.find(endpoint2), connections.end());
  }

  ConnectionLease lease2 = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);
  cache.release(std::move(lease2._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(2, connections.size());

    EXPECT_NE(connections.find(endpoint1), connections.end());
    EXPECT_EQ(1, connections.find(endpoint1)->second.size());

    EXPECT_NE(connections.find(endpoint2), connections.end());
    EXPECT_EQ(1, connections.find(endpoint2)->second.size());
  }
}

TEST(ConnectionCacheTest, testDropSuperfluous) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server
      .addFeature<arangodb::application_features::CommunicationFeaturePhase>();

  ConnectionCache cache(server, ConnectionCache::Options{3});

  std::string endpoint1 = Endpoint::unifiedForm("tcp://127.0.0.1:9999");
  std::string endpoint2 = Endpoint::unifiedForm("tcp://127.0.0.1:12345");

  ConnectionLease lease1 = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  ConnectionLease lease2 = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);
  ConnectionLease lease3 = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  ConnectionLease lease4 = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);
  ConnectionLease lease5 = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  ConnectionLease lease6 = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);
  ConnectionLease lease7 = cache.acquire(endpoint1, 10.0, 30.0, 10, 0);
  ConnectionLease lease8 = cache.acquire(endpoint2, 10.0, 30.0, 10, 0);

  cache.release(std::move(lease1._connection), true);
  cache.release(std::move(lease2._connection), true);
  cache.release(std::move(lease3._connection), true);
  cache.release(std::move(lease4._connection), true);
  cache.release(std::move(lease5._connection), true);
  cache.release(std::move(lease6._connection), true);
  cache.release(std::move(lease7._connection), true);
  cache.release(std::move(lease8._connection), true);

  {
    auto const& connections = cache.connections();
    EXPECT_EQ(2, connections.size());

    EXPECT_NE(connections.find(endpoint1), connections.end());
    EXPECT_EQ(3, connections.find(endpoint1)->second.size());

    EXPECT_NE(connections.find(endpoint2), connections.end());
    EXPECT_EQ(3, connections.find(endpoint2)->second.size());
  }
}
