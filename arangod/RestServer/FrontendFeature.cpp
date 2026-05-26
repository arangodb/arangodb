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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "FrontendFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/FrontendOptionsProvider.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FrontendFeature::FrontendFeature(ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(true);
  startsAfter<ServerFeaturePhase>();
}

void FrontendFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  FrontendOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void FrontendFeature::prepare() {
  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  dealer.defineBoolean("FE_VERSION_CHECK", _options.versionCheck);
}

}  // namespace arangodb
