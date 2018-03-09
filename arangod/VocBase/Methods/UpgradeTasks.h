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
  static void setupGraphs(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupUsers(TRI_vocbase_t*, velocypack::Slice const&);
  static void createUsersIndex(TRI_vocbase_t*, velocypack::Slice const&);
  static void addDefaultUsers(TRI_vocbase_t*, velocypack::Slice const&);
  static void updateUserModels(TRI_vocbase_t*, velocypack::Slice const&);
  static void createModules(TRI_vocbase_t*, velocypack::Slice const&);
  static void createRouting(TRI_vocbase_t*, velocypack::Slice const&);
  static void insertRedirections(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupAqlFunctions(TRI_vocbase_t*, velocypack::Slice const&);
  static void createFrontend(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupQueues(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupJobs(TRI_vocbase_t*, velocypack::Slice const&);
  static void createJobsIndex(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupApps(TRI_vocbase_t*, velocypack::Slice const&);
  static void createAppsIndex(TRI_vocbase_t*, velocypack::Slice const&);
  static void setupAppBundles(TRI_vocbase_t*, velocypack::Slice const&);
};
}
}

#endif
