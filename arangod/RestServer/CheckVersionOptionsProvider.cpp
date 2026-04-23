////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#include "CheckVersionOptionsProvider.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/EnvironmentFeature.h"

namespace arangodb::check_version {

using namespace arangodb::options;

void CheckVersionOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, CheckVersionFeatureOptions& opts) {
  options->addOldOption("check-version", "database.check-version");
  options->addOption(
      "--database.check-version", "Check the version of the database and exit.",
      new BooleanParameter(&opts.checkVersion),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));
}

}  // namespace arangodb::check_version
