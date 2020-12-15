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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <velocypack/Parser.h>
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

using namespace arangodb;

struct DummyConnection final : public fuerte::Connection {
  DummyConnection(fuerte::detail::ConnectionConfiguration const& conf) : fuerte::Connection(conf) {}
  void sendRequest(std::unique_ptr<fuerte::Request> r,
                                fuerte::RequestCallback cb) override {
    _sendRequestNum++;
    cb(_err, std::move(r), std::move(_response));
  }
  
  std::size_t requestsLeft() const override {
    return 1;
  }
  
  State state() const override {
    return _state;
  }
  
  void cancel() override {}
  void start() override {}
  bool lease() override {
    return true;
  }
  void unlease() override {}

  fuerte::Connection::State _state = fuerte::Connection::State::Connected;
  
  fuerte::Error _err = fuerte::Error::NoError;
  std::unique_ptr<fuerte::Response> _response;
  int _sendRequestNum = 0;
};

struct DummyPool : public network::ConnectionPool {
  DummyPool(network::ConnectionPool::Config const& c)
  : network::ConnectionPool(c),
  _conn(std::make_shared<DummyConnection>(fuerte::detail::ConnectionConfiguration())) {
  }
  std::shared_ptr<fuerte::Connection> createConnection(fuerte::ConnectionBuilder&) override {
    return _conn;
  }
  
  std::shared_ptr<DummyConnection> _conn;
};

struct NetworkMethodsTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::THREADS, arangodb::LogLevel::FATAL> {
  NetworkMethodsTest() : server(false) {
    server.addFeature<SchedulerFeature>(true);
    server.startFeatures();

    pool = std::make_unique<DummyPool>(config());
  }

 private:
  network::ConnectionPool::Config config() {
    network::ConnectionPool::Config config;
    config.clusterInfo = &server.getFeature<ClusterFeature>().clusterInfo();
    config.numIOThreads = 1;
    config.minOpenConnections = 1;
    config.maxOpenConnections = 3;
    config.verifyHosts = false;
    config.name = "NetworkMethodsTest";
    return config;
  }

 protected:
  tests::mocks::MockCoordinator server;
  std::unique_ptr<DummyPool> pool;
};

TEST_F(NetworkMethodsTest, simple_request) {
  pool->_conn->_err = fuerte::Error::NoError;
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  fuerte::ResponseHeader header;
  header.responseCode = fuerte::StatusAccepted;
  header.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response = std::make_unique<fuerte::Response>(std::move(header));
  std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"error\":false}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get,
                                "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_NE(res.response, nullptr);
  ASSERT_EQ(res.response->statusCode(), fuerte::StatusAccepted);
}

TEST_F(NetworkMethodsTest, request_failure) {
  pool->_conn->_err = fuerte::Error::ConnectionClosed;
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get,
                                "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::ConnectionClosed);
  ASSERT_EQ(res.response, nullptr);
}

TEST_F(NetworkMethodsTest, request_with_retry_after_error) {
  // Step 1: Provoke a connection error
  pool->_conn->_err = fuerte::Error::CouldNotConnect;
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(5.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                     fuerte::RestVerb::Get, "/", buffer,
                                     reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());
  ASSERT_EQ(pool->_conn->_sendRequestNum, 1);

  // Step 2: Now respond with no error
  pool->_conn->_err = fuerte::Error::NoError;

  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusAccepted;
  pool->_conn->_response = std::make_unique<fuerte::Response>(std::move(header));
  std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"error\":false}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  auto status = f.wait_for(std::chrono::milliseconds(350));
  ASSERT_EQ(futures::FutureStatus::Ready, status);
  
  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_NE(res.response, nullptr);
  ASSERT_EQ(res.response->statusCode(), fuerte::StatusAccepted);
}

TEST_F(NetworkMethodsTest, request_with_retry_after_not_found_error) {
  // Step 1: Provoke a data source not found error
  pool->_conn->_err = fuerte::Error::NoError;
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusNotFound;
  pool->_conn->_response = std::make_unique<fuerte::Response>(std::move(header));
  std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"errorNum\":1203}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);
  
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);
  reqOpts.retryNotFound = true;

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                     fuerte::RestVerb::Get, "/", buffer,
                                     reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());
  
  // Step 2: Now respond with no error
  pool->_conn->_err = fuerte::Error::NoError;

  header.responseCode = fuerte::StatusAccepted;
  header.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response = std::make_unique<fuerte::Response>(std::move(header));
  b = VPackParser::fromJson("{\"error\":false}");
  resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  auto status = f.wait_for(std::chrono::milliseconds(350));
  ASSERT_EQ(futures::FutureStatus::Ready, status);
  
  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_NE(res.response, nullptr);
  ASSERT_EQ(res.response->statusCode(), fuerte::StatusAccepted);
}
