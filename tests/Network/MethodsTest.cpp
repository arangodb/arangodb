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

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Cluster/ClusterFeature.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "gtest/gtest.h"

using namespace arangodb;

struct DummyConnection final : public fuerte::Connection {
  DummyConnection(fuerte::detail::ConnectionConfiguration const& conf)
      : fuerte::Connection(conf) {}
  void sendRequest(std::unique_ptr<fuerte::Request> r,
                   fuerte::RequestCallback cb) override {
    _sendRequestNum++;
    cb(_err, std::move(r), std::move(_response));
    if (_err == fuerte::Error::WriteError ||
        _err == fuerte::Error::ConnectionClosed) {
      _state = fuerte::Connection::State::Closed;
    }
  }

  std::size_t requestsLeft() const override { return 0; }

  State state() const override { return _state; }

  void cancel() override {}

  fuerte::Connection::State _state = fuerte::Connection::State::Connected;

  fuerte::Error _err = fuerte::Error::NoError;
  std::unique_ptr<fuerte::Response> _response;
  int _sendRequestNum = 0;
};

struct DummyPool : public network::ConnectionPool {
  DummyPool(network::ConnectionPool::Config const& c)
      : network::ConnectionPool(c),
        _conn(std::make_shared<DummyConnection>(
            fuerte::detail::ConnectionConfiguration())),
        _pooledConnection(std::make_shared<DummyConnection>(
            fuerte::detail::ConnectionConfiguration())) {}

  std::shared_ptr<fuerte::Connection> createConnection(
      fuerte::ConnectionBuilder&) override {
    LOG_DEVEL << "Reconnecting";
    if (_handOutPooledConnectionNext) {
      _handOutPooledConnectionNext = false;
      // We only hand out connected connections
      _pooledConnection->_state = fuerte::Connection::State::Connected;
      return _pooledConnection;
    }
    // We only hand out connected connections
    _conn->_state = fuerte::Connection::State::Connected;
    return _conn;
  }

  void prepareGoodConnectionInPool() {
    _handOutPooledConnectionNext = true;
    // Setup a good response
    injectGoodResponseIntoConnection(_pooledConnection.get());
  }

  void prepareGoodConnection() {
    // Setup a good response
    injectGoodResponseIntoConnection(_conn.get());
  }

  void breakPooledConnection(fuerte::Error error) {
    // The response was collected
    ASSERT_EQ(_pooledConnection->_response.get(), nullptr);
    ASSERT_EQ(_pooledConnection->_sendRequestNum, 1);
    _pooledConnection->_err = error;
  }

  std::shared_ptr<DummyConnection> _conn;

 private:
  void injectGoodResponseIntoConnection(DummyConnection* conn) {
    fuerte::ResponseHeader header;
    header.responseCode = fuerte::StatusAccepted;
    header.contentType(fuerte::ContentType::VPack);
    std::shared_ptr<VPackBuilder> b =
        VPackParser::fromJson("{\"error\":false}");
    auto resBuffer = b->steal();

    conn->_err = fuerte::Error::NoError;
    conn->_state = fuerte::Connection::State::Connected;
    conn->_response = std::make_unique<fuerte::Response>(std::move(header));
    conn->_response->setPayload(std::move(*resBuffer), 0);
  }

 private:
  bool _handOutPooledConnectionNext{false};
  std::shared_ptr<DummyConnection> _pooledConnection;
};

