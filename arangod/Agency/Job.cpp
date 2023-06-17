////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <unordered_map>
#include <utility>

#include "Job.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AgentInterface.h"
#include "Agency/Node.h"
#include "Agency/Supervision.h"
#include "AgencyStrings.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"
#include "Helpers.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
using namespace arangodb::velocypack;

namespace {
std::vector<std::string> const jobStatus{"ToDo", "Pending", "Finished",
                                         "Failed"};
}

namespace arangodb::consensus {

std::string const mapUniqueToShortID = "/Target/MapUniqueToShortID/";
std::string const pendingPrefix = "/Target/Pending/";
std::string const failedPrefix = "/Target/Failed/";
std::string const finishedPrefix = "/Target/Finished/";
std::string const toDoPrefix = "/Target/ToDo/";
std::string const cleanedPrefix = "/Target/CleanedServers";
std::string const toBeCleanedPrefix = "/Target/ToBeCleanedServers";
std::string const failedServersPrefix = "/Target/FailedServers";
std::string const planColPrefix = "/Plan/Collections/";
std::string const targetRepStatePrefix = "/Target/ReplicatedLogs/";
std::string const planRepStatePrefix = "/Plan/ReplicatedLogs/";
std::string const planDBPrefix = "/Plan/Databases/";
std::string const curServersKnown = "/Current/ServersKnown/";
std::string const curColPrefix = "/Current/Collections/";
std::string const blockedServersPrefix = "/Supervision/DBServers/";
std::string const blockedShardsPrefix = "/Supervision/Shards/";
std::string const planVersion = "/Plan/Version";
std::string const currentVersion = "/Current/Version";
std::string const plannedServers = "/Plan/DBServers";
std::string const healthPrefix = "/Supervision/Health/";
std::string const maintenancePrefix = "/Current/MaintenanceDBServers/";
std::string const asyncReplLeader = "/Plan/AsyncReplication/Leader";
std::string const asyncReplTransientPrefix = "/AsyncReplication/";
std::string const planAnalyzersPrefix = "/Plan/Analyzers/";
std::string const returnLeadershipPrefix = "/Target/ReturnLeadership/";

write_ret_t singleWriteTransaction(AgentInterface* _agent,
                                   velocypack::Builder const& transaction,
                                   bool waitForCommit) {
  velocypack::Builder envelope;

  velocypack::Slice trx = transaction.slice();
  try {
    {
      VPackArrayBuilder listOfTrxs(&envelope);
      VPackArrayBuilder onePair(&envelope);
      {
        VPackObjectBuilder mutationPart(&envelope);
        for (auto pair : VPackObjectIterator(trx[0])) {
          envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                       pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(&envelope);
        for (auto pair : VPackObjectIterator(trx[1])) {
          envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                       pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("5be90", ERR, Logger::SUPERVISION)
        << "Supervision failed to build single-write transaction: " << e.what();
  }

  auto ret = _agent->write(envelope.slice());
  if (waitForCommit && !ret.indices.empty()) {
    auto maximum = *std::max_element(ret.indices.begin(), ret.indices.end());
    if (maximum > 0) {  // some baby has worked
      _agent->waitFor(maximum);
    }
  }
  return ret;
}

trans_ret_t generalTransaction(AgentInterface* _agent,
                               velocypack::Builder const& transaction) {
  velocypack::Builder envelope;
  velocypack::Slice trx = transaction.slice();

  try {
    {
      VPackArrayBuilder listOfTrxs(&envelope);
      for (auto singleTrans : VPackArrayIterator(trx)) {
        TRI_ASSERT(singleTrans.isArray() && singleTrans.length() > 0);
        if (singleTrans[0].isObject()) {
          VPackArrayBuilder onePair(&envelope);
          {
            VPackObjectBuilder mutationPart(&envelope);
            for (auto pair : VPackObjectIterator(singleTrans[0])) {
              envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                           pair.value);
            }
          }
          if (singleTrans.length() > 1) {
            VPackObjectBuilder preconditionPart(&envelope);
            for (auto pair : VPackObjectIterator(singleTrans[1])) {
              envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                           pair.value);
            }
          }
        } else if (singleTrans[0].isString()) {
          VPackArrayBuilder reads(&envelope);
          for (auto path : VPackArrayIterator(singleTrans)) {
            envelope.add(
                VPackValue("/" + Job::agencyPrefix + path.copyString()));
          }
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("aae99", ERR, Logger::SUPERVISION)
        << "Supervision failed to build transaction: " << e.what();
  }

  auto ret = _agent->transact(envelope.slice());

  // This is for now disabled to speed up things. We wait after a full
  // Supervision run, which is good enough.
  // if (ret.maxind > 0) {
  //   _agent->waitFor(ret.maxind);
  // }

  return ret;
}

trans_ret_t transient(AgentInterface* _agent,
                      velocypack::Builder const& transaction) {
  velocypack::Builder envelope;

  velocypack::Slice trx = transaction.slice();
  try {
    {
      VPackArrayBuilder listOfTrxs(&envelope);
      VPackArrayBuilder onePair(&envelope);
      {
        VPackObjectBuilder mutationPart(&envelope);
        for (auto pair : VPackObjectIterator(trx[0])) {
          envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                       pair.value);
        }
      }
      if (trx.length() > 1) {
        VPackObjectBuilder preconditionPart(&envelope);
        for (auto pair : VPackObjectIterator(trx[1])) {
          envelope.add("/" + Job::agencyPrefix + pair.key.copyString(),
                       pair.value);
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("d03d5", ERR, Logger::SUPERVISION)
        << "Supervision failed to build transaction for transient: "
        << e.what();
  }

  return _agent->transient(envelope.slice());
}

}  // namespace arangodb::consensus

using namespace arangodb::basics;
using namespace arangodb::consensus;

Job::Job(JOB_STATUS status, Node const& snapshot, AgentInterface* agent,
         std::string const& jobId, std::string const& creator)
    : _status(status),
      _snapshot(snapshot),
      _agent(agent),
      _jobId(jobId),
      _creator(creator),
      _jb(nullptr) {}

Job::~Job() = default;

// this will be initialized in the AgencyFeature
std::string Job::agencyPrefix = "arango";

bool Job::considerCancellation() {
  // Allow for cancellation of shard moves
  auto val = _snapshot.hasAsBool(
      std::string("/Target/") + ::jobStatus[_status] + "/" + _jobId + "/abort");
  auto cancelled = val && val.value();
  if (cancelled) {
    abort("Killed via API");
  }
  return cancelled;
}

void Job::runHelper(std::string const& server, std::string const& shard,
                    bool& aborts) {
  if (_status == FAILED) {  // happens when the constructor did not work
    return;
  }
  try {
    status();  // This runs everything to to with state PENDING if needed!
  } catch (std::exception const& e) {
    LOG_TOPIC("e2d06", WARN, Logger::AGENCY)
        << "Exception caught in status() method: " << e.what();
    finish(server, shard, false, e.what());
  }
  try {
    if (_status == TODO) {
      start(aborts);
    } else if (_status == NOTFOUND) {
      if (create(nullptr)) {
        start(aborts);
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("5ac04", WARN, Logger::AGENCY)
        << "Exception caught in create() or "
           "start() method: "
        << e.what();
    finish("", "", false, e.what());
  }
}

bool Job::finish(std::string const& server, std::string const& shard,
                 bool success, std::string const& reason,
                 query_t const& payload) {
  try {  // protect everything, just in case

    // Get todo entry
    bool started = false;
    VPackBuilder pending;
    {
      VPackArrayBuilder guard(&pending);
      if (_snapshot.exists(pendingPrefix + _jobId).size() == 3) {
        std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, pending);
        started = true;
      } else if (_snapshot.exists(toDoPrefix + _jobId).size() == 3) {
        std::ignore = _snapshot.hasAsBuilder(toDoPrefix + _jobId, pending);
      } else {
        LOG_TOPIC("54fde", DEBUG, Logger::AGENCY)
            << "Nothing in pending to finish up for job " << _jobId;
        return false;
      }
    }

    std::string jobType;
    try {
      jobType = pending.slice()[0].get("type").copyString();
    } catch (std::exception const&) {
      LOG_TOPIC("76352", WARN, Logger::AGENCY)
          << "Failed to obtain type of job " << _jobId;
    }

    // Additional payload, which is to be executed in the finish transaction
    Slice operations = Slice::emptyObjectSlice();
    Slice preconditions = Slice::emptyObjectSlice();

    if (payload != nullptr) {
      Slice slice = payload->slice();
      TRI_ASSERT(slice.isObject() || slice.isArray());
      if (slice.isObject()) {  // opers only
        operations = slice;
        TRI_ASSERT(operations.isObject());
      } else {
        TRI_ASSERT(slice.length() < 3);  // opers + precs only
        if (slice.length() > 0) {
          operations = slice[0];
          TRI_ASSERT(operations.isObject());
          if (slice.length() > 1) {
            preconditions = slice[1];
            TRI_ASSERT(preconditions.isObject());
          }
        }
      }
    }

    // Prepare pending entry, block toserver
    VPackBuilder finished;
    {
      VPackArrayBuilder guard(&finished);

      {  // operations --
        VPackObjectBuilder operguard(&finished);

        addPutJobIntoSomewhere(finished, success ? "Finished" : "Failed",
                               pending.slice()[0], reason);

        addRemoveJobFromSomewhere(finished, "ToDo", _jobId);
        addRemoveJobFromSomewhere(finished, "Pending", _jobId);

        if (operations.length() > 0) {
          for (auto oper : VPackObjectIterator(operations)) {
            finished.add(oper.key.stringView(), oper.value);
          }
        }

        // --- Remove blocks if specified:
        if (started && !server.empty()) {
          addReleaseServer(finished, server);
        }
        if (started && !shard.empty()) {
          addReleaseShard(finished, shard);
        }

      }  // -- operations

      if (preconditions.isObject() &&
          preconditions.length() > 0) {  // preconditions --
        VPackObjectBuilder precguard(&finished);
        for (auto prec : VPackObjectIterator(preconditions)) {
          finished.add(prec.key.stringView(), prec.value);
        }
      }  // -- preconditions
    }

    write_ret_t res = singleWriteTransaction(_agent, finished, false);
    if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
      LOG_TOPIC("21362", DEBUG, Logger::AGENCY)
          << "Successfully finished job " << jobType << "(" << _jobId << ")";
      _status = (success ? FINISHED : FAILED);
      return true;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("1fead", WARN, Logger::AGENCY)
        << "Caught exception in finish, message: " << e.what();
  } catch (...) {
    LOG_TOPIC("7762f", WARN, Logger::AGENCY)
        << "Caught unspecified exception in finish.";
  }
  return false;
}

std::string Job::randomIdleAvailableServer(
    Node const& snap, std::vector<std::string> const& exclude) {
  std::vector<std::string> as = availableServers(snap);
  std::string ret;

  // Prefer good servers over bad servers
  std::vector<std::string> good;

  // Only take good servers as valid server.
  try {
    for (auto const& srv : *snap.hasAsChildren(healthPrefix)) {
      // ignore excluded servers
      if (std::find(std::begin(exclude), std::end(exclude), srv.first) !=
          std::end(exclude)) {
        continue;
      }
      if (snap.has(maintenancePrefix + srv.first)) {
        continue;  // ignore servers in maintenance mode
      }

      // ignore servers not in availableServers above:
      if (std::find(std::begin(as), std::end(as), srv.first) == std::end(as)) {
        continue;
      }

      std::string const status = (*srv.second).hasAsString("Status").value();
      if (status == Supervision::HEALTH_STATUS_GOOD) {
        good.push_back(srv.first);
      }
    }
  } catch (...) {
  }

  if (good.empty()) {
    return ret;
  }

  // Choose random server from rest
  if (good.size() == 1) {
    ret = good[0];
    return ret;
  }
  uint16_t interval = static_cast<uint16_t>(good.size() - 1);
  uint16_t random = RandomGenerator::interval(interval);
  ret = good.at(random);
  return ret;
}

// The following counts in a given server list how many of the servers are
// in Status "GOOD" or "BAD".
size_t Job::countGoodOrBadServersInList(Node const& snap,
                                        VPackSlice serverList) {
  size_t count = 0;
  if (!serverList.isArray()) {
    // No array, strange, return 0
    return count;
  }
  auto const& health = snap.hasAsChildren(healthPrefix);
  auto const& maintenanceSet = snap.hasAsChildren(maintenancePrefix);
  // Do we have a Health substructure?
  if (health) {
    Node::Children const& healthData = *health;  // List of servers in Health
    for (VPackSlice serverName : VPackArrayIterator(serverList)) {
      if (serverName.isString()) {
        // serverName not a string? Then don't count
        std::string serverStr = serverName.copyString();
        // Ignore a potential _ prefix, which can occur on leader resign:
        if (serverStr.starts_with('_')) {
          serverStr.erase(0, 1);  // remove leading _
        }
        // Now look up this server:
        auto it = healthData.find(serverStr);
        if (it != healthData.end()) {
          // Only check if found
          std::shared_ptr<Node> healthNode = it->second;
          // Check its status:

          if (auto status = healthNode->hasAsString("Status");
              status && (status.value() == Supervision::HEALTH_STATUS_GOOD ||
                         status.value() == Supervision::HEALTH_STATUS_BAD)) {
            ++count;
            // check is server is maintenance mode, if so, consider as good
          } else if (maintenanceSet && maintenanceSet->contains(serverStr)) {
            ++count;
          }
        }
      }
    }
  }
  return count;
}

// The following counts in a given server list how many of the servers are
// in Status "GOOD" or "BAD".
size_t Job::countGoodOrBadServersInList(
    Node const& snap, std::vector<std::string> const& serverList) {
  size_t count = 0;
  auto const& health = snap.hasAsChildren(healthPrefix);
  // Do we have a Health substructure?
  if (health) {
    Node::Children const& healthData = *health;  // List of servers in Health
    for (auto& serverStr : serverList) {
      // Now look up this server:
      auto it = healthData.find(serverStr);
      if (it != healthData.end()) {
        // Only check if found
        std::shared_ptr<Node> healthNode = it->second;
        // Check its status:
        if (auto status = healthNode->hasAsString("Status");
            status && (status.value() == Supervision::HEALTH_STATUS_GOOD ||
                       status.value() == Supervision::HEALTH_STATUS_BAD)) {
          ++count;
        }
      }
    }
  }
  return count;
}

/// @brief Check if a server is cleaned or to be cleaned out:
bool Job::isInServerList(Node const& snap, std::string const& prefix,
                         std::string const& server, bool isArray) {
  bool found = false;
  if (isArray) {
    auto slice = snap.hasAsSlice(prefix);
    if (slice && slice->isArray()) {
      for (VPackSlice srv : VPackArrayIterator(*slice)) {
        if (srv.isEqualString(server)) {
          found = true;
          break;
        }
      }
    }
  } else {  // an object
    auto const& children = snap.hasAsChildren(prefix);
    if (children) {
      for (auto const& srv : *children) {
        if (srv.first == server) {
          found = true;
          break;
        }
      }
    }
  }
  return found;
}

/// @brief Get servers from plan, which are not failed or (to be) cleaned out
std::vector<std::string> Job::availableServers(Node const& snapshot) {
  std::vector<std::string> ret;

  // Get servers from plan
  Node::Children const& dbservers = *snapshot.hasAsChildren(plannedServers);
  for (auto const& srv : dbservers) {
    ret.push_back(srv.first);
  }

  auto excludePrefix = [&ret, &snapshot](std::string const& prefix,
                                         bool isArray) {
    if (isArray) {
      auto slice = snapshot.hasAsSlice(prefix);
      if (slice) {
        for (VPackSlice srv : VPackArrayIterator(*slice)) {
          ret.erase(std::remove(ret.begin(), ret.end(), srv.copyString()),
                    ret.end());
        }
      }
    } else {
      auto const& children = snapshot.hasAsChildren(prefix);
      if (children) {
        for (auto const& srv : *children) {
          ret.erase(std::remove(ret.begin(), ret.end(), srv.first), ret.end());
        }
      }
    }
  };

  // Remove (to be) cleaned and failed servers from the list
  excludePrefix(cleanedPrefix, true);
  excludePrefix(failedServersPrefix, false);
  excludePrefix(toBeCleanedPrefix, true);

  return ret;
}

/// @brief Get servers from Supervision with health status GOOD
std::vector<std::string> Job::healthyServers(
    arangodb::consensus::Node const& snapshot) {
  std::vector<std::string> ret;
  for (auto const& srv : snapshot.get(healthPrefix)->children()) {
    auto healthState = srv.second->hasAsString("Status");
    if (healthState && healthState.value() == Supervision::HEALTH_STATUS_GOOD) {
      ret.emplace_back(srv.first);
    }
  }
  return ret;
}

template<typename T>
std::vector<size_t> idxsort(const std::vector<T>& v) {
  std::vector<size_t> idx(v.size());

  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(),
            [&v](size_t i, size_t j) { return v[i] < v[j]; });

  return idx;
}

std::vector<std::string> sortedShardList(Node const& shards) {
  std::vector<size_t> sids;
  auto const& shardMap = shards.children();
  for (auto const& shard : shardMap) {
    sids.push_back(StringUtils::uint64(shard.first.substr(1)));
  }

  std::vector<size_t> idx(idxsort(sids));
  std::vector<std::string> sorted;
  for (auto const& i : idx) {
    auto x = shardMap.begin();
    std::advance(x, i);
    sorted.push_back(x->first);
  }

  return sorted;
}

std::vector<Job::shard_t> Job::clones(Node const& snapshot,
                                      std::string const& database,
                                      std::string const& collection,
                                      std::string const& shard) {
  std::vector<shard_t> ret;
  ret.emplace_back(collection, shard);  // add (collection, shard) as first item
  // typedef std::unordered_map<std::string, std::shared_ptr<Node>> UChildren;

  std::string databasePath = planColPrefix + database,
              planPath = databasePath + "/" + collection + "/shards";

  auto myshards = sortedShardList(*snapshot.hasAsNode(planPath));
  auto steps = std::distance(
      myshards.begin(), std::find(myshards.begin(), myshards.end(), shard));

  for (auto const& colptr :
       *snapshot.hasAsChildren(databasePath)) {  // collections

    auto const& col = *colptr.second;
    auto const& otherCollection = colptr.first;

    if (otherCollection != collection &&
        col.has("distributeShardsLike") &&  // use .has() form to prevent
                                            // logging of missing
        col.hasAsSlice("distributeShardsLike").value().stringView() ==
            collection) {
      auto const& theirshards = sortedShardList(*col.hasAsNode("shards"));
      if (theirshards.size() > 0) {  // do not care about virtual collections
        if (theirshards.size() == myshards.size()) {
          ret.emplace_back(otherCollection, theirshards[steps]);
        } else {
          LOG_TOPIC("3092e", ERR, Logger::SUPERVISION)
              << "Shard distribution of clone(" << otherCollection
              << ") does not match ours (" << collection << ")";
        }
      }
    }
  }

  return ret;
}

std::string
Job::findNonblockedCommonHealthyInSyncFollower(  // Which is in "GOOD" health
    Node const& snap, std::string const& db, std::string const& col,
    std::string const& shrd, std::string const& serverToAvoid) {
  // serverToAvoid is the leader for which we are seeking a replacement. Note
  // that it is not a given that this server is the first one in Current/servers
  // or Current/failoverCandidates. Futhermore servers in
  // /Current/MaintenanceDBServers/ are not considered as well.
  auto cs = clones(snap, db, col, shrd);  // clones
  auto nclones = cs.size();               // #clones
  std::unordered_map<std::string, bool> good;

  for (auto const& i : *snap.hasAsChildren(healthPrefix)) {
    good[i.first] = ((*i.second).hasAsString("Status").value() ==
                     Supervision::HEALTH_STATUS_GOOD);
  }

  std::unordered_map<std::string, size_t> currentServers;
  for (auto const& clone : cs) {
    auto sharedPath = db + "/" + clone.collection + "/";
    auto currentShardPath =
        curColPrefix + sharedPath + clone.shard + "/servers";
    auto currentFailoverCandidatesPath =
        curColPrefix + sharedPath + clone.shard + "/failoverCandidates";
    auto plannedShardPath =
        planColPrefix + sharedPath + "shards/" + clone.shard;

    // start up race condition  ... current might not have everything in plan
    if (!snap.has(currentShardPath) || !snap.has(plannedShardPath)) {
      --nclones;
      continue;
    }  // if

    // If we do have failover candidates, we should use them
    auto serverList = snap.hasAsArray(currentFailoverCandidatesPath);
    if (!serverList) {
      // We have old DBServers that do not report failover candidates,
      // Need to rely on current
      serverList = snap.hasAsArray(currentShardPath);
      TRI_ASSERT(serverList);
      if (!serverList) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
            "Could not find common insync server for: " + currentShardPath +
                ", value is not an array.");
      }
    }
    // Guaranteed by if above
    TRI_ASSERT(serverList);

    for (auto server : *serverList) {
      auto id = server.copyString();
      if (id == serverToAvoid) {
        // Skip current leader for which we are seeking a replacement
        continue;
      }

      if (!good[id]) {
        // Skip unhealthy servers
        continue;
      }

      if (snap.has(blockedServersPrefix + id)) {
        // server is blocked
        continue;
      }

      if (snap.has(maintenancePrefix + id)) {
        // server is in maintenance mode
        continue;
      }

      // check if it is also part of the plan...because if not the soon-to-be
      // leader will drop the collection
      bool found = false;
      for (auto const& plannedServer : *snap.hasAsArray(plannedShardPath)) {
        if (plannedServer.isEqualString(server.stringView())) {
          found = true;
          break;
        }
      }

      if (!found) {
        continue;
      }

      // increment a counter for this server
      // and in case it is applicable for ALL our distributeLikeShards
      // finally return it :)
      if (++currentServers[id] == nclones) {
        return id;
      }
    }
  }

  return std::string();
}

/// @brief The shard must be one of a collection without
/// `distributeShardsLike`. This returns all servers which
/// are in sync for this shard and for all of its clones.
std::vector<std::string> Job::findAllInSyncReplicas(
    Node const& snap, std::string const& db,
    std::vector<Job::shard_t> const& shardsLikeMe) {
  std::vector<std::string> result;

  bool first = true;
  for (auto const& clone : shardsLikeMe) {
    auto sharedPath = db + "/" + clone.collection + "/";
    auto currentPath = curColPrefix + sharedPath + clone.shard + "/servers";
    // If we do have failover candidates, we should use them
    auto serverList = snap.hasAsArray(currentPath);
    if (serverList) {
      std::unordered_set<std::string> setHere;
      for (auto server : *serverList) {
        auto id = server.copyString();
        if (first) {
          result.push_back(id);
        } else {
          setHere.insert(id);
        }
      }
      if (!first) {
        // result = result intersect setHere:
        for (auto it = result.begin(); it != result.end();) {
          if (setHere.find(*it) == setHere.end()) {
            it = result.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
    first = false;
  }

  return result;
}

/// @brief The shard must be one of a collection without
/// `distributeShardsLike`. This returns all servers which
/// are in `failoverCandidates` for this shard or for any of its clones.
std::unordered_set<std::string> Job::findAllFailoverCandidates(
    Node const& snap, std::string const& db,
    std::vector<Job::shard_t> const& shardsLikeMe) {
  std::unordered_set<std::string> result;

  for (auto const& clone : shardsLikeMe) {
    auto sharedPath = db + "/" + clone.collection + "/";
    auto currentFailoverCandidatesPath =
        curColPrefix + sharedPath + clone.shard + "/failoverCandidates";
    // If we do have failover candidates, we should use them
    auto serverList = snap.hasAsArray(currentFailoverCandidatesPath);
    if (serverList) {
      for (auto server : *serverList) {
        result.insert(server.copyString());
      }
    }
  }

  return result;
}

std::string Job::uuidLookup(std::string const& shortID) {
  for (auto const& uuid : *_snapshot.hasAsChildren(mapUniqueToShortID)) {
    if ((*uuid.second).hasAsString("ShortName").value() == shortID) {
      return uuid.first;
    }
  }
  return std::string();
}

std::string Job::id(std::string const& idOrShortName) {
  std::string id = uuidLookup(idOrShortName);
  if (!id.empty()) {
    return id;
  }
  return idOrShortName;
}

bool Job::abortable(Node const& snapshot, std::string const& jobId) {
  if (!snapshot.has(pendingPrefix + jobId)) {
    return false;
  }
  auto const& job = snapshot.hasAsNode(pendingPrefix + jobId);
  if (!job || !job->has("type")) {
    return false;
  }
  auto const& tmp_type = job->hasAsString("type");
  if (!tmp_type) {
    return false;
  }
  std::string const& type = tmp_type.value();
  if (type == "failedServer" || type == "failedLeader" ||
      type == "activeFailover") {
    return false;
  } else if (type == "addFollower" || type == "moveShard" ||
             type == "cleanOutServer" || type == "resignLeadership") {
    return true;
  }

  // We should never get here
  TRI_ASSERT(false);
  return false;
}

void Job::doForAllShards(
    Node const& snapshot, std::string& database, std::vector<shard_t>& shards,
    std::function<void(Slice plan, Slice current, std::string& planPath,
                       std::string& curPath)>
        worker) {
  for (auto const& collShard : shards) {
    std::string planPath = planColPrefix + database + "/" +
                           collShard.collection + "/shards/" + collShard.shard;
    std::string curPath = curColPrefix + database + "/" + collShard.collection +
                          "/" + collShard.shard + "/servers";

    Slice plan = snapshot.hasAsSlice(planPath).value();
    Slice current = snapshot.hasAsSlice(curPath).value_or(Slice::noneSlice());

    worker(plan, current, planPath, curPath);
  }
}

void Job::addIncreasePlanVersion(Builder& trx) {
  trx.add(VPackValue(planVersion));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("increment"));
  }
}

void Job::addIncreaseCurrentVersion(Builder& trx) {
  trx.add(VPackValue(currentVersion));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("increment"));
  }
}

void Job::addIncreaseRebootId(Builder& trx, std::string const& server) {
  trx.add(VPackValue(curServersKnown + server + "/rebootId"));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("increment"));
  }
  addIncreaseCurrentVersion(trx);
}

void Job::addRemoveJobFromSomewhere(Builder& trx, std::string const& where,
                                    std::string const& jobId) {
  trx.add(VPackValue("/Target/" + where + "/" + jobId));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

void Job::addPutJobIntoSomewhere(Builder& trx, std::string const& where,
                                 Slice job, std::string const& reason) {
  Slice jobIdSlice = job.get("jobId");
  TRI_ASSERT(jobIdSlice.isString());
  std::string jobId = jobIdSlice.copyString();
  trx.add(VPackValue("/Target/" + where + "/" + jobId));
  {
    VPackObjectBuilder guard(&trx);
    if (where == "Pending") {
      trx.add("timeStarted",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
    } else {
      trx.add("timeFinished",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
    }
    for (auto obj : VPackObjectIterator(job)) {
      trx.add(obj.key.stringView(), obj.value);
    }
    if (!reason.empty()) {
      trx.add("reason", VPackValue(reason));
    }
  }
}

void Job::addPreconditionCurrentReplicaShardGroup(
    Builder& pre, std::string const& database,
    std::vector<shard_t> const& shardGroup, std::string const& serverId) {
  for (auto const& sh : shardGroup) {
    std::string const curPath = curColPrefix + database + "/" + sh.collection +
                                "/" + sh.shard + "/servers";
    pre.add(VPackValue(curPath));
    {
      VPackObjectBuilder guard(&pre);
      pre.add("in", VPackValue(serverId));
    }
  }
}

void Job::addPreconditionCollectionStillThere(Builder& pre,
                                              std::string const& database,
                                              std::string const& collection) {
  std::string planPath = planColPrefix + database + "/" + collection;
  pre.add(VPackValue(planPath));
  {
    VPackObjectBuilder guard(&pre);
    pre.add("oldEmpty", VPackValue(false));
  }
}

void Job::addPreconditionServerNotBlocked(Builder& pre,
                                          std::string const& server) {
  pre.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder serverLockEmpty(&pre);
    pre.add("oldEmpty", VPackValue(true));
  }
}

void Job::addPreconditionServerHealth(Builder& pre, std::string const& server,
                                      std::string_view health) {
  pre.add(VPackValue(healthPrefix + server + "/Status"));
  {
    VPackObjectBuilder serverGood(&pre);
    pre.add("old", VPackValue(health));
  }
}

void Job::addPreconditionShardNotBlocked(Builder& pre,
                                         std::string const& shard) {
  pre.add(VPackValue(blockedShardsPrefix + shard));
  {
    VPackObjectBuilder shardLockEmpty(&pre);
    pre.add("oldEmpty", VPackValue(true));
  }
}

void Job::addPreconditionUnchanged(Builder& pre, std::string_view key,
                                   Slice value) {
  pre.add(VPackValue(key));
  {
    VPackObjectBuilder guard(&pre);
    pre.add("old", value);
  }
}

void Job::addBlockServer(Builder& trx, std::string const& server,
                         std::string_view jobId) {
  trx.add(blockedServersPrefix + server, VPackValue(jobId));
}

void Job::addBlockShard(Builder& trx, std::string const& shard,
                        std::string_view jobId) {
  trx.add(blockedShardsPrefix + shard, VPackValue(jobId));
}

void Job::addReleaseServer(Builder& trx, std::string const& server) {
  trx.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

void Job::addReleaseShard(Builder& trx, std::string const& shard) {
  trx.add(VPackValue(blockedShardsPrefix + shard));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

void Job::addPreconditionJobStillInPending(Builder& pre,
                                           std::string const& jobId) {
  pre.add(VPackValue("/Target/Pending/" + jobId));
  {
    VPackObjectBuilder guard(&pre);
    pre.add("oldEmpty", VPackValue(false));
  }
}

std::string Job::checkServerHealth(Node const& snapshot,
                                   std::string const& server) {
  auto status = snapshot.hasAsString(healthPrefix + server + "/Status");

  if (!status) {
    return std::string{Supervision::HEALTH_STATUS_UNCLEAR};
  }
  return status.value();
}

void Job::addReadLockServer(Builder& trx, std::string const& server,
                            std::string const& jobId) {
  trx.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue(OP_READ_LOCK));
    trx.add("by", VPackValue(jobId));
  }
}
void Job::addWriteLockServer(Builder& trx, std::string const& server,
                             std::string const& jobId) {
  trx.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue(OP_WRITE_LOCK));
    trx.add("by", VPackValue(jobId));
  }
}

void Job::addReadUnlockServer(Builder& trx, std::string const& server,
                              std::string const& jobId) {
  trx.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue(OP_READ_UNLOCK));
    trx.add("by", VPackValue(jobId));
  }
}

void Job::addWriteUnlockServer(Builder& trx, std::string const& server,
                               std::string const& jobId) {
  trx.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue(OP_WRITE_UNLOCK));
    trx.add("by", VPackValue(jobId));
  }
}

