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

#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Network/ConnectionPool.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"

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

std::atomic<network::ConnectionPool*> NetworkFeature::_poolPtr(nullptr);

NetworkFeature::NetworkFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Network"),
      _numIOThreads(1),
      _maxOpenConnections(128),
      _connectionTtlMilli(5 * 60 * 1000),
      _verifyHosts(false) {
  setOptional(true);
  startsAfter("Server");
  startsAfter("Scheduler");
}

void NetworkFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("--network", "Networking ");

  options->addOption("--network.io-threads", "number of network IO threads",
                     new UInt32Parameter(&_numIOThreads));
  options->addOption("--network.max-open-connections",
                     "max open network connections",
                     new UInt64Parameter(&_maxOpenConnections));
  options->addOption("--network.connection-ttl",
                     "default time-to-live of connections (in milliseconds)",
                     new UInt64Parameter(&_connectionTtlMilli));
  options->addOption("--network.verify-hosts", "verify hosts when using TLS",
                     new BooleanParameter(&_verifyHosts));

  _gcfunc = [this](bool canceled) {
    if (canceled) {
      return;
    }

    _pool->pruneConnections();

    auto* ci = ClusterInfo::instance();
    if (ci != nullptr) {
      auto failed = ci->getFailedServers();
      for (ServerID const& f : failed) {
        _pool->cancelConnections(f);
      }
    }

    if (!application_features::ApplicationServer::isStopping() && !canceled) {
      auto off = std::chrono::seconds(3);
      ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc, off);
    }
  };
}

void NetworkFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  _numIOThreads = std::min<unsigned>(1, std::max<unsigned>(_numIOThreads, 8));
  if (_maxOpenConnections < 8) {
    _maxOpenConnections = 8;
  }
  if (_connectionTtlMilli < 10000) {
    _connectionTtlMilli = 10000;
  }
}

void NetworkFeature::prepare() {
  network::ConnectionPool::Config config;
  config.numIOThreads = static_cast<unsigned>(_numIOThreads);
  config.maxOpenConnections = _maxOpenConnections;
  config.connectionTtlMilli = _connectionTtlMilli;
  config.verifyHosts = _verifyHosts;

  _pool = std::make_unique<network::ConnectionPool>(config);
  _poolPtr.store(_pool.get(), std::memory_order_release);
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
  _poolPtr.store(nullptr, std::memory_order_release);
  if (_pool) {
    _pool->shutdown();
  }
}

void NetworkFeature::stop() {
  // we might have posted another workItem during shutdown.
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
}

}  // namespace arangodb
