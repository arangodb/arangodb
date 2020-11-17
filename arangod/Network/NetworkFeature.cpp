////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Network/ConnectionPool.h"
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
}  // namespace

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

NetworkFeature::NetworkFeature(application_features::ApplicationServer& server)
    : NetworkFeature(server, network::ConnectionPool::Config{}) {
  this->_numIOThreads = 1; // override default
}

NetworkFeature::NetworkFeature(application_features::ApplicationServer& server,
                               network::ConnectionPool::Config config)
    : ApplicationFeature(server, "Network"),
      _maxOpenConnections(config.maxOpenConnections),
      _idleTtlMilli(config.idleConnectionMilli),
      _numIOThreads(config.numIOThreads),
      _verifyHosts(config.verifyHosts),
      _prepared(false),
      _forwardedRequests(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_network_forwarded_requests", 0, "Number of requests forwarded to another coordinator")) {
  setOptional(true);
  startsAfter<ClusterFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<ServerFeature>();
  startsAfter<EngineSelectorFeature>();
}

void NetworkFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("network", "Configure cluster-internal networking");

  options->addOption("--network.io-threads", "number of network IO threads",
                     new UInt32Parameter(&_numIOThreads),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer,
                                                  arangodb::options::Flags::OnAgent,
                                                  arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  options->addOption("--network.max-open-connections",
                     "max open network connections",
                     new UInt64Parameter(&_maxOpenConnections),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer,
                                                  arangodb::options::Flags::OnAgent,
                                                  arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  options->addOption("--network.idle-connection-ttl",
                     "default time-to-live of idle connections (in milliseconds)",
                     new UInt64Parameter(&_idleTtlMilli),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer,
                                                  arangodb::options::Flags::OnAgent,
                                                  arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  options->addOption("--network.verify-hosts", "verify hosts when using TLS",
                     new BooleanParameter(&_verifyHosts),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer,
                                                  arangodb::options::Flags::OnAgent,
                                                  arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30600);

  std::unordered_set<std::string> protos = {
      "", "http", "http2", "h2", "vst"};

  options->addOption("--network.protocol", "network protocol to use",
                     new DiscreteValuesParameter<StringParameter>(&_protocol, protos),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoComponents,
                                                  arangodb::options::Flags::OnCoordinator,
                                                  arangodb::options::Flags::OnDBServer,
                                                  arangodb::options::Flags::OnAgent,
                                                  arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30700);
}

void NetworkFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  _numIOThreads = std::max<unsigned>(1, std::min<unsigned>(_numIOThreads, 8));
  if (_maxOpenConnections < 8) {
    _maxOpenConnections = 8;
  }
  if (_idleTtlMilli < 10000) {
    _idleTtlMilli = 10000;
  }

  // force internal protocol to HTTP.
  // this restriction can be lifted later once we are
  // done with investigating potential issues in fuerte VST and H2.
  if (_protocol != "http" && _protocol != "") {
    _protocol = "http";
    LOG_TOPIC("bea9e", WARN, Logger::COMMUNICATION) << "value for '--network.protocol' forced to 'http'";
  }
}

void NetworkFeature::prepare() {
  ClusterInfo* ci = nullptr;
  if (server().hasFeature<ClusterFeature>() && server().isEnabled<ClusterFeature>()) {
     ci = &server().getFeature<ClusterFeature>().clusterInfo();
  }
  
  network::ConnectionPool::Config config;
  config.numIOThreads = static_cast<unsigned>(_numIOThreads);
  config.maxOpenConnections = _maxOpenConnections;
  config.idleConnectionMilli = _idleTtlMilli;
  config.verifyHosts = _verifyHosts;
  config.clusterInfo = ci;
  config.name = "ClusterComm";

  if (_protocol == "http") {
    config.protocol = fuerte::ProtocolType::Http;
  } else if (_protocol == "http2" || _protocol == "h2") {
    config.protocol = fuerte::ProtocolType::Http2;
  } else if (_protocol == "vst") {
    config.protocol = fuerte::ProtocolType::Vst;
  } else {
    config.protocol = fuerte::ProtocolType::Http;
  }
  
  // currently the protocol is restricted to HTTP for internal
  // messages. this restriction can be lifted later once we are
  // done with investigating potential issues in fuerte VST and H2.
  TRI_ASSERT(config.protocol == fuerte::ProtocolType::Http);

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

}  // namespace arangodb