struct NetworkMethodsTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::THREADS,
                                            arangodb::LogLevel::FATAL> {
  NetworkMethodsTest() : server(false) {
    server.addFeature<SchedulerFeature>(true);
    server.startFeatures();

    pool = std::make_unique<DummyPool>(config());
  }

  void assertIsPositiveResponse(network::Response const& res) {
    ASSERT_EQ(res.destination, "tcp://example.org:80");
    ASSERT_EQ(res.error, fuerte::Error::NoError)
        << "Got " << fuerte::to_string(res.error) << " expected "
        << fuerte::to_string(fuerte::Error::NoError);
    ASSERT_TRUE(res.hasResponse());
    ASSERT_EQ(res.statusCode(), fuerte::StatusAccepted);
  }

  void setupBrokenConnectionInPool(fuerte::Error error) {
    // We first create a good connection and leave it in the pool:
    pool->prepareGoodConnectionInPool();

    network::RequestOptions reqOpts;
    reqOpts.timeout = network::Timeout(1.0);

    VPackBuffer<uint8_t> buffer;
    auto f =
        network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                  fuerte::RestVerb::Get, "/", buffer, reqOpts);

    network::Response res = std::move(f).get();
    assertIsPositiveResponse(res);

    // Now the connection is in the pool and is good
    // let us make it stale
    pool->breakPooledConnection(error);
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
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
  std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"error\":false}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

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
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::ConnectionClosed);
  ASSERT_FALSE(res.hasResponse());
}

TEST_F(NetworkMethodsTest, request_failure_on_status_not_acceptable) {
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusNotAcceptable;
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusNotAcceptable);
}

TEST_F(NetworkMethodsTest, request_failure_on_timeout) {
  pool->_conn->_err = fuerte::Error::RequestTimeout;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::RequestTimeout);
  ASSERT_FALSE(res.hasResponse());
}

TEST_F(NetworkMethodsTest, request_failure_on_shutdown) {
  pool->_conn->_err = fuerte::Error::NoError;
  auto& server = pool->config().clusterInfo->server();
  server.beginShutdown();

  network::RequestOptions reqOpts;

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusServiceUnavailable);
  VPackSlice body = res.slice();
  VPackSlice errorNum = body.get("errorNum");
  ASSERT_EQ(static_cast<int>(TRI_ERROR_SHUTTING_DOWN),
            errorNum.getNumber<int>());
}

TEST_F(NetworkMethodsTest, request_failure_on_connection_closed) {
  pool->_conn->_err = fuerte::Error::ConnectionClosed;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::ConnectionClosed);
  ASSERT_FALSE(res.hasResponse());
}

TEST_F(NetworkMethodsTest,
       request_automatic_retry_connection_closed_when_from_pool) {
  setupBrokenConnectionInPool(fuerte::Error::ConnectionClosed);

  // Now try again, it is supposed to work without error, since the
  // automatic retry of the stale connection should create a new connection
  // (which will be the alternative connection in our DummyPool)
  pool->prepareGoodConnection();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  auto res = std::move(f).get();
  assertIsPositiveResponse(res);
}

