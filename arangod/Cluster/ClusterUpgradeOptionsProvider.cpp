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

#include "ClusterUpgradeOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::upgrade {

using namespace arangodb::options;

void ClusterUpgradeOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options,
    ClusterUpgradeFeatureOptions& opts) {
  options
      ->addOption("--cluster.upgrade",
                  "Perform a cluster upgrade if necessary (auto = perform an upgrade "
                  "and shut down only if `--database.auto-upgrade true` is set, "
                  "disable = ignore `--database.auto-upgrade` and never perform an "
                  "upgrade, force = ignore `--database.auto-upgrade` and always "
                  "perform an upgrade and shut down, online = always perform an "
                  "upgrade but don't shut down).",
                  new DiscreteValuesParameter<StringParameter>(
                      &opts.upgradeMode,
                      std::unordered_set<std::string>{"auto", "disable", "force",
                                                      "online"}),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator));
}

}  // namespace arangodb::upgrade
