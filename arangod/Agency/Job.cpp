///////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Job.h"
#include "Basics/StringUtils.h"
#include "Random/RandomGenerator.h"

#include <numeric>

static std::string const DBServer = "DBServer";

namespace arangodb {
namespace consensus {

std::string const mapUniqueToShortID = "/Target/MapUniqueToShortID/";
std::string const pendingPrefix = "/Target/Pending/";
std::string const failedPrefix = "/Target/Failed/";
std::string const finishedPrefix = "/Target/Finished/";
std::string const toDoPrefix = "/Target/ToDo/";
std::string const cleanedPrefix = "/Target/CleanedServers";
std::string const failedServersPrefix = "/Target/FailedServers";
std::string const planColPrefix = "/Plan/Collections/";
std::string const curColPrefix = "/Current/Collections/";
std::string const blockedServersPrefix = "/Supervision/DBServers/";
std::string const blockedShardsPrefix = "/Supervision/Shards/";
std::string const planVersion = "/Plan/Version";
std::string const plannedServers = "/Plan/DBServers";
std::string const healthPrefix = "/Supervision/Health/";
std::string const asyncReplLeader = "/Plan/AsyncReplication/Leader";
std::string const asyncReplTransientPrefix = "/AsyncReplication/";

}  // namespace arangodb::consensus
}  // namespace arangodb

using namespace arangodb::basics;
using namespace arangodb::consensus;

Job::Job(JOB_STATUS status, Node const& snapshot, AgentInterface* agent,
         std::string const& jobId, std::string const& creator)
  : _status(status),
    _snapshot(snapshot),
    _agent(agent),
    _jobId(jobId),
    _creator(creator),
    _jb(nullptr) {
}

Job::~Job() {}

// this will be initialized in the AgencyFeature
std::string Job::agencyPrefix = "arango";

bool Job::finish(
  std::string const& server, std::string const& shard,
  bool success, std::string const& reason, query_t const payload) {

  Builder pending, finished;

  // Get todo entry
  bool started = false;
  { VPackArrayBuilder guard(&pending);
    if (_snapshot.exists(pendingPrefix + _jobId).size() == 3) {
      _snapshot.hasAsBuilder(pendingPrefix + _jobId, pending);
      started = true;
    } else if (_snapshot.exists(toDoPrefix + _jobId).size() == 3) {
      _snapshot.hasAsBuilder(toDoPrefix + _jobId, pending);
    } else {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Nothing in pending to finish up for job " << _jobId;
      return false;
    }
  }

  std::string jobType;
  try {
    jobType = pending.slice()[0].get("type").copyString();
  } catch (std::exception const&) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Failed to obtain type of job " << _jobId;
  }

  // Prepare pending entry, block toserver
  { VPackArrayBuilder guard(&finished);
    VPackObjectBuilder guard2(&finished);

    addPutJobIntoSomewhere(finished, success ? "Finished" : "Failed",
                           pending.slice()[0], reason);

    addRemoveJobFromSomewhere(finished, "ToDo", _jobId);
    addRemoveJobFromSomewhere(finished, "Pending", _jobId);

    // Additional payload, which is to be executed in the finish transaction
    if (payload != nullptr) {
      Slice slice = payload->slice();
      TRI_ASSERT(slice.isObject());
      if (slice.length() > 0) {
        for (auto const& oper : VPackObjectIterator(slice)) {
          finished.add(oper.key.copyString(), oper.value);
        }
      }
    }

    // --- Remove blocks if specified:
    if (started && !server.empty()) {
      addReleaseServer(finished, server);
    }
    if (started && !shard.empty()) {
      addReleaseShard(finished, shard);
    }

  }  // close object and array

  write_ret_t res = singleWriteTransaction(_agent, finished);
  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Successfully finished job " << jobType << "(" << _jobId << ")";
    _status = (success ? FINISHED : FAILED);
    return true;
  }

  return false;
}



