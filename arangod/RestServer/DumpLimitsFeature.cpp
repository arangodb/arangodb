////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "DumpLimitsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/PhysicalMemory.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DumpLimitsOptionsProvider.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
uint64_t defaultMemoryUsage() {
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.2
    return static_cast<uint64_t>(
        (PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.2);
  }
  // if we have at least 2GB of RAM, the default size is 64MB
  return (static_cast<uint64_t>(64) << 20);
}
}  // namespace

namespace arangodb {

DumpLimitsFeature::DumpLimitsFeature(ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

  _options.memoryUsage = defaultMemoryUsage();
}

void DumpLimitsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  DumpLimitsOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void DumpLimitsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  DumpLimitsOptionsProvider provider;
  provider.validateOptions(options, _options);
}

}  // namespace arangodb
