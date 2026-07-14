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

namespace arangodb {

DumpLimitsFeature::DumpLimitsFeature(ApplicationServer& server)
    : DumpLimitsFeature(server, DumpLimitsOptionsProvider{}.options()) {}

DumpLimitsFeature::DumpLimitsFeature(ApplicationServer& server,
                                     DumpLimitsFeatureOptions options)
    : ApplicationFeature{server, *this}, _options(std::move(options)) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
}

}  // namespace arangodb
