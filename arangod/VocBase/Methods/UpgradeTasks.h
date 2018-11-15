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

#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace methods {

/// Code to create and initialize databases
/// Replaces ugrade-database.js for good
struct UpgradeTasks {
  static bool upgradeGeoIndexes(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupGraphs(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupUsers(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool createUsersIndex(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool addDefaultUserOther(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupAnalyzers(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupAqlFunctions(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool createFrontend(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupQueues(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupJobs(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool createJobsIndex(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupApps(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool createAppsIndex(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool setupAppBundles(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
  static bool persistLocalDocumentIds(TRI_vocbase_t& vocbase, velocypack::Slice const& slice);
};

}
}

#endif
