////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/process-utils.h"
#include "Basics/FileUtils.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
static bool checkMaxMappings = arangodb::MaxMapCountFeature::needsChecking(); 
static uint64_t maxMappings = UINT64_MAX; 
static std::string mapsFilename;

// cache for the current number of mappings for this process
// (read from /proc/<pid>/maps
static Mutex mutex;
static double lastStamp = 0.0;
static bool lastValue = false;
static bool checkInFlight = false;

static constexpr double cacheLifetime = 5.0; 

static double lastLogStamp = 0.0;
static constexpr double logFrequency = 10.0;
}
  
MaxMapCountFeature::MaxMapCountFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "MaxMapCount") {
  setOptional(false);
  requiresElevatedPrivileges(false);

  maxMappings = UINT64_MAX;
  mapsFilename.clear();
  lastStamp = 0.0;
  lastValue = false;
  checkInFlight = false;
  lastLogStamp = 0.0;
}

MaxMapCountFeature::~MaxMapCountFeature() {
  // reset values
  maxMappings = UINT64_MAX;
  mapsFilename.clear();
  lastStamp = 0.0;
  lastValue = false;
  checkInFlight = false;
  lastLogStamp = 0.0;
}
  
void MaxMapCountFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("server", "Server Options");
  
  if (needsChecking()) {
    options->addHiddenOption("--server.check-max-memory-mappings, mappings", "check the maximum number of memory mappings at runtime",
                             new BooleanParameter(&checkMaxMappings));
  } else {
    options->addObsoleteOption("--server.check-max-memory-mappings", "check the maximum number of memory mappings at runtime", true);
  }
}

void MaxMapCountFeature::prepare() {
  if (!needsChecking() || !checkMaxMappings) {
    return;
  }

  mapsFilename = "/proc/" + std::to_string(Thread::currentProcessId()) + "/maps";

  if (!FileUtils::exists(mapsFilename)) {
    mapsFilename.clear();
  } else {
    try {
      basics::FileUtils::slurp(mapsFilename.c_str());
    } catch (...) {
      // maps file not readable
      mapsFilename.clear();
    }
  }

  LOG_TOPIC(DEBUG, Logger::MEMORY) << "using process maps filename '" << mapsFilename << "'";

  // in case we cannot determine the number of max_map_count, we will
  // assume an effectively unlimited number of mappings 
  TRI_ASSERT(maxMappings == UINT64_MAX);

#ifdef __linux__
  // test max_map_count value in /proc/sys/vm
  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/vm/max_map_count");

    maxMappings = basics::StringUtils::uint64(value);
  } catch (...) {
    // file not found or values not convertible into integers
  }
#endif
}

uint64_t MaxMapCountFeature::actualMaxMappings() { return maxMappings; } 
  
uint64_t MaxMapCountFeature::minimumExpectedMaxMappings() {
  TRI_ASSERT(needsChecking());

  uint64_t expected = 65530; // kernel default

  uint64_t nproc = TRI_numberProcessors();

  // we expect at most 8 times the number of cores as the effective number of threads,
  // and we want to allow at most 1000 mmaps per thread
  if (nproc * 8 * 1000 > expected) {
    expected = nproc * 8 * 1000;
  }

  return expected;
}

bool MaxMapCountFeature::isNearMaxMappings() {
  if (!needsChecking() || !checkMaxMappings || mapsFilename.empty()) {
    return false;
  }

  double const now = TRI_microtime();

  {
    // we do not want any cache stampede
    MUTEX_LOCKER(locker, mutex);
    if (lastStamp >= now || checkInFlight) {
      // serve value from cache
      return lastValue;
    }

    checkInFlight = true;
  }

  // check current maps count without holding the mutex
  double cacheTime;
  bool const value = isNearMaxMappingsInternal(cacheTime);
 
  { 
    // update cache
    MUTEX_LOCKER(locker, mutex);
    lastValue = value;
    lastStamp = now + cacheTime;
    checkInFlight = false;
  }

  return value;
}

bool MaxMapCountFeature::isNearMaxMappingsInternal(double& suggestedCacheTime) noexcept {
  try {
    std::string value =
        basics::FileUtils::slurp(mapsFilename.c_str());
    
    size_t const nmaps = StringUtils::split(value, '\n').size();
    if (nmaps + 1024 < maxMappings) {
      if (nmaps >= maxMappings * 0.95) {
        // 95 % or more of the max mappings are in use. don't cache for too long
        suggestedCacheTime = 0.001;
      } else if (nmaps >= maxMappings / 2.0) {
        // we're above half of the max mappings. reduce cache time a bit
        suggestedCacheTime = cacheLifetime / 2.0;
      } else {
        suggestedCacheTime = cacheLifetime;
      }
      return false;
    }
    // we're near the maximum number of mappings. don't cache for too long
    suggestedCacheTime = 0.001;

    double now = TRI_microtime();
    if (lastLogStamp + logFrequency < now) {
      // do not log too often to avoid log spamming
      lastLogStamp = now;
      LOG_TOPIC(WARN, Logger::MEMORY) << "process is near the maximum number of memory mappings. current: " << nmaps << ", maximum: " << maxMappings;
    }
    return true;
  } catch (...) {
    // something went wrong. don't cache for too long
    suggestedCacheTime = 0.001;
    return false;
  } 
}
