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
/// @author Simon Gräter
////////////////////////////////////////////////////////////////////////////////

#include "Upgrade.h"
#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Rest/Version.h"
#include "Utils/ExecContext.h"
#include "VocBase/Methods/UpgradeTasks.h"
#include "VocBase/Methods/Version.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

UpgradeResult methods::Upgrade::database(UpgradeArgs const& args) {
  uint32_t clusterFlag = 0;
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  if (ServerState::isSingleServer(role)) {
    clusterFlag = Flags::CLUSTER_NONE;
  } else {
    if (ServerState::isRunningInCluster(role)) {
      if (ServerState::isDBServer(role)) {
        clusterFlag = Flags::CLUSTER_DB_SERVER_LOCAL;
      } else {
        clusterFlag = Flags::CLUSTER_COORDINATOR_GLOBAL;
      }
    } else {
      clusterFlag = Flags::CLUSTER_LOCAL;
    }
  }
  
  VersionResult vInfo = Version::check(args.vocbase);
  
  
  runTasks(args, vInfo, clusterFlag, <#uint32_t dbFlag#>)
  
}

/// @brief register tasks, only run once on startup
void methods::Upgrade::registerTasks() {
  TRI_ASSERT(_tasks.empty());

  addTask("setupGraphs", "setup _graphs collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupGraphs);
  addTask("setupUsers", "setup _users collection",
          /*system*/ Flags::DATABASE_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupUsers);
  addTask("createUsersIndex", "create index on 'user' attribute in _users",
          /*system*/ Flags::DATABASE_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE,
          &UpgradeTasks::createUsersIndex);
  addTask("addDefaultUsers", "add default users for a new database",
          /*system*/ Flags::DATABASE_EXCEPT_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT, &UpgradeTasks::addDefaultUsers);
  addTask("updateUserModels",
          "convert documents in _users collection to new format",
          /*system*/ Flags::DATABASE_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_UPGRADE,  // DATABASE_EXISTING
          &UpgradeTasks::updateUserModels);
  addTask("createModules", "setup _modules collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createModules);
  addTask("createRouting", "setup _routing collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createRouting);
  addTask("insertRedirectionsAll", "insert default routes for admin interface",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_EXISTING,
          &UpgradeTasks::insertRedirections);
  addTask("setupAqlFunctions", "setup _aqlfunctions collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupAqlFunctions);
  addTask("createFrontend", "setup _frontend collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createFrontend);
  addTask("setupQueues", "setup _queues collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupQueues);
  addTask("setupJobs", "setup _jobs collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupJobs);
  addTask("createJobsIndex", "create index on attributes in _jobs collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createJobsIndex);
  addTask("setupApps", "setup _apps collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupApps);
  addTask("createAppsIndex", "create index on attributes in _apps collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createAppsIndex);
  addTask("setupAppBundles", "setup _appbundles collection",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupAppBundles);
}

void methods::Upgrade::runTasks(UpgradeArgs const& args,
                                VersionResult vinfo,
                                uint32_t clusterFlag,
                                uint32_t dbFlag) {
  TRI_ASSERT(args.vocbase != nullptr);
  TRI_ASSERT(clusterFlag != 0 && dbFlag != 0);
  
  bool isLocal = clusterFlag == CLUSTER_NONE || clusterFlag == CLUSTER_LOCAL;
  
  // execute all tasks
  for (Task const& t : _tasks) {
    // check for system database
    if (t.systemFlag == DATABASE_SYSTEM && !args.vocbase->isSystem()) {
      continue;
    }
    if (t.systemFlag == DATABASE_EXCEPT_SYSTEM && args.vocbase->isSystem()) {
      continue;
    }

    // check that the cluster occurs in the cluster list
    if (!(t.clusterFlags & clusterFlag)) {
      continue;
    }

    // check that the database occurs in the database list
    if (!(t.databaseFlags & dbFlag)) {
      // special optimisation: for local server and new database,
      // an upgrade-only task can be viewed as executed.
      if (isLocal && dbFlag == DATABASE_INIT &&
          t.databaseFlags == DATABASE_UPGRADE) {
        vInfo.tasks.emplace(t.name, true);
      }
      continue;
    }

    // we need to execute this task
    try {
      t.action(args);
      vInfo.tasks.emplace(t.name, true);
      if (isLocal) { // save after every task for resilience
        methods::Version::write(args.vocbase, vInfo.tasks);
      }
    } catch (basics::Exception const& e) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Executing " << t.name << " ("
      << t.description << ") failed with " << e.message();
    }
  }
  
  if (isLocal) {
    methods::Version::write(args.vocbase, vInfo.tasks);
  }
  
  return vInfo;
}

