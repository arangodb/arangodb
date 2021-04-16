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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "NetworkFeature.h"

#include <fuerte/connection.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace {
void queueGarbageCollection(std::mutex& mutex, arangodb::Scheduler::WorkHandle& workItem,
                            std::function<void(bool)>& gcfunc, std::chrono::seconds offset) {
  bool queued = false;
  {
    std::lock_guard<std::mutex> guard(mutex);
    std::tie(queued, workItem) =
        arangodb::basics::function_utils::retryUntilTimeout<arangodb::Scheduler::WorkHandle>(
            [&gcfunc, offset]() -> std::pair<bool, arangodb::Scheduler::WorkHandle> {
              return arangodb::SchedulerFeature::SCHEDULER->queueDelay(arangodb::RequestLane::INTERNAL_LOW,
                                                                       offset, gcfunc);
            },
            arangodb::Logger::COMMUNICATION,
            "queue transaction garbage collection");
  }
  if (!queued) {
    LOG_TOPIC("c8b3d", FATAL, arangodb::Logger::COMMUNICATION)
        << "Failed to queue transaction garbage collection, for 5 minutes, "
           "exiting.";
    FATAL_ERROR_EXIT();
  }
}

constexpr double CongestionRatio = 0.5;
constexpr std::uint64_t MaxAllowedInFlight = 65536;
constexpr std::uint64_t MinAllowedInFlight = 64;
}  // namespace

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

NetworkFeature::NetworkFeature(application_features::ApplicationServer& server)
    : NetworkFeature(server, network::ConnectionPool::Config{server.getFeature<MetricsFeature>()}) {
  this->_numIOThreads = 2; // override default
}

struct NetworkFeatureScale {
  static fixed_scale_t<double> scale() { return { 0.0, 100.0, {1.0, 5.0, 15.0, 50.0} }; }
};

DECLARE_COUNTER(arangodb_network_forwarded_requests_total, "Number of requests forwarded to another coordinator");
DECLARE_COUNTER(arangodb_network_request_timeouts_total, "Number of internal requests that have timed out");
DECLARE_HISTOGRAM(arangodb_network_request_duration_as_percentage_of_timeout, NetworkFeatureScale, "Internal request round-trip time as a percentage of timeout [%]");
DECLARE_GAUGE(arangodb_network_requests_in_flight, uint64_t, "Number of outgoing internal requests in flight");

NetworkFeature::NetworkFeature(application_features::ApplicationServer& server,
                               network::ConnectionPool::Config config)
    : ApplicationFeature(server, "Network"),
      _protocol(),
      _maxOpenConnections(config.maxOpenConnections),
      _idleTtlMilli(config.idleConnectionMilli),
      _numIOThreads(config.numIOThreads),
      _verifyHosts(config.verifyHosts),
      _prepared(false),
      _forwardedRequests(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_network_forwarded_requests_total{})),
      _maxInFlight(::MaxAllowedInFlight),
      _requestsInFlight(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_network_requests_in_flight{})),
      _requestTimeouts(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_network_request_timeouts_total{})),
      _requestDurations(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_network_request_duration_as_percentage_of_timeout{})) {
  setOptional(true);
  startsAfter<ClusterFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<ServerFeature>();
  startsAfter<EngineSelectorFeature>();
}

void NetworkFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("network", "Configure cluster-internal networking");

  options->addOption("--network.io-threads", "number of network IO threads for cluster-internal communication",
                     new UInt32Parameter(&_numIOThreads))
                     .setIntroducedIn(30600);
  options->addOption("--network.max-open-connections",
                     "max open TCP connections for cluster-internal communication per endpoint",
                     new UInt64Parameter(&_maxOpenConnections))
                     .setIntroducedIn(30600);
  options->addOption("--network.idle-connection-ttl",
                     "default time-to-live of idle connections for cluster-internal communication (in milliseconds)",
                     new UInt64Parameter(&_idleTtlMilli))
                     .setIntroducedIn(30600);
  options->addOption("--network.verify-hosts", "verify hosts when using TLS in cluster-internal communication",
                     new BooleanParameter(&_verifyHosts))
                     .setIntroducedIn(30600);

  std::unordered_set<std::string> protos = {
      "", "http", "http2", "h2", "vst"};

  options->addOption("--network.protocol", "network protocol to use for cluster-internal communication",
                     new DiscreteValuesParameter<StringParameter>(&_protocol, protos))
                     .setIntroducedIn(30700);

  options
      ->addOption("--network.max-requests-in-flight",
                  "controls the number of internal requests that can be in "
                  "flight at a given point in time",
                  new options::UInt64Parameter(&_maxInFlight),
                  options::makeDefaultFlags(options::Flags::Hidden))
      .setIntroducedIn(30800);
}

void NetworkFeature::validateOptions(std::shared_ptr<options::ProgramOptions> opts) {
  _numIOThreads = std::max<unsigned>(1, std::min<unsigned>(_numIOThreads, 8));
  if (_maxOpenConnections < 8) {
    _maxOpenConnections = 8;
  }
  if (!opts->processingResult().touched("--network.idle-connection-ttl")) {
    auto& gs = server().getFeature<GeneralServerFeature>();
    _idleTtlMilli = uint64_t(gs.keepAliveTimeout() * 1000 / 2);
  }
  if (_idleTtlMilli < 10000) {
    _idleTtlMilli = 10000;
  }

  uint64_t clamped = std::clamp(_maxInFlight, ::MinAllowedInFlight, ::MaxAllowedInFlight);
  if (clamped != _maxInFlight) {
    LOG_TOPIC("38cd1", WARN, Logger::CONFIG)
        << "Must set --network.max-requests-in-flight between " << ::MinAllowedInFlight
        << " and " << ::MaxAllowedInFlight << ", clamping value";
    _maxInFlight = clamped;
  }
}

void NetworkFeature::prepare() {
  ClusterInfo* ci = nullptr;
  if (server().hasFeature<ClusterFeature>() && server().isEnabled<ClusterFeature>()) {
     ci = &server().getFeature<ClusterFeature>().clusterInfo();
  }

  network::ConnectionPool::Config config(server().getFeature<MetricsFeature>());
  config.numIOThreads = static_cast<unsigned>(_numIOThreads);
  config.maxOpenConnections = _maxOpenConnections;
  config.idleConnectionMilli = _idleTtlMilli;
  config.verifyHosts = _verifyHosts;
  config.clusterInfo = ci;
  config.name = "ClusterComm";

  config.protocol = fuerte::ProtocolType::Http;
  if (_protocol == "http" || _protocol == "h1") {
    config.protocol = fuerte::ProtocolType::Http;
  } else if (_protocol == "http2" || _protocol == "h2") {
    config.protocol = fuerte::ProtocolType::Http2;
  } else if (_protocol == "vst") {
    config.protocol = fuerte::ProtocolType::Vst;
  }

  _pool = std::make_unique<network::ConnectionPool>(config);
  _poolPtr.store(_pool.get(), std::memory_order_relaxed);

  _gcfunc =
    [this, ci](bool canceled) {
      if (canceled) {
        return;
      }

      _pool->pruneConnections();

      if (ci != nullptr) {
        auto failed = ci->getFailedServers();
        for (ServerID const& srvId : failed) {
          std::string endpoint = ci->getServerEndpoint(srvId);
          size_t n = _pool->cancelConnections(endpoint);
          LOG_TOPIC_IF("15d94", INFO, Logger::COMMUNICATION, n > 0)
            << "canceling " << n << " connection(s) to failed server '"
            << srvId << "' on endpoint '" << endpoint << "'";
        }
      }

      if (!server().isStopping() && !canceled) {
        std::chrono::seconds off(12);
        ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc, off);
      }
    };

  _prepared = true;
}

