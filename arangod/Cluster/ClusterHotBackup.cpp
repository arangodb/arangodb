////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterHotBackup.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/UserManagerImpl.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "StorageEngine/HotBackupCommon.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackup/RocksDBHotBackup.h"
#endif

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <set>
#include <thread>

#include <absl/strings/str_cat.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;

using Helper = arangodb::basics::VelocyPackHelper;

namespace arangodb {

std::string const apiStr("/_admin/backup/");

arangodb::Result hotBackupList(
    network::ConnectionPool* pool, std::vector<ServerID> const& dbServers,
    VPackSlice const idSlice,
    std::unordered_map<std::string, BackupMeta>& hotBackups,
    VPackBuilder& plan) {
  TRI_ASSERT(pool);
  hotBackups.clear();
  TRI_ASSERT(idSlice.isArray() || idSlice.isString() || idSlice.isNone());

  std::map<std::string, std::vector<BackupMeta>> dbsBackups;

  VPackBufferUInt8 body;
  VPackBuilder b(body);
  b.openObject();
  if (!idSlice.isNone()) {
    b.add("id", idSlice);
  }
  b.close();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::string const url = apiStr + "list";

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());
  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  size_t nrGood = 0;
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();
    if (!r.ok()) {
      continue;
    }
    if (r.response().checkStatus({fuerte::StatusOK, fuerte::StatusCreated,
                                  fuerte::StatusAccepted,
                                  fuerte::StatusNoContent})) {
      nrGood++;
    }
  }

  LOG_TOPIC("410a1", DEBUG, Logger::BACKUP)
      << "Got " << nrGood << " of " << futures.size()
      << " lists of local backups";

  // Any error if no id presented
  if (idSlice.isNone() && nrGood < futures.size()) {
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_DBSERVERS_AWOL,
        std::string("not all db servers could be reached for backup listing"));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();
    if (!r.ok()) {
      continue;
    }
    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              std::string("result to list request to ") +
                                  r.destination + " not an object");
    }

    if (resSlice.get(StaticStrings::Error).getBoolean()) {
      auto res =
          ::ErrorCode{resSlice.get(StaticStrings::ErrorNum).getNumber<int>()};
      return arangodb::Result(
          res, resSlice.get(StaticStrings::ErrorMessage).copyString());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                              std::string("invalid response ") +
                                  resSlice.toJson() + "from " + r.destination);
    }

    resSlice = resSlice.get("result");

    if (!resSlice.hasKey("list") || !resSlice.get("list").isObject()) {
      continue;
    }

    if (!idSlice.isNone() && plan.slice().isNone()) {
      if (!resSlice.hasKey("agency-dump") ||
          !resSlice.get("agency-dump").isArray() ||
          resSlice.get("agency-dump").length() != 1) {
        return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                                std::string("result ") + resSlice.toJson() +
                                    " is missing agency dump");
      }
      plan.add(resSlice.get("agency-dump")[0]);
    }

    for (auto [key, value] : VPackObjectIterator(resSlice.get("list"))) {
      auto meta = [&] {
        if (auto error = value.get(arangodb::StaticStrings::ErrorNum);
            !error.isNone()) {
          return ResultT<BackupMeta>::success(BackupMeta::fromError(
              key.copyString(),
              VelocyPackHelper::getStringValue(resSlice, "server", ""), value));
        }
        return BackupMeta::fromSlice(value);
      }();
      if (meta.ok()) {
        dbsBackups[key.copyString()].push_back(std::move(meta.get()));
      }
    }
  }

  for (auto& i : dbsBackups) {
    // check if the backup is on all dbservers
    bool valid = true;
    bool available = true;

    // check here that the backups are all made with the same version
    std::string version;
    size_t totalSize = 0;
    size_t totalFiles = 0;
    std::unordered_map<std::string, result::Error> errors;

    for (BackupMeta& meta : i.second) {
      if (!meta._isAvailable) {
        available = false;
        errors.merge(meta._errors);
        continue;
      }
      if (version.empty()) {
        version = meta._version;
      } else {
        if (version != meta._version) {
          LOG_TOPIC("aaaaa", WARN, Logger::BACKUP)
              << "Backup " << meta._id
              << " has different versions accross dbservers: " << version
              << " and " << meta._version;
          valid = false;
          break;
        }
      }
      totalSize += meta._sizeInBytes;
      totalFiles += meta._nrFiles;
    }

    if (valid) {  // backup is identical on all servers
      BackupMeta& front = i.second.front();
      front._sizeInBytes = totalSize;
      front._nrFiles = totalFiles;
      front._serverId = "";  // makes no sense for whole cluster
      front._isAvailable = available && i.second.size() == dbServers.size() &&
                           i.second.size() == front._nrDBServers;
      front._nrPiecesPresent = static_cast<unsigned int>(i.second.size());
      front._errors = std::move(errors);
      hotBackups.insert(std::make_pair(front._id, front));
    }
  }

  return arangodb::Result();
}

/**
 * @brief Match existing servers with those in the backup
 *
 * @param  agencyDump
 * @param  my own DB server list
 * @param  Result container
 */
arangodb::Result matchBackupServers(VPackSlice const agencyDump,
                                    std::vector<ServerID> const& dbServers,
                                    std::map<ServerID, ServerID>& match) {
  std::vector<std::string> ap{"arango", "Plan", "DBServers"};

  if (!agencyDump.hasKey(ap)) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "agency dump must contain key DBServers");
  }
  auto planServers = agencyDump.get(ap);

  return matchBackupServersSlice(planServers, dbServers, match);
}

