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

std::vector<Upgrade::Task> Upgrade::_tasks;

/// corresponding to cluster-bootstrap.js
UpgradeResult Upgrade::clusterBootstrap(TRI_vocbase_t* system) {
  // no actually used here
  uint32_t cc = Version::current();
  VersionResult vinfo = {VersionResult::VERSION_MATCH, cc, cc, {}};
  VPackSlice params = VPackSlice::emptyObjectSlice();
  return runTasks(system, vinfo, params, Flags::CLUSTER_COORDINATOR_GLOBAL,
                  Upgrade::Flags::DATABASE_INIT);
}
/// corresponding to local-database.js
UpgradeResult Upgrade::create(TRI_vocbase_t* vocbase,
                              velocypack::Slice const& users) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  TRI_ASSERT(users.isArray());

  uint32_t clusterFlag = 0;
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  if (ServerState::isSingleServer(role)) {
    clusterFlag = Upgrade::Flags::CLUSTER_NONE;
  } else {
    if (ServerState::isRunningInCluster(role)) {
      if (ServerState::isDBServer(role)) {
        clusterFlag = Flags::CLUSTER_DB_SERVER_LOCAL;
      } else {
        clusterFlag = Flags::CLUSTER_COORDINATOR_GLOBAL;
      }
    } else {
      TRI_ASSERT(ServerState::isAgent(role));
      clusterFlag = Upgrade::Flags::CLUSTER_LOCAL;
    }
  }

  VPackBuilder params;
  params.openObject();
  params.add("users", users);
  params.close();

  // no actually used here
  uint32_t cc = Version::current();
  VersionResult vinfo = {VersionResult::VERSION_MATCH, cc, cc, {}};
  return runTasks(vocbase, vinfo, params.slice(), clusterFlag,
                  Upgrade::Flags::DATABASE_INIT);
}

UpgradeResult Upgrade::startup(TRI_vocbase_t* vocbase, bool upgrade) {
  uint32_t clusterFlag = Flags::CLUSTER_LOCAL;
  if (ServerState::instance()->isSingleServer()) {
    clusterFlag = Flags::CLUSTER_NONE;
  }

  VersionResult vinfo = Version::check(vocbase);
  if (vinfo.status == VersionResult::DOWNGRADE_NEEDED) {
    LOG_TOPIC(ERR, Logger::STARTUP)
        << "Database directory version (" << vinfo.databaseVersion
        << ") is higher than current version (" << vinfo.serverVersion << ").";

    LOG_TOPIC(ERR, Logger::STARTUP)
        << "It seems like you are running ArangoDB on a database directory"
        << "that was created with a newer version of ArangoDB. Maybe this"
        << " is what you wanted but it is not supported by ArangoDB.";

    return UpgradeResult(TRI_ERROR_BAD_PARAMETER, vinfo.status);
  } else if (vinfo.status == VersionResult::UPGRADE_NEEDED && !upgrade) {
    LOG_TOPIC(ERR, Logger::STARTUP)
        << "Database directory version (" << vinfo.databaseVersion
        << ") is lower than current version (" << vinfo.serverVersion << ").";

    LOG_TOPIC(ERR, Logger::STARTUP) << "---------------------------------------"
                                       "-------------------------------";
    LOG_TOPIC(ERR, Logger::STARTUP)
        << "It seems like you have upgraded the ArangoDB binary.";
    LOG_TOPIC(ERR, Logger::STARTUP)
        << "If this is what you wanted to do, please restart with the'";
    LOG_TOPIC(ERR, Logger::STARTUP) << "  --database.auto-upgrade true'";
    LOG_TOPIC(ERR, Logger::STARTUP)
        << "option to upgrade the data in the database directory.'";

    LOG_TOPIC(ERR, Logger::STARTUP)
        << "Normally you can use the control script to upgrade your database'";
    LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb stop'";
    LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb upgrade'";
    LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb start'";
    LOG_TOPIC(ERR, Logger::STARTUP) << "---------------------------------------"
                                       "-------------------------------'";
    return UpgradeResult(TRI_ERROR_BAD_PARAMETER, vinfo.status);
  } else {
    switch (vinfo.status) {
      case VersionResult::CANNOT_PARSE_VERSION_FILE:
      case VersionResult::CANNOT_READ_VERSION_FILE:
      case VersionResult::NO_SERVER_VERSION: {
        std::string msg =
            std::string("error during") + (upgrade ? "upgrade" : "startup");
        return UpgradeResult(TRI_ERROR_INTERNAL, msg, vinfo.status);
      }
      case VersionResult::NO_VERSION_FILE:
        if (upgrade) {
          LOG_TOPIC(FATAL, Logger::STARTUP)
              << "Cannot upgrade without VERSION file";
          return UpgradeResult(TRI_ERROR_INTERNAL, vinfo.status);
        }
        break;
      default:
        break;
    }
  }

  uint32_t dbflag = upgrade ? Flags::DATABASE_UPGRADE : Flags::DATABASE_INIT;
  VPackSlice const params = VPackSlice::emptyObjectSlice();
  return runTasks(vocbase, vinfo, params, clusterFlag, dbflag);
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

UpgradeResult methods::Upgrade::runTasks(TRI_vocbase_t* vocbase,
                                         VersionResult& vinfo,
                                         VPackSlice const& params,
                                         uint32_t clusterFlag,
                                         uint32_t dbFlag) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(clusterFlag != 0 && dbFlag != 0);
  TRI_ASSERT(!_tasks.empty()); // forgot to call registerTask!!

  bool isLocal = clusterFlag == CLUSTER_NONE || clusterFlag == CLUSTER_LOCAL;

  // execute all tasks
  for (Task const& t : _tasks) {
    // check for system database
    if (t.systemFlag == DATABASE_SYSTEM && !vocbase->isSystem()) {
      continue;
    }
    if (t.systemFlag == DATABASE_EXCEPT_SYSTEM && vocbase->isSystem()) {
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
        vinfo.tasks.emplace(t.name, true);
      }
      continue;
    }

    // we need to execute this task
    try {
      t.action(vocbase, params);
      vinfo.tasks.emplace(t.name, true);
      if (isLocal) {  // save after every task for resilience
        methods::Version::write(vocbase, vinfo.tasks);
      }
    } catch (basics::Exception const& e) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Executing " << t.name << " ("
                                      << t.description << ") failed with "
                                      << e.message();
      return UpgradeResult(e.code(), e.what(), vinfo.status);
    }
  }

  if (isLocal) {  // no need to write this for cluster bootstrap
    methods::Version::write(vocbase, vinfo.tasks);
  }
  return UpgradeResult(TRI_ERROR_NO_ERROR, vinfo.status);
}
