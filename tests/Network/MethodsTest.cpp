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

#include "catch.hpp"

#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"

#include "ApplicationFeatures/GreetingsPhase.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Scheduler.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

struct SchedulerTestSetup {
  arangodb::application_features::ApplicationServer server;

  SchedulerTestSetup() : server(nullptr, nullptr) {
    using namespace arangodb::application_features;
    std::vector<ApplicationFeature*> features;

    features.emplace_back(new GreetingsFeaturePhase(server, false));
    features.emplace_back(new arangodb::FileDescriptorsFeature(server));
    features.emplace_back(new arangodb::SchedulerFeature(server));

    for (auto& f : features) {
      ApplicationServer::server->addFeature(f);
    }
    ApplicationServer::server->setupDependencies(false);

    ApplicationServer::setStateUnsafe(ServerState::IN_WAIT);
    auto orderedFeatures = server.getOrderedFeatures();
    features[2]->validateOptions(nullptr);
    for (auto& f : orderedFeatures) {
      f->prepare();
    }
    for (auto& f : orderedFeatures) {
      f->start();
    }
  }

  ~SchedulerTestSetup() {
    using namespace arangodb::application_features;
    ApplicationServer::setStateUnsafe(ServerState::IN_STOP);

    auto orderedFeatures = server.getOrderedFeatures();
    for (auto& f : orderedFeatures) {
      f->beginShutdown();
    }
    for (auto& f : orderedFeatures) {
      f->stop();
    }
    for (auto& f : orderedFeatures) {
      f->unprepare();
    }

    arangodb::application_features::ApplicationServer::server = nullptr;
   }
  
    std::vector<std::unique_ptr<arangodb::application_features::ApplicationFeature*>> features;
 };

struct DummyConnection final : public fuerte::Connection {
  DummyConnection(fuerte::detail::ConnectionConfiguration const& conf) : fuerte::Connection(conf) {}
  fuerte::MessageID sendRequest(std::unique_ptr<fuerte::Request> r,
                        fuerte::RequestCallback cb) override {
    _sendRequestNum++;
    cb(_err, std::move(r), std::move(_response));
    return 0;
  }
  
  std::size_t requestsLeft() const override {
    return 1;
  }
  
  State state() const override {
    return _state;
  }
  
  void cancel() override {}
  void startConnection() override {}
  
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

TEST_CASE("network::Methods", "[network]") {
  network::ConnectionPool::Config config;
  config.numIOThreads = 1;
  config.minOpenConnections = 1;
  config.maxOpenConnections = 3;
  config.verifyHosts = false;
  
  DummyPool pool(config);
  NetworkFeature::setPoolTesting(&pool);
  
  std::this_thread::sleep_for(std::chrono::seconds(1));
  
  SECTION("simple request") {
        
    pool._conn->_err = fuerte::Error::NoError;
    
    fuerte::ResponseHeader header;
    header.responseCode = fuerte::StatusAccepted;
    header.contentType(fuerte::ContentType::VPack);
    pool._conn->_response = std::make_unique<fuerte::Response>(std::move(header));
    std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"error\":false}");
    auto resBuffer = b->steal();
    pool._conn->_response->setPayload(*(std::move(resBuffer).get()), 0);
    
    VPackBuffer<uint8_t> buffer;
    auto f = network::sendRequest("tcp://example.org:80", fuerte::RestVerb::Get, "/",
                                  buffer, network::Timeout(60.0));
    
    network::Response res = std::move(f).get();
    CHECK(res.destination == "tcp://example.org:80");
    CHECK(res.error == fuerte::Error::NoError);
    CHECK(res.response != nullptr);
    REQUIRE(res.response->statusCode() == fuerte::StatusAccepted);
  }
  
  SECTION("request failure") {
    
    pool._conn->_err = fuerte::Error::ConnectionClosed;
    
    VPackBuffer<uint8_t> buffer;
    auto f = network::sendRequest("tcp://example.org:80", fuerte::RestVerb::Get, "/",
                                  buffer, network::Timeout(60.0));
    
    network::Response res = std::move(f).get();
    CHECK(res.destination == "tcp://example.org:80");
    CHECK(res.error == fuerte::Error::ConnectionClosed);
    REQUIRE(res.response == nullptr);
  }
  
  SECTION("request with retry after error") {
    SchedulerTestSetup setup;
    
    // Step 1: Provoke a connection error
    pool._conn->_err = fuerte::Error::CouldNotConnect;
    
    VPackBuffer<uint8_t> buffer;
    auto f = network::sendRequestRetry("tcp://example.org:80", fuerte::RestVerb::Get, "/",
                                       buffer, network::Timeout(5.0));
    
    // the default behaviour should be to retry after 200 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    REQUIRE(!f.isReady());
    REQUIRE(pool._conn->_sendRequestNum == 1);

    // Step 2: Now respond with no error
    pool._conn->_err = fuerte::Error::NoError;
    
    fuerte::ResponseHeader header;
    header.contentType(fuerte::ContentType::VPack);
    header.responseCode = fuerte::StatusAccepted;
    pool._conn->_response = std::make_unique<fuerte::Response>(std::move(header));
    std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"error\":false}");
    auto resBuffer = b->steal();
    pool._conn->_response->setPayload(*(std::move(resBuffer).get()), 0);
    
    auto status = f.wait_for(std::chrono::milliseconds(350));
    REQUIRE(futures::FutureStatus::Ready == status);
    
    network::Response res = std::move(f).get();
    CHECK(res.destination == "tcp://example.org:80");
    CHECK(res.error == fuerte::Error::NoError);
    REQUIRE(res.response != nullptr);
    CHECK(res.response->statusCode() == fuerte::StatusAccepted);
  }
  
  SECTION("request with retry after not found error") {
    SchedulerTestSetup setup;
    
    // Step 1: Provoke a data source not found error
    pool._conn->_err = fuerte::Error::NoError;
    fuerte::ResponseHeader header;
    header.contentType(fuerte::ContentType::VPack);
    header.responseCode = fuerte::StatusNotFound;
    pool._conn->_response = std::make_unique<fuerte::Response>(std::move(header));
    std::shared_ptr<VPackBuilder> b = VPackParser::fromJson("{\"errorNum\":1203}");
    auto resBuffer = b->steal();
    pool._conn->_response->setPayload(*(std::move(resBuffer).get()), 0);
    
    VPackBuffer<uint8_t> buffer;
    auto f = network::sendRequestRetry("tcp://example.org:80", fuerte::RestVerb::Get, "/",
                                       buffer, network::Timeout(5.0), {}, true);
    
    // the default behaviour should be to retry after 200 ms
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    REQUIRE(!f.isReady());
    
    // Step 2: Now respond with no error
    pool._conn->_err = fuerte::Error::NoError;
    
    header.responseCode = fuerte::StatusAccepted;
    header.contentType(fuerte::ContentType::VPack);
    pool._conn->_response = std::make_unique<fuerte::Response>(std::move(header));
    b = VPackParser::fromJson("{\"error\":false}");
    pool._conn->_response->setPayload(*(b->steal().get()), 0);
    
    auto status = f.wait_for(std::chrono::milliseconds(350));
    CHECK(futures::FutureStatus::Ready == status);
    
    network::Response res = std::move(f).get();
    CHECK(res.destination == "tcp://example.org:80");
    CHECK(res.error == fuerte::Error::NoError);
    REQUIRE(res.response != nullptr);
    CHECK(res.response->statusCode() == fuerte::StatusAccepted);
  }
}

