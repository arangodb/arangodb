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
UpgradeResult Upgrade::clusterBootstrap(TRI_vocbase_t& system) {
  uint64_t cc = Version::current(); // not actually used here
  VersionResult vinfo = {VersionResult::VERSION_MATCH, cc, cc, {}};
  uint32_t clusterFlag = Flags::CLUSTER_COORDINATOR_GLOBAL;
  if (ServerState::instance()->isDBServer()) {
    clusterFlag = Flags::CLUSTER_DB_SERVER_LOCAL;
  }
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());

  VPackSlice params = VPackSlice::emptyObjectSlice();
  return runTasks(system, vinfo, params, clusterFlag,
                  Upgrade::Flags::DATABASE_INIT);
}

/// corresponding to local-database.js
UpgradeResult Upgrade::createDB(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& users
) {
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

  // will write a version file with this number
  uint64_t cc = Version::current();
  // to create a DB we use an empty version result because we want
  // to execute all tasks not needed for an upgrade
  VersionResult vinfo = {VersionResult::VERSION_MATCH, cc, cc, {}};
  return runTasks(vocbase, vinfo, params.slice(), clusterFlag,
                  Upgrade::Flags::DATABASE_INIT);
}

UpgradeResult Upgrade::startup(
    TRI_vocbase_t& vocbase,
    bool isUpgrade,
    bool ignoreFileErrors
) {
  uint32_t clusterFlag = Flags::CLUSTER_LOCAL;

  if (ServerState::instance()->isSingleServer()) {
    clusterFlag = Flags::CLUSTER_NONE;
  }

  uint32_t dbflag = Flags::DATABASE_EXISTING;
  auto vinfo = Version::check(&vocbase);

  if (vinfo.status == methods::VersionResult::CANNOT_PARSE_VERSION_FILE ||
      vinfo.status == methods::VersionResult::CANNOT_READ_VERSION_FILE) {
    if (ignoreFileErrors) {
      // try to install a fresh new, empty VERSION file instead
      if (methods::Version::write(&vocbase, std::map<std::string, bool>(), true).ok()) {
        // give it another try
        LOG_TOPIC(WARN, Logger::STARTUP) << "overwriting unparsable VERSION file with default value "
                                         << "because option `--database.ignore-logfile-errors` is set";
        vinfo = methods::Version::check(&vocbase);
      }
    } else {
      LOG_TOPIC(WARN, Logger::STARTUP) << "in order to automatically fix the VERSION file on startup, "
                                       << "please start the server with option `--database.ignore-logfile-errors true`";
    }
  }

  switch (vinfo.status) {
    case VersionResult::INVALID:
      TRI_ASSERT(false);  // never returned by Version::check
    case VersionResult::VERSION_MATCH:
      break;  // just run tasks that weren't run yet
    case VersionResult::UPGRADE_NEEDED: {
      if (!isUpgrade) {
        // we do not perform upgrades without being told so during startup
        LOG_TOPIC(ERR, Logger::STARTUP)
            << "Database directory version (" << vinfo.databaseVersion
            << ") is lower than current version (" << vinfo.serverVersion
            << ").";

        LOG_TOPIC(ERR, Logger::STARTUP)
    << "----------------------------------------------------------------------";
        LOG_TOPIC(ERR, Logger::STARTUP)
            << "It seems like you have upgraded the ArangoDB binary.";
        LOG_TOPIC(ERR, Logger::STARTUP)
            << "If this is what you wanted to do, please restart with the";
        LOG_TOPIC(ERR, Logger::STARTUP) << "  --database.auto-upgrade true";
        LOG_TOPIC(ERR, Logger::STARTUP)
            << "option to upgrade the data in the database directory.";

        LOG_TOPIC(ERR, Logger::STARTUP) << "Normally you can use the control "
                                           "script to upgrade your database'";
        LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb stop";
        LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb upgrade";
        LOG_TOPIC(ERR, Logger::STARTUP) << "  /etc/init.d/arangodb start";
        LOG_TOPIC(ERR, Logger::STARTUP)
        << "----------------------------------------------------------------------'";
        return UpgradeResult(TRI_ERROR_BAD_PARAMETER, vinfo.status);
      }
      // do perform the upgrade
      dbflag = Flags::DATABASE_UPGRADE;
      break;
    }
    case VersionResult::DOWNGRADE_NEEDED: {
      // we do not support downgrades, just error out
      LOG_TOPIC(ERR, Logger::STARTUP)
          << "Database directory version (" << vinfo.databaseVersion
          << ") is higher than current version (" << vinfo.serverVersion
          << ").";
      LOG_TOPIC(ERR, Logger::STARTUP)
          << "It seems like you are running ArangoDB on a database directory"
          << " that was created with a newer version of ArangoDB. Maybe this"
          << " is what you wanted but it is not supported by ArangoDB.";
      return UpgradeResult(TRI_ERROR_NO_ERROR, vinfo.status);
    }
    case VersionResult::CANNOT_PARSE_VERSION_FILE:
    case VersionResult::CANNOT_READ_VERSION_FILE:
    case VersionResult::NO_SERVER_VERSION: {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Error reading version file";
      std::string msg =
          std::string("error during ") + (isUpgrade ? "upgrade" : "startup");
      return UpgradeResult(TRI_ERROR_INTERNAL, msg, vinfo.status);
    }
    case VersionResult::NO_VERSION_FILE:
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "No VERSION file found";
      // VERSION file does not exist, we are running on a new database
      dbflag = DATABASE_INIT;
      break;
  }

  // should not do anything on VERSION_MATCH, and init the database
  // with all tasks if they were not executed yet. Tasks not listed
  // in the "tasks" attribute will be executed automatically
  VPackSlice const params = VPackSlice::emptyObjectSlice();
  return runTasks(vocbase, vinfo, params, clusterFlag, dbflag);
}

