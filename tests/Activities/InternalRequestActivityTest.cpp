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

// namespace arangodb::network {
// bool operator==(RequestActivityData const& a, RequestActivityData const& b) {
//   return a.destination == b.destination && a.method == b.method &&
//          a.path == b.path &&
//          (a.payload.size() == b.payload.size() &&
//           std::memcmp(a.payload.data(), b.payload.data(), a.payload.size()));
// }
// }  // namespace arangodb::network

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

struct RequestFilter {
  std::optional<network::DestinationId> destination;
  std::optional<fuerte::RestVerb> method;
  std::optional<std::string> path;
  std::optional<bool> hasPayload;
  std::optional<network::RequestOptions> options;
  std::optional<network::Headers> header;
};
struct Ok {
  bool operator==(Ok const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, Ok& x) {
  return f.object(x).fields();
}
using Err = std::vector<network::RequestActivityData>;
struct Res : std::variant<Ok, Err> {
  bool operator==(Res const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, Res& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::type<Ok>("Ok"), inspection::type<Err>("Error"));
}
void PrintTo(const Res& res, std::ostream* os) { *os << inspection::json(res); }
auto registryContainsRequestActivity(RequestFilter&& filter = RequestFilter{})
    -> Res {
  auto activities = requestActivitiesInRegistry();
  for (auto const& a : activities) {
    if ((!filter.destination.has_value() ||
         a.destination == filter.destination.value())                         //
        && (!filter.method.has_value() || a.method == filter.method.value())  //
        && (!filter.path.has_value() || a.path == filter.path.value())        //
        && (!filter.hasPayload.has_value() ||
            a.hasPayload == filter.hasPayload.value())  //
        &&
        (!filter.options.has_value() || a.options == filter.options.value())  //
        && (!filter.header.has_value() || a.header == filter.header.value())) {
      return {Ok{}};
    }
  }
  // LOG_DEVEL << inspection::json(activities);
  return {Err{activities}};
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
    activities::registry.garbageCollect();
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

  VPackBuilder payload;
  {
    VPackObjectBuilder b(&payload);
    payload.add("abc", VPackValue(423));
  }
  auto future = network::sendRequest(
      pool.get(), "tcp://example.org:80", fuerte::RestVerb::Post, "/_api/123",
      *payload.steal(), opts, {{"header1", "abc"}});

  ASSERT_FALSE(future.isReady());
  EXPECT_EQ(
      registryContainsRequestActivity(RequestFilter{
          .destination = "tcp://example.org:80",
          .method = fuerte::RestVerb::Post,
          .path = "/_api/123",
          .hasPayload = true,
          .options = opts,
          .header = std::map<std::string, std::string>{{"header1", "abc"}}}),
      Res{Ok{}});
  ASSERT_FALSE(future.isReady());
}

TEST_F(InternalRequestActivityTest,
       activity_is_gone_after_request_is_resolved) {
  auto opts = network::RequestOptions{.skipScheduler = true};

  auto future = network::sendRequest(pool.get(), "tcp://example.org:80",
                                     fuerte::RestVerb::Get, "/", {}, opts);

  ASSERT_EQ(registryContainsRequestActivity(
                RequestFilter{.destination = "tcp://example.org:80",
                              .method = fuerte::RestVerb::Get,
                              .path = "/",
                              .hasPayload = false,
                              .options = opts,
                              .header = {}}),
            Res{Ok{}});

  pool->conn->respondNow(fuerte::Error::NoError);
  ASSERT_TRUE(future.isReady());

  activities::registry.garbageCollect();
  EXPECT_EQ(registryContainsRequestActivity(), Res{Err{}});

  ASSERT_EQ(std::move(future).waitAndGet().error, fuerte::Error::NoError);
}
