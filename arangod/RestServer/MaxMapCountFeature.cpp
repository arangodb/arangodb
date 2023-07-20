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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "MaxMapCountFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/process-utils.h"
#include "Basics/system-functions.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

DECLARE_GAUGE(arangodb_memory_maps_current, uint64_t,
              "Number of currently mapped memory pages");
DECLARE_GAUGE(
    arangodb_memory_maps_limit, uint64_t,
    "Limit for the number of memory mappings for the arangod process");

MaxMapCountFeature::MaxMapCountFeature(Server& server)
    : ArangodFeature{server, *this},
#ifdef __linux__
      _countInterval(10 * 1000),
#else
      _countInterval(0),
#endif
      _memoryMapsCurrent(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_memory_maps_current{})),
      _memoryMapsLimit(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_memory_maps_limit{})) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void MaxMapCountFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addObsoleteOption(
      "--server.check-max-memory-mappings",
      "check the maximum number of memory mappings at startup", true);
}

uint64_t MaxMapCountFeature::actualMaxMappings() {
  uint64_t maxMappings = UINT64_MAX;

  // in case we cannot determine the number of max_map_count, we will
  // assume an effectively unlimited number of mappings
#ifdef __linux__
  // test max_map_count value in /proc/sys/vm
  try {
    std::string value = basics::FileUtils::slurp("/proc/sys/vm/max_map_count");
    maxMappings = basics::StringUtils::uint64(value);
    _memoryMapsLimit = maxMappings;
  } catch (...) {
    // file not found or values not convertible into integers
  }
#endif

  return maxMappings;
}

uint64_t MaxMapCountFeature::minimumExpectedMaxMappings() {
#ifdef __linux__
  uint64_t expected = 65530;  // Linux kernel default

  uint64_t nproc = NumberOfCores::getValue();

  // we expect at most 8 times the number of cores as the effective number of
  // threads, and we want to allow at least 8000 mmaps per thread
  if (nproc * 8 * 8000 > expected) {
    expected = nproc * 8 * 8000;
  }

  return expected;
#else
  return 0;
#endif
}

void MaxMapCountFeature::countMemoryMaps() {
#ifdef __linux__
  try {
    size_t numLines = FileUtils::countLines("/proc/self/maps");
    _memoryMapsCurrent.store(numFiles, std::memory_order_relaxed);
  } catch (std::exception const& ex) {
    LOG_TOPIC("bee41", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process: "
        << ex.what();
  } catch (...) {
    LOG_TOPIC("0a654", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process";
  }
#endif
}

void MaxMapCountFeature::countMemoryMapsIfNeeded() {
  if (_countInterval == 0) {
    return;
  }

  auto now = std::chrono::steady_clock::now();

  std::unique_lock guard{_lastCountMutex, std::try_to_lock};

  if (guard.owns_lock() &&
      (_lastCountStamp.time_since_epoch().count() == 0 ||
       now - _lastCountStamp > std::chrono::milliseconds(_countInterval))) {
    countMemoryMaps();
    _lastCountStamp = now;
  }
}
