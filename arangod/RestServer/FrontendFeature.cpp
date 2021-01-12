////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "FrontendFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FrontendFeature::FrontendFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Frontend"), _versionCheck(true) {
  setOptional(true);
  startsAfter<ServerFeaturePhase>();
}

void FrontendFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("frontend", "Configure the frontend");

  options->addOption("--frontend.version-check",
                     "alert the user if new versions are available",
                     new BooleanParameter(&_versionCheck),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle,
                     arangodb::options::Flags::Hidden));
}

void FrontendFeature::prepare() {
  V8DealerFeature& dealer = server().getFeature<V8DealerFeature>();
  dealer.defineBoolean("FE_VERSION_CHECK", _versionCheck);
}

}  // namespace arangodb