arangodb::Result matchBackupServersSlice(VPackSlice const planServers,
                                         std::vector<ServerID> const& dbServers,
                                         std::map<ServerID, ServerID>& match) {
  // LOG_TOPIC("711d8", DEBUG, Logger::BACKUP) << "matching db servers between
  // snapshot: " <<
  //  planServers.toJson() << " and this cluster's db servers " << dbServers;

  if (!planServers.isObject()) {
    return Result(TRI_ERROR_HOT_BACKUP_INTERNAL,
                  "agency dump's arango.Plan.DBServers must be object");
  }

  if (dbServers.size() < planServers.length()) {
    return Result(TRI_ERROR_BACKUP_TOPOLOGY,
                  std::string("number of db servers in the backup (") +
                      std::to_string(planServers.length()) +
                      ") and in this cluster (" +
                      std::to_string(dbServers.size()) + ") do not match");
  }

  // Clear match container
  match.clear();

  // Local copy of my servers
  // Note that we use a `std::set` here for the following reasons:
  //  - this function is supposed to return a canonical result, which
  //    does not depend on the order of the input in `planServers` or
  //    `dbServers`, therefore we use a sorted set here and a sorted
  //    map as the result.
  //  - Performance does not matter at all, this method is called for
  //    hotbackup download and hotbackup restore, both methods are
  //    using considerable amounts of time. The server lists are usually
  //    quite small (3 or 5 or a few dozen at most).
  // So please do not "optimize" this.
  std::set<std::string> localCopy;
  std::copy(dbServers.begin(), dbServers.end(),
            std::inserter(localCopy, localCopy.end()));
  // dbServers should be a list of pairwise different servers, this
  // is usually ensured since it is a vector manufactured from the keys
  // of a map:
  TRI_ASSERT(localCopy.size() == dbServers.size());

  // Skip all direct matching names in pair and remove them from localCopy.
  // If later a server does not occur it means that no translation has to
  // happen for it.
  for (auto planned : VPackObjectIterator(planServers)) {
    auto const plannedStr = planned.key.copyString();
    auto it = localCopy.find(plannedStr);
    if (it != localCopy.end()) {
      localCopy.erase(it);
    } else {
      match.try_emplace(plannedStr, std::string());
    }
  }
  // At this stage, we know the following:
  //  - initially, dbServers had at least as many servers as planServers
  //  - initially, localCopy was a perfect copy of dbServers
  //  - now, for every element of planServers, which is not contained in
  //    localCopy, we put an element in match, each element of planServers,
  //    which is in localCopy, is removed from localCopy and no entry is
  //    added to match.
  // Therefore, localCopy has at least as many entries as match and we can
  // just blindly run the iterator:
  TRI_ASSERT(match.size() <= localCopy.size());
  auto it2 = localCopy.begin();
  for (auto& m : match) {  // alphabetical order!
    m.second = *it2++;     // alphabetical order, too!
  }

  LOG_TOPIC("a201e", DEBUG, Logger::BACKUP) << "DB server matches: " << match;

  return arangodb::Result();
}

arangodb::Result controlMaintenanceFeature(
    network::ConnectionPool* pool, std::string const& command,
    std::string const& backupId, std::vector<ServerID> const& dbServers) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("execute", VPackValue(command));
    builder.add("reason", VPackValue("backup"));
    builder.add("duration", VPackValue(30));
    builder.add("id", VPackValue(backupId));
  }

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());
  std::string const url = "/_admin/actions";

  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("3d080", DEBUG, Logger::BACKUP)
      << "Attempting to execute " << command
      << " maintenance features for hot backup id " << backupId << " using "
      << builder.toJson();

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          absl::StrCat("Communication error while executing ", command,
                       " maintenance on ", r.destination, ": ",
                       r.combinedResult().errorMessage()));
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject() || !resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean()) {
      // Response has invalid format
      return arangodb::Result(
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          absl::StrCat("result of executing ", command,
                       " request to maintenance feature on ", r.destination,
                       " is invalid"));
    }

    if (resSlice.get(StaticStrings::Error).getBoolean()) {
      return arangodb::Result(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          absl::StrCat("failed to execute ", command,
                       " on maintenance feature for ", backupId, " on server ",
                       r.destination));
    }

    LOG_TOPIC("d7e7c", DEBUG, Logger::BACKUP)
        << "maintenance is paused on " << r.destination;
  }

  return arangodb::Result();
}

arangodb::Result restoreOnDBServers(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::vector<std::string> const& dbServers,
                                    std::string& previous, bool ignoreVersion) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder o(&builder);
    builder.add("id", VPackValue(backupId));
    builder.add("ignoreVersion", VPackValue(ignoreVersion));
  }

  std::string const url = apiStr + "restore";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbServers.size());

  for (auto const& dbServer : dbServers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("37960", DEBUG, Logger::BACKUP) << "Restoring backup " << backupId;

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      // oh-oh cluster is in a bad state
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          absl::StrCat("Communication error list backups on ", r.destination,
                       ": ", r.combinedResult().errorMessage()));
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              absl::StrCat("result to restore request ",
                                           r.destination, "not an object"));
    }

    if (!resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean() ||
        resSlice.get(StaticStrings::Error).getBoolean()) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              std::string("failed to restore ") + backupId +
                                  " on server " + r.destination + ": " +
                                  resSlice.toJson());
    }

    if (!resSlice.hasKey("result") || !resSlice.get("result").isObject()) {
      return arangodb::Result(
          TRI_ERROR_HOT_RESTORE_INTERNAL,
          std::string("failed to restore ") + backupId + " on server " +
              r.destination +
              " as response is missing result object: " + resSlice.toJson());
    }

    auto result = resSlice.get("result");

    if (!result.hasKey("previous") || !result.get("previous").isString()) {
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              std::string("failed to restore ") + backupId +
                                  " on server " + r.destination);
    }

    previous = result.get("previous").copyString();
    LOG_TOPIC("9a5c4", DEBUG, Logger::BACKUP)
        << "received failsafe name " << previous << " from db server "
        << r.destination;
  }

  LOG_TOPIC("755a2", DEBUG, Logger::BACKUP)
      << "Restored " << backupId << " successfully";

  return arangodb::Result();
}

arangodb::Result applyDBServerMatchesToPlan(
    VPackSlice const plan, std::map<ServerID, ServerID> const& matches,
    VPackBuilder& newPlan) {
  std::function<void(VPackSlice const, std::map<ServerID, ServerID> const&,
                     bool)>
      replaceDBServer;
  /*
   * This recursive function replaces all occurences of DBServer names with
   * their handed replacement map. In Replication2 also remove the currentTerm
   * entry, to enforce leader election.
   */
  replaceDBServer = [&newPlan, &replaceDBServer](
                        VPackSlice const s,
                        std::map<ServerID, ServerID> const& matches,
                        bool inReplicatedLogs) {
    if (s.isObject()) {
      VPackObjectBuilder o(&newPlan);
      for (auto it : VPackObjectIterator(s)) {
        newPlan.add(it.key);
        if (it.key.isEqualString("ReplicatedLogs")) {
          replaceDBServer(it.value, matches, true);
        } else if (inReplicatedLogs && it.key.isEqualString("currentTerm")) {
          newPlan.add(VPackSlice::emptyObjectSlice());
        } else {
          replaceDBServer(it.value, matches, inReplicatedLogs);
        }
      }
    } else if (s.isArray()) {
      VPackArrayBuilder a(&newPlan);
      for (VPackSlice it : VPackArrayIterator(s)) {
        replaceDBServer(it, matches, inReplicatedLogs);
      }
    } else {
      bool swapped = false;
      if (s.isString()) {
        for (auto const& match : matches) {
          if (s.isString() && s.isEqualString(match.first)) {
            newPlan.add(VPackValue(match.second));
            swapped = true;
            break;
          }
        }
      }
      if (!swapped) {
        newPlan.add(s);
      }
    }
  };

  replaceDBServer(plan, matches, false);

  return arangodb::Result();
}

