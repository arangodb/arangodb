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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Upgrade.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "RestServer/UpgradeFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/Methods/UpgradeTasks.h"
#include "VocBase/Methods/Version.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {

void addTask(arangodb::UpgradeFeature& feature, std::string&& name,
             std::string&& desc, uint32_t systemFlag, uint32_t clusterFlag,
             uint32_t dbFlag, arangodb::methods::Upgrade::TaskFunction&& action) {
  feature.addTask(arangodb::methods::Upgrade::Task{name, desc, systemFlag,
                                                   clusterFlag, dbFlag, action});
}

}  // namespace

using namespace arangodb;
using namespace arangodb::methods;

/// corresponding to cluster-bootstrap.js
UpgradeResult Upgrade::clusterBootstrap(TRI_vocbase_t& system) {
  uint64_t cc = Version::current();  // not actually used here
  VersionResult vinfo = {VersionResult::VERSION_MATCH, cc, cc, {}};
  uint32_t clusterFlag = Flags::CLUSTER_COORDINATOR_GLOBAL;
  if (ServerState::instance()->isDBServer()) {
    clusterFlag = Flags::CLUSTER_DB_SERVER_LOCAL;
  }
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());

  VPackSlice params = VPackSlice::emptyObjectSlice();
  return runTasks(system, vinfo, params, clusterFlag, Upgrade::Flags::DATABASE_INIT);
}

/// corresponding to local-database.js
UpgradeResult Upgrade::createDB(TRI_vocbase_t& vocbase,
                                arangodb::velocypack::Slice const& users) {
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
  return runTasks(vocbase, vinfo, params.slice(), clusterFlag, Upgrade::Flags::DATABASE_INIT);
}

