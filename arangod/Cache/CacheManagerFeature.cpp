////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "CacheManagerFeature.h"

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#endif

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/process-utils.h"
#include "Basics/WorkMonitor.h"
#include "Basics/asio-helper.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/Manager.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::cache;
using namespace arangodb::options;
using namespace arangodb::rest;

Manager* CacheManagerFeature::MANAGER = nullptr;
const uint64_t CacheManagerFeature::minRebalancingInterval = 500 * 1000;

CacheManagerFeature::CacheManagerFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "CacheManager"),
      _manager(nullptr),
      _rebalancer(nullptr),
      _cacheSize((TRI_PhysicalMemory >= (static_cast<uint64_t>(4) << 30))
                  ? static_cast<uint64_t>((TRI_PhysicalMemory - (static_cast<uint64_t>(2) << 30)) * 0.3)
                  : (256 << 20)),
      _rebalancingInterval(static_cast<uint64_t>(2 * 1000 * 1000)) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Scheduler");
}

CacheManagerFeature::~CacheManagerFeature() {}

void CacheManagerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("cache", "Configure the hash cache");

  options->addOption("--cache.size", "size of cache in bytes",
                     new UInt64Parameter(&_cacheSize));

  options->addOption("--cache.rebalancing-interval",
                     "microseconds between rebalancing attempts",
                     new UInt64Parameter(&_rebalancingInterval));
}

void CacheManagerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_cacheSize < Manager::minSize) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for `--cache.size', need at least "
        << Manager::minSize;
    FATAL_ERROR_EXIT();
  }

  if (_cacheSize < (CacheManagerFeature::minRebalancingInterval)) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for `--cache.rebalancing-interval', need at least "
        << (CacheManagerFeature::minRebalancingInterval);
    FATAL_ERROR_EXIT();
  }
}

void CacheManagerFeature::start() {
  auto scheduler = SchedulerFeature::SCHEDULER;
  auto postFn = [scheduler](std::function<void()> fn) -> bool {
    scheduler->post(fn);
    return true;
  };
  _manager.reset(new Manager(postFn, _cacheSize));
  MANAGER = _manager.get();
  _rebalancer.reset(
      new CacheRebalancerThread(_manager.get(), _rebalancingInterval));
  _rebalancer->start();
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "cache manager has started";
}

void CacheManagerFeature::beginShutdown() {
  if (_manager != nullptr) {
    _manager->beginShutdown();
    _rebalancer->beginShutdown();
  }
}

void CacheManagerFeature::stop() {
  if (_manager != nullptr) {
    _manager->shutdown();
  }
}

void CacheManagerFeature::unprepare() { MANAGER = nullptr; }