arangodb::Result hotRestoreCoordinator(ClusterFeature& feature,
                                       VPackSlice const payload,
                                       VPackBuilder& report) {
  // 1. Find local backup with id
  //    - fail if not found
  // 2. Match db servers
  //    - fail if not matching
  // 3. Check if they have according backup with backupId
  //    - fail if not
  // 4. Stop maintenance feature on all db servers
  // 5. a. Replay agency
  //    b. Initiate DB server restores
  // 6. Wait until all dbservers up again and good
  //    - fail if not

  VPackSlice idSlice;
  if (!payload.isObject() || !(idSlice = payload.get("id")).isString()) {
    events::RestoreHotbackup("", TRI_ERROR_BAD_PARAMETER);
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        "restore payload must be an object with string attribute 'id'");
  }
  TRI_ASSERT(idSlice.isString());

  bool ignoreVersion =
      payload.hasKey("ignoreVersion") && payload.get("ignoreVersion").isTrue();

  std::string const backupId = idSlice.copyString();
  VPackBuilder plan;
  ClusterInfo& ci = feature.clusterInfo();

  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // shutdown, leave here
    return TRI_ERROR_SHUTTING_DOWN;
  }

  std::vector<ServerID> dbServers = ci.getCurrentDBServers();
  std::unordered_map<std::string, BackupMeta> list;

  auto result = hotBackupList(pool, dbServers, idSlice, list, plan);
  if (!result.ok()) {
    LOG_TOPIC("ed4dd", ERR, Logger::BACKUP)
        << "failed to find backup " << backupId
        << " on all db servers: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }
  if (list.empty()) {
    events::RestoreHotbackup(backupId, TRI_ERROR_HTTP_NOT_FOUND);
    return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                            "result is missing backup list");
  }

  if (plan.slice().isNone()) {
    LOG_TOPIC("54b9a", ERR, Logger::BACKUP)
        << "failed to find agency dump for " << backupId
        << " on any db server: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  TRI_ASSERT(list.size() == 1);
  BackupMeta& meta = list.begin()->second;
  if (!meta._isAvailable) {
    LOG_TOPIC("ed4df", ERR, Logger::BACKUP)
        << "backup not available" << backupId;
    events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
    return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                            "backup not available for restore");
  }

  // Check if the version matches the current version
  if (!ignoreVersion) {
    TRI_ASSERT(list.size() == 1);
    using arangodb::methods::Version;
    using arangodb::methods::VersionResult;
#ifdef USE_ENTERPRISE
    // Will never be called in Community Edition
    bool autoUpgradeNeeded;  // not actually used
    if (!RocksDBHotBackup::versionTestRestore(meta._version,
                                              autoUpgradeNeeded)) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Version mismatch");
    }
