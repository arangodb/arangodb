////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_CLUSTER_UPGRADE_FEATURE_H
#define APPLICATION_FEATURES_CLUSTER_UPGRADE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

// this feature is responsible for performing a cluster upgrade. 
// it is only doing something in a coordinator, and only if the server was started 
// with the option `--database.auto-upgrade true`. The feature is late in the
// startup sequence, so it can use the full cluster functionality when run.
// after the feature has executed the upgrade, it will shut down the server.
class ClusterUpgradeFeature final : public application_features::ApplicationFeature {
 public:
  explicit ClusterUpgradeFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  
  void setBootstrapVersion();

 private:
  void tryClusterUpgrade();
  bool upgradeCoordinator();

 private:
  std::string _upgradeMode;
};

}  // namespace arangodb

#endif
