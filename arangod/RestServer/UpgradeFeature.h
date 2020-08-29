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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_UPGRADE_FEATURE_H
#define APPLICATION_FEATURES_UPGRADE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/Methods/Upgrade.h"

namespace arangodb {

// this feature is responsible for performing a database upgrade. 
// it is only doing something if the server was started with the option 
// `--database.auto-upgrade true` or `--database.check-version true`. 
// On a coordinator this feature will *not* perform the actual upgrade,
// because it is too early in the sequence. Coordinator upgrades are
// performed by the ClusterUpgradeFeature, which is way later in the
// startup sequence, so it can use the full cluster functionality when run.
// after this feature has executed the upgrade, it will shut down the server.
// in the coordinator case, this feature will not shut down the server.
// instead, the shutdown is performed by the ClusterUpgradeFeature.
class UpgradeFeature final : public application_features::ApplicationFeature {
 public:
  UpgradeFeature(application_features::ApplicationServer& server, int* result,
                 std::vector<std::type_index> const& nonServerFeatures);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  
  void addTask(methods::Upgrade::Task&& task);

 private:
  void upgradeLocalDatabase();

 private:
  friend struct methods::Upgrade;  // to allow access to '_tasks'

  bool _upgrade;
  bool _upgradeCheck;

  int* _result;
  std::vector<std::type_index> _nonServerFeatures;
  std::vector<methods::Upgrade::Task> _tasks;
};

}  // namespace arangodb

#endif