/// @brief register tasks, only run once on startup
void methods::Upgrade::registerTasks() {
  TRI_ASSERT(_tasks.empty());

  addTask("upgradeGeoIndexes", "upgrade legacy geo indexes",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_DB_SERVER_LOCAL,
          /*database*/ DATABASE_UPGRADE,
          &UpgradeTasks::upgradeGeoIndexes);
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
  addTask("addDefaultUserOther", "add default users for a new database",
          /*system*/ Flags::DATABASE_EXCEPT_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT, &UpgradeTasks::addDefaultUserOther);
  addTask("setupAnalyzers", "setup _iresearch_analyzers collection",
          /*system*/ Flags::DATABASE_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::setupAnalyzers);
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
  addTask("persistLocalDocumentIds", "convert collection data from old format",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_DB_SERVER_LOCAL,
          /*database*/ DATABASE_UPGRADE,
          &UpgradeTasks::persistLocalDocumentIds);
}

UpgradeResult methods::Upgrade::runTasks(
    TRI_vocbase_t& vocbase,
    VersionResult& vinfo,
    arangodb::velocypack::Slice const& params,
    uint32_t clusterFlag,
    uint32_t dbFlag
) {
  TRI_ASSERT(clusterFlag != 0 && dbFlag != 0);
  TRI_ASSERT(!_tasks.empty());  // forgot to call registerTask!!
  // needs to run in superuser scope, otherwise we get errors
  ExecContextScope scope(ExecContext::superuser());
  // only local should actually write a VERSION file
  bool isLocal = clusterFlag == CLUSTER_NONE || clusterFlag == CLUSTER_LOCAL;

  bool ranOnce = false;
  // execute all tasks
  for (Task const& t : _tasks) {
    // check for system database
    if (t.systemFlag == DATABASE_SYSTEM && !vocbase.isSystem()) {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: DB not system, skipping " << t.name;
      continue;
    }
    if (t.systemFlag == DATABASE_EXCEPT_SYSTEM && vocbase.isSystem()) {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: DB system, Skipping " << t.name;
      continue;
    }

    // check that the cluster occurs in the cluster list
    if (!(t.clusterFlags & clusterFlag)) {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: cluster mismatch, skipping " << t.name;
      continue;
    }

    auto const& it = vinfo.tasks.find(t.name);
    if (it != vinfo.tasks.end()) {
      if (it->second) {
        LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: Already executed, skipping " << t.name;
        continue;
      }
      vinfo.tasks.erase(it);  // in case we encounter false
    }

    // check that the database occurs in the database list
    if (!(t.databaseFlags & dbFlag)) {
      // special optimization: for local server and new database,
      // an upgrade-only task can be viewed as executed.
      if (isLocal && dbFlag == DATABASE_INIT &&
          t.databaseFlags == DATABASE_UPGRADE) {
        vinfo.tasks.emplace(t.name, true);
      }
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: db flag mismatch, skipping " << t.name;
      continue;
    }

    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: Executing " << t.name;
    try {
      bool ranTask = t.action(vocbase, params);
      if (!ranTask) {
        std::string msg =
            "Executing " + t.name + " (" + t.description + ") failed.";
        LOG_TOPIC(ERR, Logger::STARTUP) << msg << " Aborting procedure.";
        return UpgradeResult(TRI_ERROR_INTERNAL, msg, vinfo.status);
      }
    } catch (basics::Exception const& e) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Executing " << t.name << " ("
                                      << t.description << ") failed with "
                                      << e.message() << ". Aborting procedure.";
      return UpgradeResult(e.code(), e.what(), vinfo.status);
    }

    // remember we already executed this one
    vinfo.tasks.emplace(t.name, true);

    if (isLocal) {  // save after every task for resilience
      auto res = methods::Version::write(&vocbase, vinfo.tasks, /*sync*/false);

      if (res.fail()) {
        return UpgradeResult(res.errorNumber(), res.errorMessage(),
                             vinfo.status);
      }

      ranOnce = true;
    }
  }

  if (isLocal) {  // no need to write this for cluster bootstrap
    // save even if no tasks were executed
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Upgrade: Writing VERSION file";
    auto res = methods::Version::write(&vocbase, vinfo.tasks, /*sync*/ranOnce);

    if (res.fail()) {
      return UpgradeResult(res.errorNumber(), res.errorMessage(), vinfo.status);
    }
  }

  return UpgradeResult(TRI_ERROR_NO_ERROR, vinfo.status);
}