std::string Job::randomIdleGoodAvailableServer(
  Node const& snap, std::vector<std::string> const& exclude) {

  std::vector<std::string> as = availableServers(snap);
  std::string ret;
  auto ex(exclude);

  // ungood;
  try {
    for (auto const& srv : snap.hasAsChildren(healthPrefix).first) {
      if ((*srv.second).hasAsString("Status").first != "GOOD") {
        ex.push_back(srv.first);
      }
    }
  } catch (...) {}

  // blocked;
  try {
    for (auto const& srv : snap.hasAsChildren(blockedServersPrefix).first) {
      ex.push_back(srv.first);
    }
  } catch (...) {}


  // Remove excluded servers
  std::sort(std::begin(ex), std::end(ex));
  as.erase(
    std::remove_if(
      std::begin(as),std::end(as),
      [&](std::string const& s){
        return std::binary_search(
          std::begin(ex), std::end(ex), s);}), std::end(as));

  // Choose random server from rest
  if (!as.empty()) {
    if (as.size() == 1) {
      ret = as[0];
    } else {
      uint16_t interval = static_cast<uint16_t>(as.size() - 1);
      uint16_t random = RandomGenerator::interval(interval);
      ret = as.at(random);
    }
  }

  return ret;

}


std::string Job::randomIdleGoodAvailableServer(Node const& snap,
                                               Slice const& exclude) {

  std::vector<std::string> ev;
  if (exclude.isArray()) {
    for (const auto& s : VPackArrayIterator(exclude)) {
      if (s.isString()) {
        ev.push_back(s.copyString());
      }
    }
  }
  return randomIdleGoodAvailableServer(snap,ev);

}

/// @brief Get servers from plan, which are not failed or cleaned out
std::vector<std::string> Job::availableServers(Node const& snapshot) {

  std::vector<std::string> ret;

  // Get servers from plan
  Node::Children const& dbservers = snapshot.hasAsChildren(plannedServers).first;
  for (auto const& srv : dbservers) {
    ret.push_back(srv.first);
  }

  // Remove cleaned servers from list (test first to avoid warning log
  if (snapshot.has(cleanedPrefix)) try {
    for (auto const& srv :
           VPackArrayIterator(snapshot.hasAsSlice(cleanedPrefix).first)) {
      ret.erase(
        std::remove(ret.begin(), ret.end(), srv.copyString()),
        ret.end());
    }
  } catch (...) {}

  // Remove failed servers from list (test first to avoid warning log)
  if (snapshot.has(failedServersPrefix)) try {
    for (auto const& srv : snapshot.hasAsChildren(failedServersPrefix).first) {
      ret.erase(
        std::remove(ret.begin(), ret.end(), srv.first), ret.end());
    }
  } catch (...) {}

  return ret;

}

/// @brief Get servers from Supervision with health status GOOD
std::vector<std::string> Job::healthyServers(arangodb::consensus::Node const& snapshot) {
  std::vector<std::string> ret;
  for (auto const& srv : snapshot(healthPrefix).children()) {
    auto healthState = srv.second->hasAsString("Status");
    if (healthState.second && healthState.first == Supervision::HEALTH_STATUS_GOOD) {
      ret.emplace_back(srv.first);
    }
  }
  return ret;
}

template<typename T> std::vector<size_t> idxsort (const std::vector<T> &v) {

  std::vector<size_t> idx(v.size());

  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(),
       [&v](size_t i, size_t j) {return v[i] < v[j];});

  return idx;

}

std::vector<std::string> sortedShardList(Node const& shards) {

  std::vector<size_t> sids;
  auto const& shardMap = shards.children();
  for (const auto& shard : shardMap) {
    sids.push_back(StringUtils::uint64(shard.first.substr(1)));
  }

  std::vector<size_t> idx(idxsort(sids));
  std::vector<std::string> sorted;
  for (const auto& i : idx) {
    auto x = shardMap.begin();
    std::advance(x,i);
    sorted.push_back(x->first);
  }

  return sorted;

}