void Job::addPreconditionServerReadLockable(Builder& pre,
                                            std::string const& server,
                                            std::string const& jobId) {
  pre.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder shardLockEmpty(&pre);
    pre.add(PREC_CAN_READ_LOCK, VPackValue(jobId));
  }
}

void Job::addPreconditionServerReadLocked(Builder& pre,
                                          std::string const& server,
                                          std::string const& jobId) {
  pre.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder shardLockEmpty(&pre);
    pre.add(PREC_IS_READ_LOCKED, VPackValue(jobId));
  }
}

void Job::addPreconditionServerWriteLocked(Builder& pre,
                                           std::string const& server,
                                           std::string const& jobId) {
  pre.add(VPackValue(blockedServersPrefix + server));
  {
    VPackObjectBuilder shardLockEmpty(&pre);
    pre.add(PREC_IS_WRITE_LOCKED, VPackValue(jobId));
  }
}

auto Job::isReplication2Database(std::string_view database) -> bool {
  auto dbs = _snapshot.hasAsChildren(PLAN_DATABASES);
  if (!dbs) {
    return false;
  }
  return isReplicationTwoDB(*dbs, std::string{database});
}

bool Job::isServerLeaderForState(const Node& snap, std::string const& db,
                                 arangodb::replication2::LogId stateId,
                                 const std::string& server) {
  auto target = readStateTarget(snap, db, stateId);
  if (not target) {
    return false;
  }
  if (target->leader == server) {
    return true;
  }
  auto plan = readLogPlan(snap, db, stateId);
  if (not plan) {
    return false;
  }
  return plan->currentTerm.has_value() and plan->currentTerm->leader and
         plan->currentTerm->leader->serverId == server;
}

