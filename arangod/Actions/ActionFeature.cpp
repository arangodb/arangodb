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

#include "ActionFeature.h"

#include "Actions/actions.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ActionFeature::ActionFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Action"), _allowUseDatabase(false) {
  setOptional(true);
  startsAfter<ClusterFeaturePhase>();
}

void ActionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption(
      "--server.allow-use-database",
      "allow change of database in REST actions, only needed for "
      "unittests",
      new BooleanParameter(&_allowUseDatabase),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}


void ActionFeature::unprepare() {
  TRI_CleanupActions();
}

bool ActionFeature::allowUseDatabase() const { return _allowUseDatabase; }

}  // namespace arangodb