std::vector<Job::shard_t> Job::clones(
  Node const& snapshot, std::string const& database,
  std::string const& collection, std::string const& shard) {

  std::vector<shard_t> ret;
  ret.emplace_back(collection, shard);  // add (collection, shard) as first item
  //typedef std::unordered_map<std::string, std::shared_ptr<Node>> UChildren;

  std::string databasePath = planColPrefix + database,
    planPath = databasePath + "/" + collection + "/shards";

  auto myshards = sortedShardList(snapshot.hasAsNode(planPath).first);
  auto steps = std::distance(
    myshards.begin(), std::find(myshards.begin(), myshards.end(), shard));

  for (const auto& colptr : snapshot.hasAsChildren(databasePath).first) { // collections

    auto const col = *colptr.second;
    auto const otherCollection = colptr.first;

    if (otherCollection != collection &&
        col.has("distributeShardsLike") && // use .has() form to prevent logging of missing
        col.hasAsSlice("distributeShardsLike").first.copyString() == collection) {
      auto const theirshards = sortedShardList(col("shards"));
      if (theirshards.size() > 0) { // do not care about virtual collections
        if (theirshards.size() == myshards.size()) {
          ret.emplace_back(otherCollection,
                           sortedShardList(col.hasAsNode("shards").first)[steps]);
        } else {
          LOG_TOPIC(ERR, Logger::SUPERVISION)
            << "Shard distribution of clone(" << otherCollection
            << ") does not match ours (" << collection << ")";
        }
      }
    }
  }

  return ret;
}


