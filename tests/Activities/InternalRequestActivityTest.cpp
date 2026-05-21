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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <fuerte/connection.h>
#include <velocypack/Iterator.h>

#include "Mocks/Servers.h"

#include "Activities/Registry.h"
#include "Activities/RegistryGlobalVariable.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Metrics/MetricsFeature.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/RequestOptions.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;

namespace {

/**
 * Fuerte connection that captures the request callback without firing it.
 *
 * The test triggers the response explicitly via respondNow(...), which lets
 * us observe the state of the system while a request is in flight.
 */
struct DeferredConnection final : fuerte::Connection {
  DeferredConnection()
      : fuerte::Connection(fuerte::detail::ConnectionConfiguration{}) {}

  // is called internally when network::sendRequest is called
  // cb is executed after we got a response or error back and finally sets the
  // promise value of network::sendRequest
  void sendRequest(std::unique_ptr<fuerte::Request> req,
                   fuerte::RequestCallback cb) override {
    _pendingReq = std::move(req);
    _pendingCb = std::move(cb);
  }

  void respondNow(fuerte::Error err) {
    ASSERT_TRUE(static_cast<bool>(_pendingCb))
        << "respondNow() called without a pending request";
    auto cb = std::move(_pendingCb);
    cb(err, std::move(_pendingReq), nullptr);
  }

  std::size_t requestsLeft() const override { return _pendingCb ? 1 : 0; }
  State state() const override { return fuerte::Connection::State::Connected; }
  void cancel() override {}
  std::string localEndpoint() override { return "not implemented"; }

 private:
  std::unique_ptr<fuerte::Request> _pendingReq;
  fuerte::RequestCallback _pendingCb;
};

struct DeferredPool final : network::ConnectionPool {
  explicit DeferredPool(network::ConnectionPool::Config const& cfg)
      : network::ConnectionPool(cfg),
        conn(std::make_shared<DeferredConnection>()) {}

  std::shared_ptr<fuerte::Connection> createConnection(
      fuerte::ConnectionBuilder&) override {
    return conn;
  }

  std::shared_ptr<DeferredConnection> conn;
};

auto registryContainsActivityOfType(std::string_view typeName) -> bool {
  auto snap = activities::registry.snapshot();
  if (!snap.ok()) {
    return false;
  }
  for (auto entry : velocypack::ArrayIterator(snap.get().slice())) {
    auto type = entry.get("type");
    if (type.isString() && type.stringView() == typeName) {
      return true;
    }
  }
  return false;
}

}  // namespace

struct InternalRequestActivityTest : ::testing::Test {
  InternalRequestActivityTest() : server("CRDN_0001", false) {
    activities::Registry::setCurrentlyExecutingActivity(activities::Root);
    server.addFeature<SchedulerFeature>(
        true, server.getFeature<metrics::MetricsFeature>(), sharedPRNG);
    server.startFeatures();
    pool = std::make_unique<DeferredPool>(network::ConnectionPool::Config{
        .metrics = network::ConnectionPool::Metrics::fromMetricsFeature(
            server.getFeature<metrics::MetricsFeature>(), "mock-network"),
        .clusterInfo = &server.getFeature<ClusterFeature>().clusterInfo(),
    });
  }

  ~InternalRequestActivityTest() override {
    activities::registry.garbageCollect();
  }

  basics::SharedPRNG sharedPRNG;
  tests::mocks::MockCoordinator server;
  std::unique_ptr<DeferredPool> pool;
};

TEST_F(InternalRequestActivityTest,
       activity_is_present_while_request_is_in_flight) {
  // Resolve the promise on the calling thread instead of bouncing through the
  // scheduler — without this, the future may not be ready by the time we
  // garbage-collect, and we'd race the scheduler thread.
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto future = network::sendRequest(pool.get(), "tcp://example.org:80",
                                     fuerte::RestVerb::Get, "/", {}, opts);

  ASSERT_FALSE(future.isReady());
  EXPECT_TRUE(registryContainsActivityOfType("InternalRequestActivity"));
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_request_is_resolved) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto future = network::sendRequest(pool.get(), "tcp://example.org:80",
                                     fuerte::RestVerb::Get, "/", {}, opts);

  ASSERT_TRUE(registryContainsActivityOfType("InternalRequestActivity"));

  pool->conn->respondNow(fuerte::Error::NoError);

  activities::registry.garbageCollect();
  EXPECT_FALSE(registryContainsActivityOfType("InternalRequestActivity"));

  ASSERT_EQ(std::move(future).waitAndGet().error, fuerte::Error::NoError);
}
