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
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/process-utils.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

#ifdef __linux__
// the option is only meaningful on Linux
bool MaxMapCountFeature::_doCheck = true;
#else
// and turned off elsewhere
bool MaxMapCountFeature::_doCheck = false;
#endif

MaxMapCountFeature::MaxMapCountFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "MaxMapCount") {
  setOptional(false);
  startsAfter("GreetingsPhase");
}

void MaxMapCountFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("server", "Server Options");

  if (_doCheck) {
    options->addOption("--server.check-max-memory-mappings, mappings",
                       "check the maximum number of memory mappings at runtime",
                       new BooleanParameter(&_doCheck),
                       arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
  } else {
    options->addObsoleteOption(
        "--server.check-max-memory-mappings",
        "check the maximum number of memory mappings at runtime", true);
  }
}

uint64_t MaxMapCountFeature::actualMaxMappings() {
  uint64_t maxMappings = UINT64_MAX;

  // in case we cannot determine the number of max_map_count, we will
  // assume an effectively unlimited number of mappings
  if (needsChecking()) {
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

  return maxMappings;
}

uint64_t MaxMapCountFeature::minimumExpectedMaxMappings() {
  uint64_t expected = 0;

  if (needsChecking()) {
    expected = 65530;  // Linux kernel default

    uint64_t nproc = TRI_numberProcessors();

    // we expect at most 8 times the number of cores as the effective number of
    // threads, and we want to allow at least 8000 mmaps per thread
    if (nproc * 8 * 8000 > expected) {
      expected = nproc * 8 * 8000;
    }
  }

  return expected;
}