bool Job::isServerParticipantForState(const Node& snap, std::string const& db,
                                      arangodb::replication2::LogId stateId,
                                      const std::string& server) {
  auto target = readStateTarget(snap, db, stateId);
  if (target) {
    return target->participants.contains(server);
  }

  return false;
}

std::optional<arangodb::replication2::agency::LogTarget> Job::readStateTarget(
    Node const& snap, std::string const& db, replication2::LogId stateId) {
  using namespace arangodb::cluster::paths::aliases;
  auto targetPath = target()->replicatedLogs()->database(db)->log(stateId);
  auto targetNode = snap.get(targetPath);
  if (not targetNode) {
    return std::nullopt;
  }
  auto target =
      velocypack::deserialize<arangodb::replication2::agency::LogTarget>(
          targetNode->toBuilder().slice());
  return target;
}

std::optional<arangodb::replication2::agency::LogPlanSpecification>
Job::readLogPlan(Node const& snap, std::string const& db,
                 replication2::LogId stateId) {
  auto planPath = "/Plan/ReplicatedLogs/" + db + "/" + to_string(stateId);
  auto planNode = snap.get(planPath);
  if (not planNode) {
    return std::nullopt;
  }
  auto plan = velocypack::deserialize<
      arangodb::replication2::agency::LogPlanSpecification>(
      planNode->toBuilder().slice());
  return plan;
}

std::string Job::findOtherHealthyParticipant(Node const& snap,
                                             std::string const& db,
                                             replication2::LogId stateId,
                                             std::string const& serverToAvoid) {
  auto target = readStateTarget(snap, db, stateId);
  if (not target) {
    return {};
  }
  std::unordered_map<std::string, bool> good;
  for (const auto& i : *snap.hasAsChildren(healthPrefix)) {
    good[i.first] = ((*i.second).hasAsString("Status").value() ==
                     Supervision::HEALTH_STATUS_GOOD);
  }

  for (const auto& [id, participantData] : target->participants) {
    if (id == serverToAvoid) {
      // Skip current leader for which we are seeking a replacement
      continue;
    }

    if (not good[id]) {
      // Skip unhealthy servers
      continue;
    }

    if (snap.has(blockedServersPrefix + id)) {
      // server is blocked
      continue;
    }

    if (snap.has(maintenancePrefix + id)) {
      // server is in maintenance mode
      continue;
    }

    return id;
  }

  return {};
}