TEST_F(NetworkMethodsTest, request_automatic_retry_write_error_when_from_pool) {
  setupBrokenConnectionInPool(fuerte::Error::WriteError);

  // Now try again, it is supposed to work without error, since the
  // automatic retry of the stale connection should create a new connection
  // (which will be the alternative connection in our DummyPool)
  pool->prepareGoodConnection();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f = network::sendRequest(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  auto res = std::move(f).get();
  assertIsPositiveResponse(res);
}

TEST_F(NetworkMethodsTest, request_with_retry_after_error) {
  // Step 1: Provoke a connection error
  pool->_conn->_err = fuerte::Error::CouldNotConnect;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(5.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());
  ASSERT_EQ(pool->_conn->_sendRequestNum, 1);

  // Step 2: Now respond with no error
  pool->_conn->_err = fuerte::Error::NoError;

  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusAccepted;
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
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

TEST_F(NetworkMethodsTest, request_with_retry_after_421) {
  // Step 1: Provoke a 421 response
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusMisdirectedRequest;
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(5.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());
  ASSERT_EQ(pool->_conn->_sendRequestNum, 1);

  // Step 2: Now respond with no error
  header.responseCode = fuerte::StatusAccepted;
  header.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
  auto b = VPackParser::fromJson("{\"error\":false}");
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

TEST_F(NetworkMethodsTest, request_with_retry_after_conn_canceled) {
  // Step 1: Provoke a ConnectionCanceled
  pool->_conn->_err = fuerte::Error::ConnectionCanceled;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(5.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());
  ASSERT_EQ(pool->_conn->_sendRequestNum, 1);

  // Step 2: Now respond with no error
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusOK;
  header.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
  auto b = VPackParser::fromJson("{\"error\":false}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);
  pool->_conn->_err = fuerte::Error::NoError;

  auto status = f.wait_for(std::chrono::milliseconds(350));
  ASSERT_EQ(futures::FutureStatus::Ready, status);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusOK);
}

TEST_F(NetworkMethodsTest, request_with_retry_after_not_found_error) {
  // Step 1: Provoke a data source not found error
  pool->_conn->_err = fuerte::Error::NoError;
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusNotFound;
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
  std::shared_ptr<VPackBuilder> b =
      VPackParser::fromJson("{\"errorNum\":1203}");
  auto resBuffer = b->steal();
  pool->_conn->_response->setPayload(std::move(*resBuffer), 0);

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);
  reqOpts.retryNotFound = true;

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  // the default behaviour should be to retry after 200 ms
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  ASSERT_FALSE(f.isReady());

  // Step 2: Now respond with no error
  pool->_conn->_err = fuerte::Error::NoError;

  header.responseCode = fuerte::StatusAccepted;
  header.contentType(fuerte::ContentType::VPack);
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));
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

TEST_F(NetworkMethodsTest, request_with_retry_failure) {
  pool->_conn->_err = fuerte::Error::ConnectionClosed;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::ConnectionClosed);
  ASSERT_FALSE(res.hasResponse());
}

TEST_F(NetworkMethodsTest,
       request_with_retry_failure_on_status_not_acceptable) {
  fuerte::ResponseHeader header;
  header.contentType(fuerte::ContentType::VPack);
  header.responseCode = fuerte::StatusNotAcceptable;
  pool->_conn->_response =
      std::make_unique<fuerte::Response>(std::move(header));

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::NoError);
  ASSERT_TRUE(res.hasResponse());
  ASSERT_EQ(res.statusCode(), fuerte::StatusNotAcceptable);
}

TEST_F(NetworkMethodsTest, request_with_retry_failure_on_timeout) {
  pool->_conn->_err = fuerte::Error::RequestTimeout;

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  network::Response res = std::move(f).get();
  ASSERT_EQ(res.destination, "tcp://example.org:80");
  ASSERT_EQ(res.error, fuerte::Error::RequestTimeout);
  ASSERT_FALSE(res.hasResponse());
}

TEST_F(NetworkMethodsTest,
       request_with_retry_automatic_retry_connection_closed_when_from_pool) {
  setupBrokenConnectionInPool(fuerte::Error::ConnectionClosed);

  // Now try again, it is supposed to work without error, since the
  // automatic retry of the stale connection should create a new connection
  // (which will be the alternative connection in our DummyPool)
  pool->prepareGoodConnection();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  auto res = std::move(f).get();
  assertIsPositiveResponse(res);
}

TEST_F(NetworkMethodsTest,
       request_with_retry_automatic_retry_write_error_when_from_pool) {
  setupBrokenConnectionInPool(fuerte::Error::WriteError);

  // Now try again, it is supposed to work without error, since the
  // automatic retry of the stale connection should create a new connection
  // (which will be the alternative connection in our DummyPool)
  pool->prepareGoodConnection();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(60.0);

  VPackBuffer<uint8_t> buffer;
  auto f =
      network::sendRequestRetry(pool.get(), "tcp://example.org:80",
                                fuerte::RestVerb::Get, "/", buffer, reqOpts);

  auto res = std::move(f).get();
  assertIsPositiveResponse(res);
}
