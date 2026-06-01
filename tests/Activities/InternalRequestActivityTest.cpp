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

#include "Inspection/VPack.h"
#include "Network/RequestActivity.h"
#include "Network/types.h"
#include "fuerte/types.h"
#include "gtest/gtest.h"

#include <fuerte/connection.h>
#include <velocypack/Iterator.h>

#include <chrono>
#include <mutex>
#include <thread>

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

  // Is called internally when network::sendRequest is called.
  // cb is executed after we got a response or error back and finally sets the
  // promise value of the return value of network::sendRequest.
  // Note: with sendRequestRetry this is also called from the NetworkFeature
  // retry thread, hence the lock.
  void sendRequest(std::unique_ptr<fuerte::Request> req,
                   fuerte::RequestCallback cb) override {
    std::lock_guard guard(_mutex);
    _pendingReq = std::move(req);
    _pendingCb = std::move(cb);
  }

  // A NoError result must carry a response: the retry path dereferences it.
  void respondNow(fuerte::Error err,
                  std::unique_ptr<fuerte::Response> response = nullptr) {
    fuerte::RequestCallback cb;
    std::unique_ptr<fuerte::Request> req;
    {
      std::lock_guard guard(_mutex);
      ASSERT_TRUE(static_cast<bool>(_pendingCb))
          << "respondNow() called without a pending request";
      cb = std::move(_pendingCb);
      req = std::move(_pendingReq);
    }
    cb(err, std::move(req), std::move(response));
  }

  auto hasPendingRequest() const -> bool {
    std::lock_guard guard(_mutex);
    return static_cast<bool>(_pendingCb);
  }

  std::size_t requestsLeft() const override {
    return hasPendingRequest() ? 1 : 0;
  }
  State state() const override { return fuerte::Connection::State::Connected; }
  void cancel() override {}
  std::string localEndpoint() override { return "not implemented"; }

 private:
  mutable std::mutex _mutex;
  std::unique_ptr<fuerte::Request> _pendingReq;
  fuerte::RequestCallback _pendingCb;
};

/**
 * Spin until the connection holds a freshly captured callback.
 *
 * Retries are re-issued from the NetworkFeature retry thread, so the test
 * thread has to wait for the next attempt to arrive. Returns false on timeout.
 */
auto waitForPendingRequest(DeferredConnection& conn) -> bool {
  auto const deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (conn.hasPendingRequest()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

auto makeResponse(fuerte::StatusCode code)
    -> std::unique_ptr<fuerte::Response> {
  fuerte::ResponseHeader header;
  header.responseCode = code;
  header.contentType(fuerte::ContentType::VPack);
  return std::make_unique<fuerte::Response>(std::move(header));
}

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

auto requestActivitiesInRegistry()
    -> std::vector<network::RequestActivityData> {
  auto snap = activities::registry.snapshot();
  if (!snap.ok()) {
    return std::vector<network::RequestActivityData>{};
  }
  std::vector<network::RequestActivityData> requestActivities;
  for (auto entry : velocypack::ArrayIterator(snap.get().slice())) {
    auto type = entry.get("type");
    if (type.isString() && type.stringView() == "InternalRequest") {
      auto a = inspection::deserializeWithErrorT<network::RequestActivityData>(
          entry.get("data"));
      if (a.ok()) {
        requestActivities.emplace_back(a.get());
      }
    }
  }
  return requestActivities;
}

}  // namespace

struct InternalRequestActivityTest : ::testing::Test {
  static void SetUpTestSuite() { activities::registry.garbageCollectAll(); }
  static void TearDownTestSuite() {
    activities::registry.garbageCollectAll();
    EXPECT_EQ(activities::registry.size(), 0);
  }

  InternalRequestActivityTest() : server("CRDN_0001", false) {
    server.addFeature<SchedulerFeature>(
        true, server.getFeature<metrics::MetricsFeature>(), sharedPRNG);
    server.startFeatures();
    pool = std::make_unique<DeferredPool>(network::ConnectionPool::Config{
        .metrics = network::ConnectionPool::Metrics::fromMetricsFeature(
            server.getFeature<metrics::MetricsFeature>(), "mock-network"),
        .clusterInfo = &server.getFeature<ClusterFeature>().clusterInfo(),
    });
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

  VPackBuilder payload;
  {
    VPackObjectBuilder b(&payload);
    payload.add("abc", VPackValue(423));
  }
  auto response = network::sendRequest(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Post, "/_api/123",
      *payload.steal(), opts, {{"header1", "abc"}});

  ASSERT_FALSE(response.isReady());
  ASSERT_EQ(
      requestActivitiesInRegistry(),
      (std::vector<network::RequestActivityData>{
          {.destination = "tcp://example.org:80",
           .method = fuerte::RestVerb::Post,
           .path = "/_api/123",
           .hasPayload = true,
           .options = opts,
           .header = std::map<std::string, std::string>{{"header1", "abc"}}}}));
  ASSERT_FALSE(response.isReady());
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_request_is_resolved) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto response = network::sendRequest(pool.get(), "tcp://example.org:80",
                                       fuerte::RestVerb::Get, "/", {}, opts);
  ASSERT_FALSE(response.isReady());

  pool->conn->respondNow(fuerte::Error::NoError);
  ASSERT_TRUE(response.isReady());

  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{}));

  ASSERT_EQ(std::move(response).waitAndGet().error, fuerte::Error::NoError);
}