std::string Job::findNonblockedCommonHealthyInSyncFollower( // Which is in "GOOD" health
  Node const& snap, std::string const& db, std::string const& col,
  std::string const& shrd) {

  auto cs = clones(snap, db, col, shrd);       // clones
  auto nclones = cs.size();                    // #clones
  std::unordered_map<std::string,bool> good;

  for (const auto& i : snap.hasAsChildren(healthPrefix).first) {
    good[i.first] = ((*i.second).hasAsString("Status").first == "GOOD");
  }

  std::unordered_map<std::string,size_t> currentServers;
  for (const auto& clone : cs) {
    auto currentShardPath =
      curColPrefix + db + "/" + clone.collection + "/"
      + clone.shard + "/servers";
    auto plannedShardPath =
      planColPrefix + db + "/" + clone.collection + "/shards/"
      + clone.shard;
    size_t i = 0;

    // start up race condition  ... current might not have everything in plan
    if (!snap.has(currentShardPath) || !snap.has(plannedShardPath)) {
      --nclones;
      continue;
    } // if

    for (const auto& server : VPackArrayIterator(snap.hasAsArray(currentShardPath).first)) {
      auto id = server.copyString();
      if (i++ == 0) {
        // Skip leader
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

      // check if it is also part of the plan...because if not the soon to be leader
      // will drop the collection
      bool found = false;
      for (const auto& plannedServer : VPackArrayIterator(snap.hasAsArray(plannedShardPath).first)) {
        if (plannedServer == server) {
          found = true;
          continue;
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

std::string Job::uuidLookup (std::string const& shortID) {
  for (auto const& uuid : _snapshot.hasAsChildren(mapUniqueToShortID).first) {
    if ((*uuid.second).hasAsString("ShortName").first == shortID) {
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
  if (!job.second || !job.first.has("type")) {
    return false;
  }
  auto const& tmp_type = job.first.hasAsString("type");

  std::string const& type = tmp_type.first;
  if (!tmp_type.second || type == "failedServer" || type == "failedLeader" ||
      type == "activeFailover") {
    return false;
  } else if (type == "addFollower" || type == "moveShard" ||
             type == "cleanOutServer") {
    return true;
  }

  // We should never get here
  TRI_ASSERT(false);
  return false;
}

void Job::doForAllShards(Node const& snapshot,
  std::string& database,
  std::vector<shard_t>& shards,
  std::function<void(Slice plan, Slice current, std::string& planPath)> worker) {
  for (auto const& collShard : shards) {
    std::string shard = collShard.shard;
    std::string collection = collShard.collection;

    std::string planPath =
      planColPrefix + database + "/" + collection + "/shards/" + shard;
    std::string curPath = curColPrefix + database + "/" + collection
      + "/" + shard + "/servers";

    Slice plan = snapshot.hasAsSlice(planPath).first;
    Slice current = snapshot.hasAsSlice(curPath).first;

    worker(plan, current, planPath);
  }
}

void Job::addIncreasePlanVersion(Builder& trx) {
  trx.add(VPackValue(planVersion));
  { VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("increment"));
  }
}

void Job::addRemoveJobFromSomewhere(Builder& trx, std::string const& where,
  std::string const& jobId) {
  trx.add(VPackValue("/Target/" + where + "/" + jobId));
  { VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

void Job::addPutJobIntoSomewhere(Builder& trx, std::string const& where, Slice job,
    std::string const& reason) {
  Slice jobIdSlice = job.get("jobId");
  TRI_ASSERT(jobIdSlice.isString());
  std::string jobId = jobIdSlice.copyString();
  trx.add(VPackValue("/Target/" + where + "/" + jobId));
  { VPackObjectBuilder guard(&trx);
    if (where == "Pending") {
      trx.add("timeStarted",
        VPackValue(timepointToString(std::chrono::system_clock::now())));
    } else {
      trx.add("timeFinished",
        VPackValue(timepointToString(std::chrono::system_clock::now())));
    }
    for (auto const& obj : VPackObjectIterator(job)) {
      trx.add(obj.key.copyString(), obj.value);
    }
    if (!reason.empty()) {
      trx.add("reason", VPackValue(reason));
    }
  }
}

void Job::addPreconditionCollectionStillThere(Builder& pre,
    std::string const& database, std::string const& collection) {
  std::string planPath
      = planColPrefix + database + "/" + collection;
  pre.add(VPackValue(planPath));
  { VPackObjectBuilder guard(&pre);
    pre.add("oldEmpty", VPackValue(false));
  }
}

void Job::addPreconditionServerNotBlocked(Builder& pre, std::string const& server) {
  pre.add(VPackValue(blockedServersPrefix + server));
  { VPackObjectBuilder serverLockEmpty(&pre);
    pre.add("oldEmpty", VPackValue(true));
  }
}

void Job::addPreconditionServerHealth(Builder& pre, std::string const& server,
                                      std::string const& health) {
  pre.add(VPackValue(healthPrefix + server + "/Status"));
  { VPackObjectBuilder serverGood(&pre);
    pre.add("old", VPackValue(health));
  }
}

void Job::addPreconditionShardNotBlocked(Builder& pre, std::string const& shard) {
  pre.add(VPackValue(blockedShardsPrefix + shard));
  { VPackObjectBuilder shardLockEmpty(&pre);
    pre.add("oldEmpty", VPackValue(true));
  }
}

void Job::addPreconditionUnchanged(Builder& pre,
    std::string const& key, Slice value) {
  pre.add(VPackValue(key));
  { VPackObjectBuilder guard(&pre);
    pre.add("old", value);
  }
}

void Job::addBlockServer(Builder& trx, std::string const& server, std::string const& jobId) {
  trx.add(blockedServersPrefix + server, VPackValue(jobId));
}

void Job::addBlockShard(Builder& trx, std::string const& shard, std::string const& jobId) {
  trx.add(blockedShardsPrefix + shard, VPackValue(jobId));
}

void Job::addReleaseServer(Builder& trx, std::string const& server) {
  trx.add(VPackValue(blockedServersPrefix + server));
  { VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

void Job::addReleaseShard(Builder& trx, std::string const& shard) {
  trx.add(VPackValue(blockedShardsPrefix + shard));
  { VPackObjectBuilder guard(&trx);
    trx.add("op", VPackValue("delete"));
  }
}

std::string Job::checkServerHealth(Node const& snapshot,
                                   std::string const& server) {
  auto status = snapshot.hasAsString(healthPrefix + server + "/Status");
  
  if (!status.second) {
    return "UNCLEAR";
  }
  return status.first;
}
