////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Network/Methods.cpp
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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Compare.h>
#include <velocypack/velocypack-aliases.h>

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Cluster/ClusterFeature.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include "Agency/AsyncAgencyComm.h"

using namespace arangodb;
using namespace std::chrono_literals;

using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

VPackBuffer<uint8_t>&& vpackFromJsonString(char const* c) {
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return std::move(*builder->steal());
}

VPackBuffer<uint8_t>&& operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

struct AsyncAgencyCommTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::THREADS, arangodb::LogLevel::FATAL> {
  AsyncAgencyCommTest() : server(false) {
    server.addFeature<SchedulerFeature>(true);
    server.startFeatures();
  }

  network::ConnectionPool::Config config() {
    network::ConnectionPool::Config config;
    config.clusterInfo = &server.getFeature<ClusterFeature>().clusterInfo();
    config.numIOThreads = 1;
    config.minOpenConnections = 1;
    config.maxOpenConnections = 3;
    config.verifyHosts = false;
    return config;
  }

 protected:
  tests::mocks::MockCoordinator server;
};




struct AsyncAgencyCommPoolMock final : public network::ConnectionPool {

  struct RequestPrototype {
    std::string endpoint;
    fuerte::RestVerb method;
    std::string url;
    VPackBuffer<uint8_t> body;
    fuerte::Error error;
    std::unique_ptr<fuerte::Response> response;

    void returnResponse(fuerte::StatusCode statusCode, VPackBuffer<uint8_t>&& body) {
      fuerte::ResponseHeader header;
      header.contentType(fuerte::ContentType::VPack);
      header.responseCode = statusCode;

      response = std::make_unique<fuerte::Response>(std::move(header));
      response->setPayload(std::move(body), 0);
    };

    void returnError(fuerte::Error err) {
      error = err;
      response = nullptr;
    }

    void returnRedirect(std::string const& redirectTo) {

      fuerte::ResponseHeader header;
      header.contentType(fuerte::ContentType::VPack);
      header.responseCode = fuerte::StatusServiceUnavailable;
      header.addMeta (arangodb::StaticStrings::Location, redirectTo);

      response = std::make_unique<fuerte::Response>(std::move(header));
      response->setPayload(VPackBuffer<uint8_t>(), 0);
    }
  };

  struct Connection final : public fuerte::Connection {

    Connection(AsyncAgencyCommPoolMock *mock,
               std::string const& endpoint)
      : fuerte::Connection(fuerte::detail::ConnectionConfiguration()), _mock(mock), _endpoint(endpoint) {}

    std::size_t requestsLeft() const override { return 1; }
    State state() const override { return fuerte::Connection::State::Connected; }

    void cancel() override {}
    void startConnection() override {}

    AsyncAgencyCommPoolMock *_mock;
    std::string _endpoint;

    fuerte::MessageID sendRequest(std::unique_ptr<fuerte::Request> req, fuerte::RequestCallback cb) override {

      // check request
      TRI_ASSERT(_mock->_requests.size() > 0);
      auto &expectReq = _mock->_requests.front();
      TRI_ASSERT(expectReq.endpoint == _endpoint);
      TRI_ASSERT(expectReq.method == req->header.restVerb);
      TRI_ASSERT(expectReq.url == req->header.path);
      TRI_ASSERT(arangodb::velocypack::NormalizedCompare::equals(VPackSlice(expectReq.body.data()), req->slice()));

      // send response
      cb(expectReq.error, std::move(req), std::move(expectReq.response));

      _mock->_requests.pop_front();
      return 0;
    }
  };

  AsyncAgencyCommPoolMock(network::ConnectionPool::Config const& c) : network::ConnectionPool(c) {}

  std::shared_ptr<fuerte::Connection> createConnection(fuerte::ConnectionBuilder &cb) override {
    return std::make_shared<Connection>(this, cb.normalizedEndpoint());
  }

  RequestPrototype& expectRequest(std::string const& endpoint, fuerte::RestVerb method,
    std::string const& url, VPackBuffer<uint8_t>&& body) {
     _requests.emplace_back(RequestPrototype{endpoint, method, url, std::move(body), fuerte::Error::NoError, nullptr});
     return _requests.back();
  }

  std::deque<RequestPrototype> _requests;
};



TEST_F(AsyncAgencyCommTest, send_with_failover) {

  AsyncAgencyCommPoolMock pool(config());
  pool.expectRequest("http+tcp://10.0.0.1:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnResponse(200, "[{\"a\":12}]"_vpack);

  AsyncAgencyCommManager manager;
  manager.pool(&pool);
  manager.updateEndpoints({
    "http+tcp://10.0.0.1:8529", "http+tcp://10.0.0.2:8529", "http+tcp://10.0.0.3:8529",
  });

  auto result = AsyncAgencyComm(manager).sendWithFailover(fuerte::RestVerb::Post, "/_api/agency/read", 1s, "[[\"a\"]]"_vpack).get();
  ASSERT_EQ(result.error, fuerte::Error::NoError);
  ASSERT_EQ(result.slice().at(0).get("a").getNumber<int>(), 12);

  ASSERT_EQ(pool._requests.size(), 0);
}

TEST_F(AsyncAgencyCommTest, send_with_failover_failover) {

  AsyncAgencyCommPoolMock pool(config());
  pool.expectRequest("http+tcp://10.0.0.1:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnError(fuerte::Error::CouldNotConnect);
  pool.expectRequest("http+tcp://10.0.0.2:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnResponse(200, "[{\"a\":12}]"_vpack);

  AsyncAgencyCommManager manager;
  manager.pool(&pool);
  manager.updateEndpoints({
    "http+tcp://10.0.0.1:8529", "http+tcp://10.0.0.2:8529", "http+tcp://10.0.0.3:8529",
  });

  auto result = AsyncAgencyComm(manager).sendWithFailover(fuerte::RestVerb::Post, "/_api/agency/read", 1s, "[[\"a\"]]"_vpack).get();
  ASSERT_EQ(result.error, fuerte::Error::NoError);
  ASSERT_EQ(result.slice().at(0).get("a").getNumber<int>(), 12);

  ASSERT_EQ(pool._requests.size(), 0);
}

TEST_F(AsyncAgencyCommTest, send_with_failover_redirect) {

  AsyncAgencyCommPoolMock pool(config());
  pool.expectRequest("http+tcp://10.0.0.1:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnError(fuerte::Error::CouldNotConnect);
  pool.expectRequest("http+tcp://10.0.0.2:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnRedirect("http+tcp://10.0.0.3:8529");
  pool.expectRequest("http+tcp://10.0.0.3:8529", fuerte::RestVerb::Post, "/_api/agency/read", "[[\"a\"]]"_vpack)
    .returnResponse(200, "[{\"a\":12}]"_vpack);

  AsyncAgencyCommManager manager;
  manager.pool(&pool);
  manager.updateEndpoints({
    "http+tcp://10.0.0.1:8529", "http+tcp://10.0.0.2:8529", "http+tcp://10.0.0.3:8529",
  });

  auto result = AsyncAgencyComm(manager).sendWithFailover(fuerte::RestVerb::Post, "/_api/agency/read", 1s, "[[\"a\"]]"_vpack).get();
  ASSERT_EQ(result.error, fuerte::Error::NoError);
  ASSERT_EQ(result.slice().at(0).get("a").getNumber<int>(), 12);

  ASSERT_EQ(pool._requests.size(), 0);
}