#endif
  }

  // Match my db servers to those in the backups's agency dump
  std::map<ServerID, ServerID> matches;
  result = matchBackupServers(plan.slice(), dbServers, matches);
  if (!result.ok()) {
    LOG_TOPIC("5a746", ERR, Logger::BACKUP)
        << "failed to match db servers: " << result.errorMessage();
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Apply matched servers to create new plan, if any matches to be done,
  // else just take
  VPackBuilder newPlan;
  if (!matches.empty()) {
    result = applyDBServerMatchesToPlan(plan.slice(), matches, newPlan);
    if (!result.ok()) {
      events::RestoreHotbackup(backupId, result.errorNumber());
      return result;
    }
  }

  // Pause maintenance feature everywhere, fail, if not succeeded everywhere
  result = controlMaintenanceFeature(pool, "pause", backupId, dbServers);
  if (!result.ok()) {
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Enact new plan upon the agency
  result = (matches.empty()) ? ci.agencyReplan(plan.slice())
                             : ci.agencyReplan(newPlan.slice());
  if (!result.ok()) {
    // We ignore the result of the Proceed here.
    // In case one of the servers does not proceed now, it will automatically
    // reactivate maintenance after 30s.
    std::ignore =
        controlMaintenanceFeature(pool, "proceed", backupId, dbServers);
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // Now I will have to wait for the plan to trickle down
  std::this_thread::sleep_for(std::chrono::seconds(5));

  // We keep the currently registered timestamps in Current/ServersRegistered,
  // such that we can wait until all have reregistered and are up:

  ci.loadCurrentDBServers();
  auto const preServersKnown = ci.rebootIds();

  // Restore all db servers
  std::string previous;
  result =
      restoreOnDBServers(pool, backupId, dbServers, previous, ignoreVersion);
  if (!result.ok()) {  // This is disaster!
    events::RestoreHotbackup(backupId, result.errorNumber());
    return result;
  }

  // no need to keep connections to shut-down servers, they auto close when
  // unused
  if (pool) {
    pool->drainConnections();
  }

  auto startTime = std::chrono::steady_clock::now();
  while (true) {  // will be left by a timeout
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (feature.server().isStopping()) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Shutdown of coordinator!");
    }
    if (std::chrono::steady_clock::now() - startTime >
        std::chrono::minutes(15)) {
      events::RestoreHotbackup(backupId, TRI_ERROR_HOT_RESTORE_INTERNAL);
      return arangodb::Result(TRI_ERROR_HOT_RESTORE_INTERNAL,
                              "Not all DBservers came back in time!");
    }
    ci.loadCurrentDBServers();
    auto const postServersKnown = ci.rebootIds();
    if (ci.getCurrentDBServers().size() < dbServers.size()) {
      LOG_TOPIC("8dce7", INFO, Logger::BACKUP)
          << "Waiting for all db servers to return";
      continue;
    }

    // Check timestamps of all dbservers:
    size_t good = 0;  // Count restarted servers
    for (auto const& dbs : dbServers) {
      if (postServersKnown.at(dbs).rebootId !=
          preServersKnown.at(dbs).rebootId) {
        ++good;
      }
    }
    LOG_TOPIC("8dc7e", INFO, Logger::BACKUP)
        << "Backup restore: So far " << good << "/" << dbServers.size()
        << " dbServers have reregistered.";
    if (good >= dbServers.size()) {
      break;
    }
  }

  // and Wait for Shards to decide on a leader
  ci.syncWaitForAllShardsToEstablishALeader();

  // After we restored the _users collection we want to trigger a global reload
  // to trigger a cache reload in the whole Cluster.
  auto* authFeature = AuthenticationFeature::instance();
  if (authFeature != nullptr && authFeature->userManager() != nullptr) {
    authFeature->userManager()->triggerCacheRevalidation();
  } else {
    // We apparently do not have Authentication on this server, but we still
    // should increase the UserVersion to be consistent.
    auth::UserManagerImpl::triggerGlobalReload(authFeature->server());
  }

  {
    VPackObjectBuilder o(&report);
    report.add("previous", VPackValue(previous));
    report.add("isCluster", VPackValue(true));
  }
  events::RestoreHotbackup(backupId, TRI_ERROR_NO_ERROR);
  return arangodb::Result();
}

std::vector<std::string> lockPath =
    std::vector<std::string>{"result", "lockId"};

arangodb::Result lockServersTrxCommit(network::ConnectionPool* pool,
                                      std::string const& backupId,
                                      std::vector<ServerID> const& servers,
                                      double lockWait,
                                      std::vector<ServerID>& lockedServers) {
  // Make sure all servers have the backup with backup Id

  std::string const url = apiStr + "lock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
    lock.add("timeout", VPackValue(lockWait));
    // unlock timeout for commit lock on coordinator
    lock.add("unlockTimeout", VPackValue(30.0 + lockWait));
  }

  LOG_TOPIC("707ed", DEBUG, Logger::BACKUP)
      << "Trying to acquire global transaction locks using body "
      << lock.toJson();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& server : servers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Post, url, body, reqOpts));
  }

  // Now listen to the results and report the aggregated final result:
  arangodb::Result finalRes(TRI_ERROR_NO_ERROR);
  auto reportError = [&](::ErrorCode c, std::string const& m) {
    if (finalRes.ok()) {
      finalRes = arangodb::Result(c, m);
    } else {
      // If we see at least one TRI_ERROR_LOCAL_LOCK_FAILED it is a failure
      // if all errors are TRI_ERROR_LOCK_TIMEOUT, then we report this and
      // this will lead to a retry:
      if (finalRes.is(TRI_ERROR_LOCAL_LOCK_FAILED)) {
        c = TRI_ERROR_LOCAL_LOCK_FAILED;
      }
      finalRes =
          arangodb::Result(c, absl::StrCat(finalRes.errorMessage(), ", ", m));
    }
  };
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
      continue;
    }
    VPackSlice slc = r.slice();

    if (!slc.isObject() || !slc.hasKey(StaticStrings::Error) ||
        !slc.get(StaticStrings::Error).isBoolean()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to freeze transactions for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    if (slc.get(StaticStrings::Error).getBoolean()) {
      LOG_TOPIC("f4b8f", DEBUG, Logger::BACKUP)
          << "failed to acquire lock from " << r.destination << ": "
          << slc.toJson();
      auto errorNum = slc.get(StaticStrings::ErrorNum).getNumber<int>();
      auto err = ::ErrorCode{errorNum};
      if (err == TRI_ERROR_LOCK_TIMEOUT) {
        reportError(err, slc.get(StaticStrings::ErrorMessage).copyString());
        continue;
      }
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("lock was denied from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    if (!slc.hasKey(lockPath) || !slc.get(lockPath).isNumber() ||
        !slc.hasKey("result") || !slc.get("result").isObject()) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId, ": ", slc.toJson()));
      continue;
    }

    uint64_t lockId = 0;
    try {
      lockId = slc.get(lockPath).getNumber<uint64_t>();
      LOG_TOPIC("14457", DEBUG, Logger::BACKUP)
          << "acquired lock from " << r.destination << " for backupId "
          << backupId << " with lockId " << lockId;
    } catch (std::exception const& e) {
      reportError(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("invalid response from ", r.destination,
                       " when trying to get lockId for hot backup ", backupId,
                       ": ", slc.toJson(), ", msg: ", e.what()));
      continue;
    }

    lockedServers.push_back(
        r.destination.substr(strlen("server:"), std::string::npos));
  }

  if (finalRes.ok()) {
    LOG_TOPIC("c1869", DEBUG, Logger::BACKUP)
        << "acquired transaction locks on all coordinators";
  } else {
    LOG_TOPIC("8226a", DEBUG, Logger::BACKUP)
        << "unable to acquire transaction locks on all coordinators: "
        << finalRes.errorMessage();
  }

  return finalRes;
}

arangodb::Result unlockServersTrxCommit(
    network::ConnectionPool* pool, std::string const& backupId,
    std::vector<ServerID> const& lockedServers) {
  LOG_TOPIC("2ba8f", DEBUG, Logger::BACKUP)
      << "best effort attempt to kill all locks on coordinators "
      << lockedServers;

  // Make sure all servers have the backup with backup Id

  std::string const url = apiStr + "unlock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
  }

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(lockedServers.size());

  for (auto const& server : lockedServers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Post, url, body, reqOpts));
  }

  auto responses = futures::collectAll(std::move(futures)).waitAndGet();

  Result res;
  for (auto const& tryRes : responses) {
    network::Response const& r = tryRes.get();
    if (r.combinedResult().fail() && res.ok()) {
      res = r.combinedResult();
    }
  }

  LOG_TOPIC("48510", DEBUG, Logger::BACKUP)
      << "killing all locks on coordinators resulted in: "
      << res.errorMessage();

  // return value is ignored by callers, but we'll return our status
  // anyway.
  return res;
}

std::vector<std::string> idPath{"result", "id"};

