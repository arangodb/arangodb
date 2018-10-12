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
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {

static bool checkMaxMappings = arangodb::MaxMapCountFeature::needsChecking(); 
static uint64_t maxMappings = UINT64_MAX; 
static std::string mapsFilename;
}

MaxMapCountFeature::MaxMapCountFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "MaxMapCount") {
  setOptional(false);
  startsAfter("GreetingsPhase");

  maxMappings = UINT64_MAX;
  mapsFilename.clear();
}

MaxMapCountFeature::~MaxMapCountFeature() {
  // reset values
  maxMappings = UINT64_MAX;
  mapsFilename.clear();
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
      basics::FileUtils::slurp(mapsFilename);
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
  // and we want to allow at least 8000 mmaps per thread
  if (nproc * 8 * 8000 > expected) {
    expected = nproc * 8 * 8000;
  }

  return expected;
}

bool MaxMapCountFeature::isNearMaxMappings() {
  if (!needsChecking() || !checkMaxMappings || mapsFilename.empty()) {
    return false;
  }

  return isNearMaxMappingsInternal();
}

bool MaxMapCountFeature::isNearMaxMappingsInternal() noexcept {
  try {
    StringBuffer fileBuffer(8192, false);
    basics::FileUtils::slurp(mapsFilename, fileBuffer);
    
    size_t const nmaps = std::count(fileBuffer.begin(), fileBuffer.end(), '\n');
    if (nmaps + 1024 < maxMappings) {
      return false;
    }

    LOG_TOPIC(ERR, Logger::MEMORY) << "process is near the maximum number of memory mappings. current: " << nmaps << ", maximum: " << maxMappings 
                                   << ". it may be sensible to increase the maximum number of mappings per process";
    return true;
  } catch (...) {
    return false;
    // something went wrong
  } 
}
