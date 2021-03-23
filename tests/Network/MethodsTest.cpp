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
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;

struct DummyConnection final : public fuerte::Connection {
  DummyConnection(fuerte::detail::ConnectionConfiguration const& conf) : fuerte::Connection(conf) {}
  void sendRequest(std::unique_ptr<fuerte::Request> r,
                                fuerte::RequestCallback cb) override {
    _sendRequestNum++;
    auto err = _err;
    _err = fuerte::Error::NoError;  // next time we will report OK
    cb(err, std::move(r), std::move(_response));
  }
  
  std::size_t requestsLeft() const override {
    return 0;
  }
  
  State state() const override {
    return _state;
  }
  
  void cancel() override {}

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
    network::ConnectionPool::Config config(server.getFeature<MetricsFeature>());
    config.clusterInfo = &server.getFeature<ClusterFeature>().clusterInfo();
    config.numIOThreads = 1;
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
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusAccepted);
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
  ASSERT_FALSE(res.hasResponse());
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
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusAccepted);
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
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusAccepted);
}

TEST_F(NetworkMethodsTest, automatic_retry_when_from_pool) {
  // We first create a good connection and leave it in the pool:
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
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusAccepted);

  // Now the connection is in the pool and is good
  // Let's set the returned error to fuerte::Error::ConnectionClosed as if
  // the connection has gone stale:
  pool->_conn->_err = fuerte::Error::ConnectionClosed;

  // Now try again, it is supposed to work without error, since the
  // automatic retry of the stale connection should create a new connection
  // (which in this test will be the same connection which has repaired
  // itself, LOL).
  b = VPackParser::fromJson("{\"error\":false}");
  resBuffer = b->steal();
  fuerte::ResponseHeader header2;
  header2.responseCode = fuerte::StatusAccepted;
  header2.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response = std::make_unique<fuerte::Response>(std::move(header2));
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  f = network::sendRequest(pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get,
                           "/", buffer, reqOpts);

  res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
}