arangodb::Result hotBackupDBServers(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::string const& timeStamp,
                                    std::vector<ServerID> servers,
                                    VPackSlice agencyDump, bool force,
                                    BackupMeta& meta) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("label", VPackValue(backupId));
    builder.add("agency-dump", agencyDump);
    builder.add("timestamp", VPackValue(timeStamp));
    builder.add("allowInconsistent", VPackValue(force));
    builder.add("nrDBServers", VPackValue(servers.size()));
  }

  std::string const url = apiStr + "create";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& dbServer : servers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("478ef", DEBUG, Logger::BACKUP)
      << "Inquiring about backup " << backupId;

  // Now listen to the results:
  size_t totalSize = 0;
  size_t totalFiles = 0;
  std::vector<std::string> secretHashes;
  std::string version;
  bool sizeValid = true;
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          std::string("Communication error list backups on ") + r.destination);
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject() || !resSlice.hasKey("result")) {
      // Response has invalid format
      return arangodb::Result(
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          std::string("result to take snapshot on ") + r.destination +
              " not an object or has no 'result' attribute: " +
              resSlice.toJson());
    }
    resSlice = resSlice.get("result");

    // TODO: shouldn't the id be checked ?
    VPackSlice value = resSlice.get(BackupMeta::ID);
    if (!value.isString()) {
      LOG_TOPIC("6240a", ERR, Logger::BACKUP)
          << "DB server " << r.destination << "is missing backup " << backupId;
      return arangodb::Result(TRI_ERROR_FILE_NOT_FOUND,
                              std::string("no backup with id ") + backupId +
                                  " on server " + r.destination);
    }

    value = resSlice.get(BackupMeta::SECRETHASH);
    if (value.isArray()) {
      for (VPackSlice hash : VPackArrayIterator(value)) {
        if (hash.isString()) {
          secretHashes.push_back(hash.copyString());
        }
      }
    }

    if (resSlice.hasKey(BackupMeta::SIZEINBYTES)) {
      totalSize +=
          Helper::getNumericValue<size_t>(resSlice, BackupMeta::SIZEINBYTES, 0);
    } else {
      sizeValid = false;
    }
    if (resSlice.hasKey(BackupMeta::NRFILES)) {
      totalFiles +=
          Helper::getNumericValue<size_t>(resSlice, BackupMeta::NRFILES, 0);
    } else {
      sizeValid = false;
    }
    if (version.empty() && resSlice.hasKey(BackupMeta::VERSION)) {
      VPackSlice verSlice = resSlice.get(BackupMeta::VERSION);
      if (verSlice.isString()) {
        version = verSlice.copyString();
      }
    }

    LOG_TOPIC("b370d", DEBUG, Logger::BACKUP)
        << r.destination << " created local backup "
        << resSlice.get(BackupMeta::ID).stringView();
  }

  // remove duplicate hashes
  std::sort(secretHashes.begin(), secretHashes.end());
  secretHashes.erase(std::unique(secretHashes.begin(), secretHashes.end()),
                     secretHashes.end());

  if (sizeValid) {
    meta = BackupMeta(backupId, version, timeStamp, secretHashes, totalSize,
                      totalFiles, static_cast<unsigned int>(servers.size()), "",
                      force);
  } else {
    meta = BackupMeta(backupId, version, timeStamp, secretHashes, 0, 0,
                      static_cast<unsigned int>(servers.size()), "", force);
    LOG_TOPIC("54265", WARN, Logger::BACKUP)
        << "Could not determine total size of backup with id '" << backupId
        << "'!";
  }
  LOG_TOPIC("5c5e9", DEBUG, Logger::BACKUP)
      << "Have created backup " << backupId;

  return arangodb::Result();
}

/**
 * @brief delete all backups with backupId from the db servers
 */
arangodb::Result removeLocalBackups(network::ConnectionPool* pool,
                                    std::string const& backupId,
                                    std::vector<ServerID> const& servers,
                                    std::vector<std::string>& deleted) {
  VPackBufferUInt8 body;
  VPackBuilder builder(body);
  {
    VPackObjectBuilder b(&builder);
    builder.add("id", VPackValue(backupId));
  }

  std::string const url = apiStr + "delete";

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;

  std::vector<Future<network::Response>> futures;
  futures.reserve(servers.size());

  for (auto const& dbServer : servers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + dbServer,
                                                   fuerte::RestVerb::Post, url,
                                                   body, reqOpts));
  }

  LOG_TOPIC("33e85", DEBUG, Logger::BACKUP) << "Deleting backup " << backupId;

  size_t notFoundCount = 0;

  // Now listen to the results:
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          network::fuerteToArangoErrorCode(r),
          std::string("Communication error while deleting backup") + backupId +
              " on " + r.destination);
    }

    VPackSlice resSlice = r.slice();
    if (!resSlice.isObject()) {
      // Response has invalid format
      return arangodb::Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                              std::string("failed to remove backup from ") +
                                  r.destination + ", result not an object");
    }

    if (!resSlice.hasKey(StaticStrings::Error) ||
        !resSlice.get(StaticStrings::Error).isBoolean() ||
        resSlice.get(StaticStrings::Error).getBoolean()) {
      auto errorNum = resSlice.get(StaticStrings::ErrorNum).getNumber<int>();
      auto res = ::ErrorCode{errorNum};

      if (res == TRI_ERROR_FILE_NOT_FOUND) {
        notFoundCount += 1;
        continue;
      }

      std::string errorMsg =
          std::string("failed to delete backup ") + backupId + " on " +
          r.destination + ":" +
          resSlice.get(StaticStrings::ErrorMessage).copyString() + " (" +
          std::to_string(errorNum) + ")";

      LOG_TOPIC("9b94f", ERR, Logger::BACKUP) << errorMsg;
      return arangodb::Result(res, errorMsg);
    }
  }

  LOG_TOPIC("1b318", DEBUG, Logger::BACKUP)
      << "removeLocalBackups: notFoundCount = " << notFoundCount << " "
      << servers.size();

  if (notFoundCount == servers.size()) {
    return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND,
                            "Backup " + backupId + " not found.");
  }

  deleted.emplace_back(backupId);
  LOG_TOPIC("04e97", DEBUG, Logger::BACKUP)
      << "Have located and deleted " << backupId;

  return arangodb::Result();
}

std::vector<std::string> const versionPath =
    std::vector<std::string>{"arango", "Plan", "Version"};

arangodb::Result hotbackupAsyncLockCoordinatorsTransactions(
    network::ConnectionPool* pool, std::string const& backupId,
    std::vector<ServerID> const& coordinators, double const& lockWait,
    std::unordered_map<std::string, std::string>& serverLockIds) {
  std::string const url = apiStr + "lock";

  VPackBufferUInt8 body;
  VPackBuilder lock(body);
  {
    VPackObjectBuilder o(&lock);
    lock.add("id", VPackValue(backupId));
    lock.add("timeout", VPackValue(lockWait));
    lock.add("unlockTimeout", VPackValue(5.0 + lockWait));
  }

  LOG_TOPIC("707ee", DEBUG, Logger::BACKUP)
      << "Trying to acquire async global transaction locks using body "
      << lock.toJson();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(coordinators.size());

  for (auto const& coordinator : coordinators) {
    network::Headers headers;
    headers.emplace(StaticStrings::Async, "store");
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + coordinator, fuerte::RestVerb::Post, url, body,
        reqOpts, std::move(headers)));
  }

  // Perform the requests
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
    }

    if (r.statusCode() != 202) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("lock was denied from ", r.destination,
                       " when trying to check for lockId for hot backup ",
                       backupId));
    }

    bool hasJobID;
    std::string jobId =
        r.response().header.metaByKey(StaticStrings::AsyncId, hasJobID);
    if (!hasJobID) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("lock was denied from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId);
    }

    serverLockIds[r.serverId()] = jobId;
  }

  return arangodb::Result();
}

