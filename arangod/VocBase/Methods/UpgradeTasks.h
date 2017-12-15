////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_API_UPGRADE_TASKS_H
#define ARANGOD_VOC_BASE_API_UPGRADE_TASKS_H 1

#include "VocBase/Methods/Upgrade.h"

namespace arangodb {
namespace methods {

/// Code to create and initialize databases
/// Replaces ugrade-database.js for good
struct UpgradeTasks {
  static void setupGraphs(UpgradeArgs const&);
  static void setupUsers(UpgradeArgs const&);
  static void createUsersIndex(UpgradeArgs const&);
  static void addDefaultUsers(UpgradeArgs const&);
  static void updateUserModels(UpgradeArgs const&);
  static void createModules(UpgradeArgs const&);
  static void createRouting(UpgradeArgs const&);
  static void insertRedirections(UpgradeArgs const&);
  static void setupAqlFunctions(UpgradeArgs const&);
  static void createFrontend(UpgradeArgs const&);
  static void setupQueues(UpgradeArgs const&);
  static void setupJobs(UpgradeArgs const&);
  static void createJobsIndex(UpgradeArgs const&);
  static void setupApps(UpgradeArgs const&);
  static void createAppsIndex(UpgradeArgs const&);
  static void setupAppBundles(UpgradeArgs const&);
};
}
}

#endif