void NetworkFeature::start() {
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {  // is nullptr in catch tests
    auto off = std::chrono::seconds(1);
    ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc, off);
  }
}


void NetworkFeature::beginShutdown() {
  {
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }
  _poolPtr.store(nullptr, std::memory_order_relaxed);
  if (_pool) {  // first cancel all connections
    _pool->shutdownConnections();
  }
}

void NetworkFeature::stop() {
  {
    // we might have posted another workItem during shutdown.
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }
  if (_pool) {
    _pool->shutdownConnections();
  }
}

void NetworkFeature::unprepare() {
  if (_pool) {
    _pool->drainConnections();
  }
}

arangodb::network::ConnectionPool* NetworkFeature::pool() const {
  return _poolPtr.load(std::memory_order_relaxed);
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void NetworkFeature::setPoolTesting(arangodb::network::ConnectionPool* pool) {
  _poolPtr.store(pool, std::memory_order_release);
}
#endif

bool NetworkFeature::prepared() const {
  return _prepared;
}

void NetworkFeature::trackForwardedRequest() {
  ++_forwardedRequests;
}

std::size_t NetworkFeature::requestsInFlight() const {
  return _requestsInFlight.load();
}

bool NetworkFeature::isCongested() const {
  return _requestsInFlight.load() >= (_maxInFlight * ::CongestionRatio);
}

bool NetworkFeature::isSaturated() const {
  return _requestsInFlight.load() >= _maxInFlight;
}

void NetworkFeature::sendRequest(network::ConnectionPool& pool,
                                 network::RequestOptions const& options,
                                 std::string const& endpoint,
                                 std::unique_ptr<fuerte::Request>&& req,
                                 RequestCallback&& cb) {
  TRI_ASSERT(req != nullptr);
  prepareRequest(pool, req);
  bool isFromPool = false;
  auto conn = pool.leaseConnection(endpoint, isFromPool);
  conn->sendRequest(std::move(req),
                    [this, &pool, isFromPool,
                     cb = std::move(cb)](fuerte::Error err,
                                         std::unique_ptr<fuerte::Request> req,
                                         std::unique_ptr<fuerte::Response> res) {
                      TRI_ASSERT(req != nullptr);
                      finishRequest(pool, err, req, res);
                      TRI_ASSERT(req != nullptr);
                      cb(err, std::move(req), std::move(res), isFromPool);
                    });
}

void NetworkFeature::prepareRequest(network::ConnectionPool const& pool,
                                    std::unique_ptr<fuerte::Request>& req) {
  _requestsInFlight += 1;
  if (req) {
    req->timestamp(std::chrono::steady_clock::now());
  }
}
void NetworkFeature::finishRequest(network::ConnectionPool const& pool, fuerte::Error err,
                                   std::unique_ptr<fuerte::Request> const& req,
                                   std::unique_ptr<fuerte::Response>& res) {
  _requestsInFlight -= 1;
  if (err == fuerte::Error::RequestTimeout) {
    _requestTimeouts.count();
  } else if (req && res) {
    res->timestamp(std::chrono::steady_clock::now());
    std::chrono::milliseconds duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(res->timestamp() -
                                                              req->timestamp());
    std::chrono::milliseconds timeout = req->timeout();
    TRI_ASSERT(timeout.count() > 0);
    if (timeout.count() > 0) {
      // only go in here if we are sure to not divide by zero 
      double percentage = std::clamp(100.0 * static_cast<double>(duration.count()) /
                                         static_cast<double>(timeout.count()),
                                     0.0, 100.0);
      _requestDurations.count(percentage);
    } else {
      // the timeout value was 0, for whatever reason. this is unexpected,
      // but we must not make the program crash here.
      // so instead log a warning and interpret this as a request that took
      // 100% of the timeout duration.
      _requestDurations.count(100.0);
      LOG_TOPIC("1688c", WARN, Logger::FIXME) 
          << "encountered invalid 0s timeout for internal request to path " 
          << req->header.path;
    }
  }
}

}  // namespace arangodb