arangodb::Result hotbackupWaitForLockCoordinatorsTransactions(
    network::ConnectionPool* pool, std::string const& backupId,
    std::unordered_map<std::string, std::string>& serverLockIds,
    std::vector<ServerID>& lockedServers, double const& lockWait) {
  // query all remaining jobs here

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(lockWait + 5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(serverLockIds.size());

  VPackBufferUInt8 body;  // empty body
  for (auto const& lock : serverLockIds) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + lock.first, fuerte::RestVerb::Put,
        "/_api/job/" + lock.second, body, reqOpts));
  }

  // Perform the requests
  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.waitAndGet();

    if (r.fail()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          absl::StrCat("Communication error locking transactions on ",
                       r.destination, ": ", r.combinedResult().errorMessage()));
    }
    // continue on 204 No Content
    if (r.statusCode() == 204) {
      continue;
    }

    VPackSlice slc = r.slice();

    if (!slc.isObject() || !slc.hasKey(StaticStrings::Error) ||
        !slc.get(StaticStrings::Error).isBoolean()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to freeze transactions for hot backup " + backupId +
              ": " + slc.toJson());
    }

    if (slc.get(StaticStrings::Error).getBoolean()) {
      LOG_TOPIC("d7a8a", DEBUG, Logger::BACKUP)
          << "failed to acquire lock from " << r.destination << ": "
          << slc.toJson();
      auto errorNum =
          ::ErrorCode{slc.get(StaticStrings::ErrorNum).getNumber<int>()};
      if (errorNum == TRI_ERROR_LOCK_TIMEOUT) {
        return arangodb::Result(
            errorNum, slc.get(StaticStrings::ErrorMessage).copyString());
      }
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("lock was denied from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId +
              ": " + slc.toJson());
    }

    if (!slc.hasKey(lockPath) || !slc.get(lockPath).isNumber() ||
        !slc.hasKey("result") || !slc.get("result").isObject()) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to check for lockId for hot backup " + backupId +
              ": " + slc.toJson());
    }

    uint64_t lockId = 0;
    try {
      lockId = slc.get(lockPath).getNumber<uint64_t>();
      LOG_TOPIC("144f5", DEBUG, Logger::BACKUP)
          << "acquired lock from " << r.destination << " for backupId "
          << backupId << " with lockId " << lockId;
    } catch (std::exception const& e) {
      return arangodb::Result(
          TRI_ERROR_LOCAL_LOCK_FAILED,
          std::string("invalid response from ") + r.destination +
              " when trying to get lockId for hot backup " + backupId + ": " +
              slc.toJson() + ", msg: " + e.what());
    }

    lockedServers.push_back(r.serverId());
    serverLockIds.erase(r.serverId());
  }

  return arangodb::Result();
}

void hotbackupCancelAsyncLocks(
    network::ConnectionPool* pool,
    std::unordered_map<std::string, std::string>& dbserverLockIds,
    std::vector<ServerID>& lockedServers) {
  // abort all the jobs
  // if a job can not be aborted, assume it has started and add the server to
  // the unlock list

  // cancel all remaining jobs here

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  reqOpts.timeout = network::Timeout(5.0);

  std::vector<Future<network::Response>> futures;
  futures.reserve(dbserverLockIds.size());

  VPackBufferUInt8 body;  // empty body
  for (auto const& lock : dbserverLockIds) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + lock.first, fuerte::RestVerb::Put,
        "/_api/job/" + lock.second + "/cancel", body, reqOpts));
  }
}

