////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "CacheManagerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/SharedPRNGFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/Manager.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::cache;
using namespace arangodb::options;

namespace arangodb {

CacheManagerFeature::CacheManagerFeature(Server& server)
    : ArangodFeature{server, *this},
      _manager(nullptr),
      _rebalancer(nullptr),
      _cacheSize(
          (PhysicalMemory::getValue() >= (static_cast<std::uint64_t>(4) << 30))
              ? static_cast<std::uint64_t>(
                    (PhysicalMemory::getValue() -
                     (static_cast<std::uint64_t>(2) << 30)) *
                    0.25)
              : (256 << 20)),
      _rebalancingInterval(static_cast<std::uint64_t>(2 * 1000 * 1000)) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
}

CacheManagerFeature::~CacheManagerFeature() = default;

void CacheManagerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("cache", "in-memory hash cache");

  options
      ->addOption("--cache.size",
                  "The global size limit for all caches (in bytes).",
                  new UInt64Parameter(&_cacheSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Dynamic))
      .setLongDescription(R"(The global caching system, all caches, and all the
data contained therein are constrained to this limit.

If there is less than 4 GiB of RAM in the system, default value is 256 MiB.
If there is more, the default is `(system RAM size - 2 GiB) * 0.25`.)");

  options
      ->addOption(
          "--cache.rebalancing-interval",
          "The time between cache rebalancing attempts (in microseconds). "
          "The minimum value is 500000 (0.5 seconds).",
          new UInt64Parameter(
              &_rebalancingInterval, /*base*/ 1,
              /*minValue*/ CacheManagerFeature::minRebalancingInterval))
      .setLongDescription(R"(The server uses a cache system which pools memory
across many different cache tables. In order to provide intelligent internal
memory management, the system periodically reclaims memory from caches which are
used less often and reallocates it to caches which get more activity.)");
}

void CacheManagerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_cacheSize > 0 && _cacheSize < Manager::kMinSize) {
    LOG_TOPIC("75778", FATAL, arangodb::Logger::FIXME)
        << "invalid value for `--cache.size', need at least "
        << Manager::kMinSize;
    FATAL_ERROR_EXIT();
  }
}

void CacheManagerFeature::start() {
  if (ServerState::instance()->isAgent() || _cacheSize == 0) {
    // we intentionally do not activate the cache on an agency node, as it
    // is not needed there
    return;
  }

  auto scheduler = SchedulerFeature::SCHEDULER;
  auto postFn = [scheduler](std::function<void()> fn) -> bool {
    if (scheduler->server().isStopping()) {
      return false;
    }
    try {
      scheduler->queue(RequestLane::INTERNAL_LOW, std::move(fn));
      return true;
    } catch (...) {
      return false;
    }
  };

  SharedPRNGFeature& sharedPRNG = server().getFeature<SharedPRNGFeature>();
  _manager =
      std::make_unique<Manager>(sharedPRNG, std::move(postFn), _cacheSize);

  _rebalancer = std::make_unique<CacheRebalancerThread>(
      server(), _manager.get(), _rebalancingInterval);
  if (!_rebalancer->start()) {
    LOG_TOPIC("13895", FATAL, Logger::STARTUP)
        << "cache manager startup failed";
    FATAL_ERROR_EXIT();
  }
  LOG_TOPIC("13894", DEBUG, Logger::STARTUP) << "cache manager has started";
}

void CacheManagerFeature::beginShutdown() {
  if (_manager != nullptr) {
    _rebalancer->beginShutdown();
    _manager->beginShutdown();
  }
}

void CacheManagerFeature::stop() {
  if (_manager != nullptr) {
    _rebalancer->shutdown();
    _manager->shutdown();
  }
}

cache::Manager* CacheManagerFeature::manager() { return _manager.get(); }

}  // namespace arangodb
