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

#pragma once

#include "RestServer/arangod.h"
namespace arangodb {

// this feature is responsible for performing a cluster upgrade.
// it is only doing something in a coordinator, and only if the server was
// started with the option `--database.auto-upgrade true`. The feature is late
// in the startup sequence, so it can use the full cluster functionality when
// run. after the feature has executed the upgrade, it will shut down the
// server.
class ClusterUpgradeFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ClusterUpgrade"; }

  ClusterUpgradeFeature(ArangodServer& server,
                        DatabaseFeature& databaseFeature);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

  void setBootstrapVersion();

 private:
  void tryClusterUpgrade();
  bool upgradeCoordinator();

 private:
  std::string _upgradeMode;
  DatabaseFeature& _databaseFeature;
};

}  // namespace arangodb