TEST_F(InternalRequestActivityTest,
       activity_is_present_while_retry_request_is_in_flight) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  VPackBuilder payload;
  {
    VPackObjectBuilder b(&payload);
    payload.add("abc", VPackValue(423));
  }
  auto response = network::sendRequestRetry(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Post, "/_api/123",
      *payload.steal(), opts, {{"header1", "abc"}});

  ASSERT_FALSE(response.isReady());
  ASSERT_EQ(
      requestActivitiesInRegistry(),
      (std::vector<network::RequestActivityData>{
          {.destination = "tcp://example.org:80",
           .method = fuerte::RestVerb::Post,
           .path = "/_api/123",
           .hasPayload = true,
           .options = opts,
           .header = std::map<std::string, std::string>{{"header1", "abc"}},
           .retryCount = 0}}));
  ASSERT_FALSE(response.isReady());
}

TEST_F(
    InternalRequestActivityTest,
    activity_is_present_while_retry_request_continually_fails_with_retryable_error) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto response = network::sendRequestRetry(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get, "/", {}, opts);
  ASSERT_FALSE(response.isReady());

  // Fail it with a retryable error. The production code schedules a retry on
  // the NetworkFeature retry thread (~200ms later); the request is not done.
  pool->conn->respondNow(fuerte::Error::CouldNotConnect);
  ASSERT_FALSE(response.isReady());
  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{
                {.destination = "tcp://example.org:80",
                 .method = fuerte::RestVerb::Get,
                 .path = "/",
                 .hasPayload = false,
                 .options = opts,
                 .header = {},
                 .retryCount = 1}}));

  // Wait for the retry thread to re-issue the request
  ASSERT_TRUE(waitForPendingRequest(*pool->conn));

  // Fail once more
  pool->conn->respondNow(fuerte::Error::CouldNotConnect);
  ASSERT_FALSE(response.isReady());
  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{
                {.destination = "tcp://example.org:80",
                 .method = fuerte::RestVerb::Get,
                 .path = "/",
                 .hasPayload = false,
                 .options = opts,
                 .header = {},
                 .retryCount = 2}}));

  // Wait for the retry thread to re-issue the request
  ASSERT_TRUE(waitForPendingRequest(*pool->conn));
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_retry_request_is_resolved) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto response = network::sendRequestRetry(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get, "/", {}, opts);
  ASSERT_FALSE(response.isReady());

  // Respond with no error: stops retry loop
  pool->conn->respondNow(fuerte::Error::NoError,
                         makeResponse(fuerte::StatusAccepted));
  ASSERT_TRUE(response.isReady());

  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{}));

  ASSERT_EQ(std::move(response).waitAndGet().error, fuerte::Error::NoError);
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_retry_request_is_retried_and_then_resolved) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto response = network::sendRequestRetry(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get, "/", {}, opts);
  ASSERT_FALSE(response.isReady());

  // Start one retry
  pool->conn->respondNow(fuerte::Error::CouldNotConnect);
  // Wait for the retry thread to re-issue the request
  ASSERT_TRUE(waitForPendingRequest(*pool->conn));
  ASSERT_FALSE(response.isReady());

  // Respond with no error: stops retry loop
  pool->conn->respondNow(fuerte::Error::NoError,
                         makeResponse(fuerte::StatusAccepted));
  ASSERT_TRUE(response.isReady());

  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{}));

  ASSERT_EQ(std::move(response).waitAndGet().error, fuerte::Error::NoError);
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_retry_request_finishes_early_with_error) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto response = network::sendRequestRetry(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Get, "/", {}, opts);
  ASSERT_FALSE(response.isReady());

  pool->conn->respondNow(fuerte::Error::ConnectionClosed);
  ASSERT_TRUE(response.isReady());

  ASSERT_EQ(requestActivitiesInRegistry(),
            (std::vector<network::RequestActivityData>{}));

  ASSERT_EQ(std::move(response).waitAndGet().error,
            fuerte::Error::ConnectionClosed);
}