arangodb::Result hotBackupCoordinator(ClusterFeature& feature,
                                      VPackSlice const payload,
                                      VPackBuilder& report) {
  // ToDo: mode
  // HotBackupMode const mode = CONSISTENT;

  /*
    Suggestion for procedure for cluster hotbackup:
    1. Check that ToDo and Pending are empty, if not, delay, back to 1.
       Timeout for giving up.
    2. Stop Supervision, remember if it was on or not.
    3. Check that ToDo and Pending are empty, if not, start Supervision again,
       back to 1.
       4. Get Plan, will have no resigned leaders.
    5. Stop Transactions, if this does not work in time, restore Supervision
       and give up.
    6. Take hotbackups everywhere, if any fails, all failed.
    7. Resume Transactions.
    8. Resume Supervision if it was on.
    9. Keep Maintenance on dbservers on all the time.
  */

  try {
    if (!payload.isNone() &&
        (!payload.isObject() ||
         (payload.hasKey("label") && !payload.get("label").isString()) ||
         (payload.hasKey("timeout") && !payload.get("timeout").isNumber()) ||
         (payload.hasKey("allowInconsistent") &&
          !payload.get("allowInconsistent").isBoolean()) ||
         (payload.hasKey("force") && !payload.get("force").isBoolean()))) {
      events::CreateHotbackup("", TRI_ERROR_BAD_PARAMETER);
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, BAD_PARAMS_CREATE);
    }

    bool allowInconsistent =
        payload.isNone() ? false : payload.get("allowInconsistent").isTrue();
    bool force = payload.isNone() ? false : payload.get("force").isTrue();

    std::string const backupId =
        (payload.isObject() && payload.hasKey("label"))
            ? payload.get("label").copyString()
            : to_string(boost::uuids::random_generator()());
    std::string timeStamp = timepointToString(std::chrono::system_clock::now());

    double timeout = (payload.isObject() && payload.hasKey("timeout"))
                         ? payload.get("timeout").getNumber<double>()
                         : 120.;
    // unreasonably short even under allowInconsistent
    if (timeout < 2.5) {
      auto const tmp = timeout;
      timeout = 2.5;
      LOG_TOPIC("67ae2", WARN, Logger::BACKUP)
          << "Backup timeout " << tmp << " is too short - raising to "
          << timeout;
    }

    using namespace std::chrono;
    auto end = steady_clock::now() +
               milliseconds(static_cast<uint64_t>(1000 * timeout));
    ClusterInfo& ci = feature.clusterInfo();

    auto const& nf = feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (!pool) {
      // nullptr happens only during controlled shutdown
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_SHUTTING_DOWN);
      return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
    }

    // Go to backup mode for *timeout* if and only if not already in
    // backup mode. Otherwise we cannot know, why backup mode was activated
    // We specifically want to make sure that no other backup is going on.
    bool supervisionOff = false;
    auto result = ci.agencyHotBackupLock(backupId, timeout, supervisionOff);

    if (!result.ok()) {
      // Failed to go to backup mode
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   StringUtils::concatT("agency lock operation resulted in ",
                                        result.errorMessage()));
      LOG_TOPIC("6c73d", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_HOT_BACKUP_INTERNAL);
      return result;
    }

    auto releaseAgencyLock = scopeGuard([&]() noexcept {
      try {
        LOG_TOPIC("52416", DEBUG, Logger::BACKUP)
            << "Releasing agency lock with scope guard! backupId: " << backupId;
        ci.agencyHotBackupUnlock(backupId, timeout, supervisionOff);
      } catch (std::exception const& e) {
        LOG_TOPIC("a163b", ERR, Logger::BACKUP)
            << "Failed to unlock hotbackup lock: " << e.what();
      }
    });

    if (end < steady_clock::now()) {
      LOG_TOPIC("352d6", INFO, Logger::BACKUP)
          << "hot backup didn't get to locking phase within " << timeout
          << "s.";
      // release the lock
      releaseAgencyLock.fire();

      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_CLUSTER_TIMEOUT);
      return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT,
                              "hot backup timeout before locking phase");
    }

    // acquire agency dump
    auto agency = std::make_shared<VPackBuilder>();
    result = ci.agencyPlan(agency);

    if (!result.ok()) {
      // release the lock
      releaseAgencyLock.fire();
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   StringUtils::concatT("failed to acquire agency dump: ",
                                        result.errorMessage()));
      LOG_TOPIC("c014d", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId,
                              TRI_ERROR_HOT_BACKUP_INTERNAL);
      return result;
    }

    // Call lock on all database servers

    std::vector<ServerID> dbServers = ci.getCurrentDBServers();
    std::vector<ServerID> serversToBeLocked = ci.getCurrentCoordinators();
    std::vector<ServerID> lockedServers;
    // We try to hold all write transactions on all servers at the same time.
    // The default timeout to get to this state is 120s. We first try for a
    // certain time t, and if not everybody has stopped all transactions within
    // t seconds, we release all locks and try again with t doubled, until the
    // total timeout has been reached. We start with t=15, which gives us
    // 15, 30 and 60 to try before the default timeout of 120s has been reached.
    double lockWait(15.0);
    while (steady_clock::now() < end && !feature.server().isStopping()) {
      result = lockServersTrxCommit(pool, backupId, serversToBeLocked, lockWait,
                                    lockedServers);
      if (!result.ok()) {
        unlockServersTrxCommit(pool, backupId, lockedServers);
        lockedServers.clear();
        if (result.is(TRI_ERROR_LOCAL_LOCK_FAILED)) {  // Unrecoverable
          LOG_TOPIC("99dbe", WARN, Logger::BACKUP)
              << "unable to lock servers for hot backup: "
              << result.errorMessage();
          // release the lock
          releaseAgencyLock.fire();
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  TRI_ERROR_LOCAL_LOCK_FAILED);
          return result;
        }
      } else {
        break;
      }
      if (lockWait < 3600.0) {
        lockWait *= 2.0;
      }
      std::this_thread::sleep_for(milliseconds(300));
    }

    // TODO: the force attribute is still present and offered by arangobackup,
    // but it can likely be removed nowadays.
    if (!result.ok() && force) {
      // About this code:
      // it first creates async requests to lock all coordinators.
      //    the corresponding lock ids are stored int the map lockJobIds.
      // Then we continously abort all trx while checking all the above jobs
      //    for completion.
      // If a job was completed then its id is removed from lockJobIds
      //  and the server is added to the lockedServers list.
      // Once lockJobIds is empty or an error occured we exit the loop
      //  and continue on the normal path (as if all servers would have been
      //  locked or error-exit)

      // dbserver -> jobId
      std::unordered_map<std::string, std::string> lockJobIds;

      auto releaseLocks = scopeGuard([&]() noexcept {
        try {
          hotbackupCancelAsyncLocks(pool, lockJobIds, lockedServers);
          unlockServersTrxCommit(pool, backupId, lockedServers);
        } catch (std::exception const& ex) {
          LOG_TOPIC("3449d", ERR, Logger::BACKUP)
              << "Failed to unlock hot backup: " << ex.what();
        }
      });

      // we have to reset the timeout, otherwise the code below will exit too
      // soon
      end = steady_clock::now() +
            milliseconds(static_cast<uint64_t>(1000 * timeout));

      // send the locks
      result = hotbackupAsyncLockCoordinatorsTransactions(
          pool, backupId, serversToBeLocked, lockWait, lockJobIds);
      if (result.fail()) {
        events::CreateHotbackup(timeStamp + "_" + backupId,
                                result.errorNumber());
        return result;
      }

      transaction::Manager* mgr = transaction::ManagerFeature::manager();

      while (!lockJobIds.empty()) {
        if (steady_clock::now() > end) {
          return arangodb::Result(TRI_ERROR_CLUSTER_TIMEOUT,
                                  "hot backup timeout before locking phase");
        }

        // kill all transactions
        result =
            mgr->abortAllManagedWriteTrx(ExecContext::current().user(), true);
        if (result.fail()) {
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  result.errorNumber());
          return result;
        }

        // wait for locks, servers that got the lock are removed from lockJobIds
        result = hotbackupWaitForLockCoordinatorsTransactions(
            pool, backupId, lockJobIds, lockedServers, lockWait);
        if (result.fail()) {
          LOG_TOPIC("b6496", WARN, Logger::BACKUP)
              << "Waiting for hot backup server locks failed: "
              << result.errorMessage();
          events::CreateHotbackup(timeStamp + "_" + backupId,
                                  result.errorNumber());
          return result;
        }

        std::this_thread::sleep_for(milliseconds(300));
      }

      releaseLocks.cancel();
    }

    bool gotLocks = result.ok();

    // In the case we left the above loop with a negative result,
    // and we are in the case of a force backup we want to continue here
    if (!gotLocks && !allowInconsistent) {
      unlockServersTrxCommit(pool, backupId, serversToBeLocked);
      // release the lock
      releaseAgencyLock.fire();
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT(
              "failed to acquire global transaction lock on all coordinators: ",
              result.errorMessage()));
      LOG_TOPIC("b7d09", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    BackupMeta meta(backupId, "", timeStamp, std::vector<std::string>{}, 0, 0,
                    static_cast<unsigned int>(serversToBeLocked.size()), "",
                    !gotLocks);  // Temporary
    std::vector<std::string> dummy;
    result = hotBackupDBServers(pool, backupId, timeStamp, dbServers,
                                agency->slice(),
                                /* force */ !gotLocks, meta);
    if (!result.ok()) {
      unlockServersTrxCommit(pool, backupId, serversToBeLocked);
      // release the lock
      releaseAgencyLock.fire();
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT("failed to hot backup on all coordinators: ",
                               result.errorMessage()));
      LOG_TOPIC("6b333", ERR, Logger::BACKUP) << result.errorMessage();
      removeLocalBackups(pool, backupId, dbServers, dummy);
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    unlockServersTrxCommit(pool, backupId, serversToBeLocked);
    // release the lock
    releaseAgencyLock.fire();

    auto agencyCheck = std::make_shared<VPackBuilder>();
    result = ci.agencyPlan(agencyCheck);
    if (!result.ok()) {
      if (!allowInconsistent) {
        removeLocalBackups(pool, backupId, dbServers, dummy);
      }
      result.reset(
          TRI_ERROR_HOT_BACKUP_INTERNAL,
          StringUtils::concatT("failed to acquire agency dump post backup: ",
                               result.errorMessage(),
                               " backup's integrity is not guaranteed"));
      LOG_TOPIC("d4229", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    try {
      if (!Helper::equal(agency->slice()[0].get(versionPath),
                         agencyCheck->slice()[0].get(versionPath), false)) {
        if (!allowInconsistent) {
          removeLocalBackups(pool, backupId, dbServers, dummy);
        }
        result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                     "data definition of cluster was changed during hot "
                     "backup: backup's integrity is not guaranteed");
        LOG_TOPIC("0ad21", ERR, Logger::BACKUP) << result.errorMessage();
        events::CreateHotbackup(timeStamp + "_" + backupId,
                                result.errorNumber());
        return result;
      }
    } catch (std::exception const& e) {
      removeLocalBackups(pool, backupId, dbServers, dummy);
      result.reset(TRI_ERROR_HOT_BACKUP_INTERNAL,
                   std::string("invalid agency state: ") + e.what());
      LOG_TOPIC("037eb", ERR, Logger::BACKUP) << result.errorMessage();
      events::CreateHotbackup(timeStamp + "_" + backupId, result.errorNumber());
      return result;
    }

    std::replace(timeStamp.begin(), timeStamp.end(), ':', '.');
    {
      VPackObjectBuilder o(&report);
      report.add("id", VPackValue(timeStamp + "_" + backupId));
      report.add("sizeInBytes", VPackValue(meta._sizeInBytes));
      report.add("nrFiles", VPackValue(meta._nrFiles));
      report.add("nrDBServers", VPackValue(meta._nrDBServers));
      report.add("datetime", VPackValue(meta._datetime));
      if (!gotLocks) {
        report.add("potentiallyInconsistent", VPackValue(true));
      }
    }

    events::CreateHotbackup(timeStamp + "_" + backupId, TRI_ERROR_NO_ERROR);
    return arangodb::Result();

  } catch (std::exception const& e) {
    events::CreateHotbackup("", TRI_ERROR_HOT_BACKUP_INTERNAL);
    return arangodb::Result(
        TRI_ERROR_HOT_BACKUP_INTERNAL,
        std::string("caught exception creating cluster backup: ") + e.what());
  }
}

