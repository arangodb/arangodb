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

#include "ActionFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Actions/actions.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ActionFeature::ActionFeature(Server& server)
    : ArangodFeature{server, *this}, _allowUseDatabase(false) {
  setOptional(true);
  startsAfter<application_features::ClusterFeaturePhase>();
}

void ActionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--server.allow-use-database",
      "Allow to change the database in REST actions. Only needed internally "
      "for unit tests.",
      new BooleanParameter(&_allowUseDatabase),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

void ActionFeature::unprepare() { TRI_CleanupActions(); }

bool ActionFeature::allowUseDatabase() const { return _allowUseDatabase; }

}  // namespace arangodb
