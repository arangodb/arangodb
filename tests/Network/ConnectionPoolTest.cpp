////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ConnectionPool
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"

#include "Network/ConnectionPool.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>

using namespace arangodb;
using namespace arangodb::network;

TEST_CASE("network::ConnectionPool", "[network]") {
  ConnectionPool::Config config;
  config.numIOThreads = 1;
  config.minOpenConnections = 1;
  config.maxOpenConnections = 3;
  config.connectionTtlMilli = 10; // extra small for testing
  config.verifyHosts = false;
  config.protocol = fuerte::ProtocolType::Http;
  

  SECTION("acquire endpoint") {
    ConnectionPool pool(config);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto ref = pool.leaseConnection("tcp://example.org:80");
    CHECK(pool.numOpenConnections() == 1);
    auto req = fuerte::createRequest(fuerte::RestVerb::Get, fuerte::ContentType::Unset);
    auto res = ref.connection()->sendRequest(std::move(req));
    CHECK(res->statusCode() == fuerte::StatusOK);

  } // acquire endpoint

  SECTION("acquire multiple endpoints") {
    ConnectionPool pool(config);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto ref1 = pool.leaseConnection("tcp://example.org:80");
    auto ref2 = pool.leaseConnection("tcp://example.org:80");

    CHECK(ref1.connection().get() != ref2.connection().get());
    CHECK(pool.numOpenConnections() == 2);
    
    auto ref3 = pool.leaseConnection("tcp://example.com:80");
    CHECK(ref1.connection().get() != ref3.connection().get());
    
    CHECK(pool.numOpenConnections() == 3);
  } // acquire multiple endpoints
  
  SECTION("release multiple endpoints (1)") {
    ConnectionPool pool(config);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
      auto ref1 = pool.leaseConnection("tcp://example.org:80");
      CHECK(pool.numOpenConnections() == 1);
      
      auto ref2 = pool.leaseConnection("tcp://example.com:80");
      CHECK(ref1.connection().get() != ref2.connection().get());
      CHECK(pool.numOpenConnections() == 2);
    }
    CHECK(pool.numOpenConnections() == 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(11));
    pool.pruneConnections();
    
    CHECK(pool.numOpenConnections() == 2); // keep one endpoint each
  } // acquire multiple endpoints
  
  SECTION("release multiple endpoints (2)") {
    config.minOpenConnections = 0;
    ConnectionPool pool(config);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
      auto ref1 = pool.leaseConnection("tcp://example.org:80");
      CHECK(pool.numOpenConnections() == 1);
      
      auto ref2 = pool.leaseConnection("tcp://example.com:80");
      CHECK(ref1.connection().get() != ref2.connection().get());
      CHECK(pool.numOpenConnections() == 2);
    }
    CHECK(pool.numOpenConnections() == 2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(11));
    pool.pruneConnections();
    
    CHECK(pool.numOpenConnections() == 0);
  } // acquire multiple endpoints
  
  SECTION("checking min and max connections") {
    config.minOpenConnections = 1;
    config.maxOpenConnections = 2;
    ConnectionPool pool(config);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
      auto ref1 = pool.leaseConnection("tcp://example.org:80");
      CHECK(pool.numOpenConnections() == 1);
      
      auto ref2 = pool.leaseConnection("tcp://example.org:80");
      CHECK(ref1.connection().get() != ref2.connection().get());
      CHECK(pool.numOpenConnections() == 2);
      
      auto ref3 = pool.leaseConnection("tcp://example.org:80");
      CHECK(ref1.connection().get() != ref3.connection().get());
      CHECK(pool.numOpenConnections() == 3);
    }
    CHECK(pool.numOpenConnections() == 3);
    
    // 15ms > 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    pool.pruneConnections();
    
    CHECK(pool.numOpenConnections() == 1);
  } // acquire multiple endpoints
}