arangodb::Result listHotBackupsOnCoordinator(ClusterFeature& feature,
                                             VPackSlice const payload,
                                             VPackBuilder& report) {
  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {
    // shutdown, leave here
    return TRI_ERROR_SHUTTING_DOWN;
  }

  ClusterInfo& ci = feature.clusterInfo();
  std::vector<ServerID> dbServers = ci.getCurrentDBServers();

  std::unordered_map<std::string, BackupMeta> list;

  VPackSlice idSlice;
  if (payload.isObject() && payload.hasKey("id")) {
    idSlice = payload.get("id");
    if (idSlice.isArray()) {
      for (auto const i : VPackArrayIterator(payload.get("id"))) {
        if (!i.isString()) {
          return arangodb::Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                                  "invalid list JSON: all ids must be string.");
        }
      }
    } else if (!idSlice.isString()) {
      return arangodb::Result(
          TRI_ERROR_HTTP_BAD_PARAMETER,
          "invalid JSON: id must be string or array of strings.");
    }
  } else if (!payload.isNone()) {
    return arangodb::Result(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "invalid JSON: body must be empty or object with attribute 'id'.");
  }  // allow continuation with None slice

  VPackBuilder dummy;

  // Try to get complete listing for 2 minutes
  using namespace std::chrono;
  auto timeout = steady_clock::now() + duration<double>(120.0);
  arangodb::Result result;
  std::chrono::duration<double> wait(1.0);
  while (true) {
    if (feature.server().isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN, "server is shutting down");
    }

    result = hotBackupList(pool, dbServers, idSlice, list, dummy);

    if (!result.ok()) {
      if (payload.isObject() && !idSlice.isNone() &&
          result.is(TRI_ERROR_HTTP_NOT_FOUND)) {
        auto error =
            std::string("failed to locate backup '") + idSlice.toJson() + "'";
        LOG_TOPIC("2020b", DEBUG, Logger::BACKUP) << error;
        return arangodb::Result(TRI_ERROR_HTTP_NOT_FOUND, error);
      }
      if (steady_clock::now() > timeout) {
        return arangodb::Result(
            TRI_ERROR_CLUSTER_TIMEOUT,
            "timeout waiting for all db servers to report backup list");
      } else {
        LOG_TOPIC("76865", DEBUG, Logger::BACKUP)
            << "failed to get a hot backup listing from all db servers waiting "
            << wait.count() << " seconds";
        std::this_thread::sleep_for(wait);
        wait *= 1.1;
      }
    } else {
      break;
    }
  }

  // Build report
  {
    VPackObjectBuilder o(&report);
    report.add(VPackValue("list"));
    {
      VPackObjectBuilder a(&report);
      for (auto const& i : list) {
        report.add(VPackValue(i.first));
        i.second.toVelocyPack(report);
      }
    }
  }

  return arangodb::Result();
}

arangodb::Result deleteHotBackupsOnCoordinator(ClusterFeature& feature,
                                               VPackSlice const payload,
                                               VPackBuilder& report) {
  std::vector<std::string> deleted;
  VPackBuilder dummy;
  arangodb::Result result;

  auto const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (!pool) {  // shutdown, leave here
    events::DeleteHotbackup("", TRI_ERROR_SHUTTING_DOWN);
    return TRI_ERROR_SHUTTING_DOWN;
  }

  ClusterInfo& ci = feature.clusterInfo();
  std::vector<ServerID> dbServers = ci.getCurrentDBServers();

  if (!payload.isObject() || !payload.hasKey("id") ||
      !payload.get("id").isString()) {
    events::DeleteHotbackup("", TRI_ERROR_HTTP_BAD_PARAMETER);
    return arangodb::Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                            "Expecting object with key `id` set to backup id.");
  }

  std::string id = payload.get("id").copyString();

  result = removeLocalBackups(pool, id, dbServers, deleted);
  if (!result.ok()) {
    events::DeleteHotbackup(id, result.errorNumber());
    return result;
  }

  {
    VPackObjectBuilder o(&report);
    report.add(VPackValue("id"));
    {
      VPackArrayBuilder a(&report);
      for (auto const& i : deleted) {
        report.add(VPackValue(i));
      }
    }
  }

  result.reset();
  events::DeleteHotbackup(id, TRI_ERROR_NO_ERROR);
  return result;
}

}  // namespace arangodb
