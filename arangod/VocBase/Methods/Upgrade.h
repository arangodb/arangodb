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

#ifndef ARANGOD_VOC_BASE_API_UPGRADE_H
#define ARANGOD_VOC_BASE_API_UPGRADE_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include "Basics/Result.h"
#include "VocBase/Methods/Version.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace methods {

struct UpgradeArgs {
  TRI_vocbase_t* vocbase;
  velocypack::Slice users;
  bool isUpgrade;
};
  
struct UpgradeResult : Result {
  UpgradeResult(int err, VersionResult::StatusCode s)
    : Result(err), type(s) {}
  VersionResult::StatusCode type;
};

/// Code to create and initialize databases
/// Replaces ugrade-database.js for good
struct Upgrade {
  friend class UpgradeFeature;

  enum Flags : uint32_t {
    DATABASE_SYSTEM = (1u << 0),
    DATABASE_ALL = (1u << 1),
    DATABASE_EXCEPT_SYSTEM = (1u << 2),
    // =============
    DATABASE_INIT = (1u << 3),
    DATABASE_UPGRADE = (1u << 4),
    DATABASE_EXISTING = (1u << 5),
    // =============
    CLUSTER_NONE = (1u << 6),
    CLUSTER_LOCAL = (1u << 7),
    CLUSTER_COORDINATOR_GLOBAL = (1u << 8),
    CLUSTER_DB_SERVER_LOCAL = (1u << 9)
  };

  typedef std::function<void(UpgradeArgs const&)> TaskFunction;
  struct Task {
    std::string name;
    std::string description;
    uint32_t systemFlag;
    uint32_t clusterFlags;
    uint32_t databaseFlags;
    TaskFunction action;
  };
  
public:
  
  static UpgradeResult database(UpgradeArgs const&);

 private:
  static std::vector<Task> _tasks;
  static void addTask(std::string&& name, std::string&& desc,
                      uint32_t systemFlag, uint32_t clusterFlag,
                      uint32_t dbFlag, TaskFunction&& action) {
    _tasks.push_back(Task{name, desc, 0, 0, 0, action});
  }

  /// @brief register tasks, only run once on startup
  static void registerTasks();
  static void runTasks(UpgradeArgs const&, VersionResult vInfi,
                       uint32_t clusterFlag, uint32_t dbFlag);

  /*
  /// @brief system database only
  constexpr int DATABASE_SYSTEM = 1000;
  /// @brief all databases
  constexpr int DATABASE_ALL = 1001;
  /// @brief all databases expect system
  constexpr int DATABASE_EXCEPT_SYSTEM = 1002;
  /// @brief for stand-alone, no cluster
  constexpr int CLUSTER_NONE = 2000;
  /// @brief for cluster local part
  constexpr int CLUSTER_LOCAL = 2001;
  /// @brief for cluster global part (shared collections)
  constexpr int CLUSTER_COORDINATOR_GLOBAL = 2002;
  /// @brief db server global part (DB server local!)
  constexpr int CLUSTER_DB_SERVER_LOCAL = 2003;
  /// @brief for new databases
  constexpr int DATABASE_INIT = 3000;
  /// @brief for existing database, which must be upgraded
  constexpr int DATABASE_UPGRADE = 3001;
  /// @brief for existing database, which are already at the correct version
  constexpr int DATABASE_EXISTING = 3002;*/
};
}
}

#endif
