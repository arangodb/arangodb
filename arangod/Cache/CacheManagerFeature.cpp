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
#include "Cache/CacheOptionsFeature.h"
#include "Cache/CacheOptionsProvider.h"
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

CacheManagerFeature::CacheManagerFeature(Server& server,
                                         CacheOptionsProvider const& provider)
    : ArangodFeature{server, *this}, _provider(provider) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
  startsAfter<CacheOptionsFeature>();
}

CacheManagerFeature::~CacheManagerFeature() = default;

void CacheManagerFeature::start() {
  // get options from provider once
  _options = _provider.getOptions();

  if (ServerState::instance()->isAgent() || _options.cacheSize == 0) {
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

  LOG_TOPIC("708a6", DEBUG, Logger::CACHE)
      << "cache manager starting up. cache size: " << _options.cacheSize
      << ", ideal lower fill ratio: " << _options.idealLowerFillRatio
      << ", ideal upper fill ratio: " << _options.idealUpperFillRatio
      << ", min value size for edge compression: "
      << _options.minValueSizeForEdgeCompression << ", acceleration factor: "
      << _options.accelerationFactorForEdgeCompression
      << ", max spare allocation: " << _options.maxSpareAllocation
      << ", enable windowed stats: " << _options.enableWindowedStats;

  SharedPRNGFeature& sharedPRNG = server().getFeature<SharedPRNGFeature>();
  _manager = std::make_unique<Manager>(sharedPRNG, std::move(postFn), _options);

  _rebalancer = std::make_unique<CacheRebalancerThread>(
      server(), _manager.get(), _options.rebalancingInterval);
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

std::size_t CacheManagerFeature::minValueSizeForEdgeCompression()
    const noexcept {
  return _options.minValueSizeForEdgeCompression;
}

std::uint32_t CacheManagerFeature::accelerationFactorForEdgeCompression()
    const noexcept {
  return _options.accelerationFactorForEdgeCompression;
}

}  // namespace arangodb