UpgradeResult Upgrade::startup(TRI_vocbase_t& vocbase, bool isUpgrade, bool ignoreFileErrors) {
  if (ServerState::instance()->isCoordinator()) {
    // coordinators do not have any persistent data, so there is no VERSION file
    // available. We don't know the previous version we are upgrading from, so we
    // need to pretend no upgrade is necessary
    return UpgradeResult(TRI_ERROR_NO_ERROR, methods::VersionResult::VERSION_MATCH);
  }
  
  uint32_t clusterFlag = 0;
  
  if (ServerState::instance()->isSingleServer()) {
    clusterFlag = Flags::CLUSTER_NONE;
  } else {
    clusterFlag = Flags::CLUSTER_LOCAL;
  }

  uint32_t dbflag = Flags::DATABASE_EXISTING;
  VersionResult vinfo = Version::check(&vocbase);

  if (vinfo.status == methods::VersionResult::CANNOT_PARSE_VERSION_FILE ||
      vinfo.status == methods::VersionResult::CANNOT_READ_VERSION_FILE) {
    if (ignoreFileErrors) {
      // try to install a fresh new, empty VERSION file instead
      if (methods::Version::write(&vocbase, std::map<std::string, bool>(), true).ok()) {
        // give it another try
        LOG_TOPIC("2feaa", WARN, Logger::STARTUP)
            << "overwriting unparsable VERSION file with default value "
            << "because option `--database.ignore-datafile-errors` is set";
        vinfo = methods::Version::check(&vocbase);
      }
    } else {
      LOG_TOPIC("3dd26", WARN, Logger::STARTUP)
          << "in order to automatically fix the VERSION file on startup, "
          << "please start the server with option "
             "`--database.ignore-datafile-errors true`";
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
        LOG_TOPIC("3bc7f", ERR, Logger::STARTUP)
            << "Database directory version (" << vinfo.databaseVersion
            << ") is lower than current version (" << vinfo.serverVersion << ").";

        LOG_TOPIC("ebca0", ERR, Logger::STARTUP)
            << "---------------------------------------------------------------"
               "-------";
        LOG_TOPIC("24e3c", ERR, Logger::STARTUP)
            << "It seems like you have upgraded the ArangoDB binary.";
        LOG_TOPIC("8bcec", ERR, Logger::STARTUP)
            << "If this is what you wanted to do, please restart with the";
        LOG_TOPIC("b0360", ERR, Logger::STARTUP)
            << "  --database.auto-upgrade true";
        LOG_TOPIC("13414", ERR, Logger::STARTUP)
            << "option to upgrade the data in the database directory.";
        LOG_TOPIC("24bd1", ERR, Logger::STARTUP)
            << "---------------------------------------------------------------"
               "-------'";
        return UpgradeResult(TRI_ERROR_BAD_PARAMETER, vinfo.status);
      }
      // do perform the upgrade
      dbflag = Flags::DATABASE_UPGRADE;
      break;
    }
    case VersionResult::DOWNGRADE_NEEDED: {
      // we do not support downgrades, just error out
      LOG_TOPIC("fdbd9", ERR, Logger::STARTUP)
          << "Database directory version (" << vinfo.databaseVersion
          << ") is higher than current version (" << vinfo.serverVersion << ").";
      LOG_TOPIC("b99ca", ERR, Logger::STARTUP)
          << "It seems like you are running ArangoDB on a database directory"
          << " that was created with a newer version of ArangoDB. Maybe this"
          << " is what you wanted but it is not supported by ArangoDB.";
      return UpgradeResult(TRI_ERROR_NO_ERROR, vinfo.status);
    }
    case VersionResult::CANNOT_PARSE_VERSION_FILE:
    case VersionResult::CANNOT_READ_VERSION_FILE:
    case VersionResult::NO_SERVER_VERSION: {
      LOG_TOPIC("bb6ba", DEBUG, Logger::STARTUP)
          << "Error reading version file";
      std::string msg = std::string("error during ") + (isUpgrade ? "upgrade" : "startup");
      return UpgradeResult(TRI_ERROR_INTERNAL, msg, vinfo.status);
    }
    case VersionResult::NO_VERSION_FILE:
      LOG_TOPIC("9ce49", DEBUG, Logger::STARTUP) << "No VERSION file found";
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

UpgradeResult methods::Upgrade::startupCoordinator(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  // this will return a hard-coded version result
  VersionResult vinfo = Version::check(&vocbase);

  VPackSlice const params = VPackSlice::emptyObjectSlice();
  return runTasks(vocbase, vinfo, params, Flags::CLUSTER_COORDINATOR_GLOBAL, Flags::DATABASE_UPGRADE);
}

/// @brief register tasks, only run once on startup
void methods::Upgrade::registerTasks(arangodb::UpgradeFeature& upgradeFeature) {
  auto& _tasks = upgradeFeature._tasks;
  TRI_ASSERT(_tasks.empty());

  // note: all tasks here should be idempotent, so that they produce the same
  // result when run again
  addTask(upgradeFeature, "createSystemCollectionsAndIndices",
          "creates all system collections including their indices",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createSystemCollectionsAndIndices);
  addTask(upgradeFeature, "createSystemStatisticsDBServer",
          "creates the statistics system collections including their indices",
      /*system*/ Flags::DATABASE_SYSTEM,
      /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_DB_SERVER_LOCAL,
      /*database*/ DATABASE_INIT | DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::createStatisticsCollectionsAndIndices);
  addTask(upgradeFeature, "addDefaultUserOther", "add default users for a new database",
          /*system*/ Flags::DATABASE_EXCEPT_SYSTEM,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_COORDINATOR_GLOBAL,
          /*database*/ DATABASE_INIT, &UpgradeTasks::addDefaultUserOther);
  addTask(upgradeFeature, "renameReplicationApplierStateFiles",
          "rename replication applier state files",
          /*system*/ Flags::DATABASE_ALL,
          /*cluster*/ Flags::CLUSTER_NONE | Flags::CLUSTER_DB_SERVER_LOCAL,
          /*database*/ DATABASE_UPGRADE | DATABASE_EXISTING,
          &UpgradeTasks::renameReplicationApplierStateFiles);

  // IResearch related upgrade tasks:
  // NOTE: db-servers do not have a dedicated collection for storing analyzers,
  //       instead they get their cache populated from coordinators
  addTask(upgradeFeature, "dropLegacyAnalyzersCollection",            // name
          "drop _iresearch_analyzers collection",     // description
          Upgrade::Flags::DATABASE_SYSTEM,            // system flags
          Upgrade::Flags::CLUSTER_COORDINATOR_GLOBAL  // cluster flags
              | Upgrade::Flags::CLUSTER_NONE,
          Upgrade::Flags::DATABASE_INIT  // database flags
              | Upgrade::Flags::DATABASE_UPGRADE,
          &UpgradeTasks::dropLegacyAnalyzersCollection  // action
  );
}

UpgradeResult methods::Upgrade::runTasks(TRI_vocbase_t& vocbase, VersionResult& vinfo,
                                         arangodb::velocypack::Slice const& params,
                                         uint32_t clusterFlag, uint32_t dbFlag) {
  auto& upgradeFeature = vocbase.server().getFeature<arangodb::UpgradeFeature>();
  auto& tasks = upgradeFeature._tasks;

  TRI_ASSERT(clusterFlag != 0 && dbFlag != 0);
  TRI_ASSERT(!tasks.empty());  // forgot to call registerTask!!
  // needs to run in superuser scope, otherwise we get errors
  ExecContextSuperuserScope scope;
  // only local should actually write a VERSION file
  bool isLocal = clusterFlag == CLUSTER_NONE || clusterFlag == CLUSTER_LOCAL ||
                 clusterFlag == CLUSTER_DB_SERVER_LOCAL;

  bool ranOnce = false;
  // execute all tasks
  for (Task const& t : tasks) {
    // check for system database
    if (t.systemFlag == DATABASE_SYSTEM && !vocbase.isSystem()) {
      LOG_TOPIC("bb1ef", DEBUG, Logger::STARTUP)
          << "Upgrade: DB not system, skipping " << t.name;
      continue;
    }
    if (t.systemFlag == DATABASE_EXCEPT_SYSTEM && vocbase.isSystem()) {
      LOG_TOPIC("fd4e0", DEBUG, Logger::STARTUP)
          << "Upgrade: DB system, skipping " << t.name;
      continue;
    }

    // check that the cluster occurs in the cluster list
    if (!(t.clusterFlags & clusterFlag)) {
      LOG_TOPIC("cc057", DEBUG, Logger::STARTUP)
          << "Upgrade: cluster mismatch, skipping " << t.name;
      continue;
    }

    auto const& it = vinfo.tasks.find(t.name);
    if (it != vinfo.tasks.end()) {
      if (it->second) {
        LOG_TOPIC("ffe7f", DEBUG, Logger::STARTUP)
            << "Upgrade: already executed, skipping " << t.name;
        continue;
      }
      vinfo.tasks.erase(it);  // in case we encounter false
    }

    // check that the database occurs in the database list
    if (!(t.databaseFlags & dbFlag)) {
      // special optimization: for local server and new database,
      // an upgrade-only task can be viewed as executed.
      if (isLocal && dbFlag == DATABASE_INIT && t.databaseFlags == DATABASE_UPGRADE) {
        vinfo.tasks.try_emplace(t.name, true);
      }
      LOG_TOPIC("346ba", DEBUG, Logger::STARTUP)
          << "Upgrade: db flag mismatch, skipping " << t.name;
      continue;
    }

    LOG_TOPIC("15144", DEBUG, Logger::STARTUP) << "Upgrade: executing " << t.name;
    try {
      bool ranTask = t.action(vocbase, params);
      if (!ranTask) {
        std::string msg =
            "executing " + t.name + " (" + t.description + ") failed.";
        LOG_TOPIC("0a886", ERR, Logger::STARTUP) << msg << " aborting upgrade procedure.";
        return UpgradeResult(TRI_ERROR_INTERNAL, msg, vinfo.status);
      }
    } catch (std::exception const& e) {
      LOG_TOPIC("022fe", ERR, Logger::STARTUP)
          << "executing " << t.name << " (" << t.description
          << ") failed with error: " << e.what() << ". aborting upgrade procedure.";
      return UpgradeResult(TRI_ERROR_FAILED, e.what(), vinfo.status);
    }

    // remember we already executed this one
    vinfo.tasks.try_emplace(t.name, true);

    if (isLocal) {  // save after every task for resilience
      auto res = methods::Version::write(&vocbase, vinfo.tasks, /*sync*/ false);

      if (res.fail()) {
        return UpgradeResult(res.errorNumber(), res.errorMessage(), vinfo.status);
      }

      ranOnce = true;
    }
  }

  if (isLocal) {  // no need to write this for cluster bootstrap
    // save even if no tasks were executed
    LOG_TOPIC("e5a77", DEBUG, Logger::STARTUP)
        << "Upgrade: writing VERSION file";
    auto res = methods::Version::write(&vocbase, vinfo.tasks, /*sync*/ ranOnce);

    if (res.fail()) {
      return UpgradeResult(res.errorNumber(), res.errorMessage(), vinfo.status);
    }
  }

  return UpgradeResult(TRI_ERROR_NO_ERROR, vinfo.status);
}
