////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RemoveFollower.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterInfo.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::consensus;

RemoveFollower::RemoveFollower(Node const& snapshot, AgentInterface* agent,
                               std::string const& jobId, std::string const& creator,
                               std::string const& database,
                               std::string const& collection, std::string const& shard)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard) {}

RemoveFollower::RemoveFollower(Node const& snapshot, AgentInterface* agent,
                               JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");
  auto tmp_shard = _snapshot.hasAsString(path + "shard");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");

  if (tmp_database.second && tmp_collection.second && tmp_shard.second &&
      tmp_creator.second) {
    _database = tmp_database.first;
    _collection = tmp_collection.first;
    _shard = tmp_shard.first;
    _creator = tmp_creator.first;
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency.";
    LOG_TOPIC("ac178", ERR, Logger::SUPERVISION) << err.str();
    finish("", tmp_shard.first, false, err.str());
    _status = FAILED;
  }
}

RemoveFollower::~RemoveFollower() = default;

void RemoveFollower::run(bool& aborts) { runHelper("", _shard, aborts); }

bool RemoveFollower::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC("26152", DEBUG, Logger::SUPERVISION)
      << "Todo: RemoveFollower(s) "
      << " to shard " << _shard << " in collection " << _collection;

  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string now(timepointToString(std::chrono::system_clock::now()));

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  std::string path = toDoPrefix + _jobId;

  _jb->add(VPackValue(path));
  {
    VPackObjectBuilder guard(_jb.get());
    _jb->add("creator", VPackValue(_creator));
    _jb->add("type", VPackValue("removeFollower"));
    _jb->add("database", VPackValue(_database));
    _jb->add("collection", VPackValue(_collection));
    _jb->add("shard", VPackValue(_shard));
    _jb->add("jobId", VPackValue(_jobId));
    _jb->add("timeCreated", VPackValue(now));
  }

  _status = TODO;

  if (!selfCreate) {
    return true;
  }

  _jb->close();  // transaction object
  _jb->close();  // close array

  write_ret_t res = singleWriteTransaction(_agent, *_jb, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC("83bf8", INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool RemoveFollower::start(bool&) {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", "", true, "collection has been dropped in the meantime");
    return false;
  }
  Node const& collection =
      _snapshot.hasAsNode(planColPrefix + _database + "/" + _collection).first;
  if (collection.has("distributeShardsLike")) {
    finish("", "", false,
           "collection must not have 'distributeShardsLike' attribute");
    return false;
  }

  // Look at Plan:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  Slice planned = _snapshot.hasAsSlice(planPath).first;

  TRI_ASSERT(planned.isArray());

  // First check that we still have too many followers for the current
  // `replicationFactor`:
  size_t desiredReplFactor = 1;
  auto replFact = collection.hasAsUInt(StaticStrings::ReplicationFactor);
  if (replFact.second) {
    desiredReplFactor = replFact.first;
  } else {
    auto replFact2 = collection.hasAsString(StaticStrings::ReplicationFactor);
    if (replFact2.second && replFact2.first == StaticStrings::Satellite) {
      // satellites => distribute to every server
      auto available = Job::availableServers(_snapshot);
      desiredReplFactor = Job::countGoodOrBadServersInList(_snapshot, available);
    }
  }

  size_t actualReplFactor = planned.length();
  if (actualReplFactor <= desiredReplFactor) {
    finish("", "", true, "job no longer necessary, have few enough replicas");
    return true;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC("cef47", DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " is currently locked, not starting RemoveFollower job " << _jobId;
    return false;
  }

  // Find the group of shards that are distributed alike:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  // Now find some new servers to remove:
  std::unordered_map<std::string, int> overview;  // get an overview over the servers
                                                  // -1 : not "GOOD", can be in sync, or leader, or not
                                                  // >=0: number of servers for which it is in sync or confirmed leader
  bool leaderBad = false;
  for (VPackSlice srv : VPackArrayIterator(planned)) {
    std::string serverName = srv.copyString();
    if (checkServerHealth(_snapshot, serverName) == "GOOD") {
      overview.try_emplace(serverName, 0);
    } else {
      overview.try_emplace(serverName, -1);
      if (serverName == planned[0].copyString()) {
        leaderBad = true;
      }
    }
  }
  doForAllShards(_snapshot, _database, shardsLikeMe,
                 [&planned, &overview, &leaderBad](Slice plan, Slice current,
                                                   std::string& planPath, std::string& curPath) {
                   if (current.length() > 0) {
                     if (current[0].copyString() != planned[0].copyString()) {
                       leaderBad = true;
                     } else {
                       for (auto const s : VPackArrayIterator(current)) {
                         auto it = overview.find(s.copyString());
                         if (it != overview.end()) {
                           if (it->second >= 0) {
                             ++(it->second);
                           }
                         }
                       }
                     }
                   } else {
                     leaderBad = true;
                   }
                 });

  size_t inSyncCount = 0;
  size_t notGoodCount = 0;
  for (auto const& pair : overview) {
    if (pair.second == -1) {
      ++notGoodCount;
    } else if (static_cast<size_t>(pair.second) >= shardsLikeMe.size()) {
      ++inSyncCount;
    }
  }

  // Check that leader is OK for all shards:
  if (leaderBad) {
    LOG_TOPIC("d4845", DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " does not have a leader that has confirmed leadership, waiting, "
           "jobId="
        << _jobId;
    finish("", "", false, "job no longer sensible, leader has gone bad");
    return false;
  }

  // Check that we have enough:
  if (inSyncCount < desiredReplFactor) {
    LOG_TOPIC("d9118", DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " does not have enough in sync followers to remove one, waiting, "
           "jobId="
        << _jobId;
    finish("", "", false, "job no longer sensible, do not have few enough replicas");
    return true;
  }

  // We now know actualReplFactor >= inSyncCount + noGoodCount and
  //                  inSyncCount >= desiredReplFactor
  // Let notInSyncCount = actualReplFactor - (inSyncCount + noGoodCount)
  // To reduce actualReplFactor down to desiredReplFactor we
  // Randomly choose enough servers:
  std::unordered_set<std::string> chosenToRemove;
  size_t currentReplFactor = actualReplFactor;  // will be counted down
  if (currentReplFactor > desiredReplFactor) {
    // First choose BAD servers:

    // Iterate the list of planned servers in reverse order,
    // because the last must be removed first
    std::vector<ServerID> reversedPlannedServers{planned.length()};
    {
      auto rDbIt = reversedPlannedServers.rbegin();
      for (auto const& vPackIt : VPackArrayIterator(planned)) {
        *rDbIt = vPackIt.copyString();
        rDbIt++;
      }
    }

    for (auto const& it : reversedPlannedServers) {
      auto const pair = *overview.find(it);
      if (pair.second == -1) {
        chosenToRemove.insert(pair.first);
        --currentReplFactor;
      }
      if (currentReplFactor == desiredReplFactor) {
        break;
      }
    }
    if (currentReplFactor > desiredReplFactor) {
      // Now choose servers that are not in sync for all shards:
      for (auto const& it : reversedPlannedServers) {
        auto const pair = *overview.find(it);
        if (pair.second >= 0 && static_cast<size_t>(pair.second) < shardsLikeMe.size()) {
          chosenToRemove.insert(pair.first);
          --currentReplFactor;
        }
        if (currentReplFactor == desiredReplFactor) {
          break;
        }
      }
      if (currentReplFactor > desiredReplFactor) {
        // Finally choose servers that are in sync, but are no leader:
        for (auto const& it : reversedPlannedServers) {
          auto const pair = *overview.find(it);
          if (pair.second >= 0 &&
              static_cast<size_t>(pair.second) >= shardsLikeMe.size() &&
              pair.first != planned[0].copyString()) {
            if (Job::isInServerList(_snapshot, toBeCleanedPrefix, pair.first, true) ||
                Job::isInServerList(_snapshot, cleanedPrefix, pair.first, true)) {
              // Prefer those cleaned or to be cleaned servers
              chosenToRemove.insert(pair.first);
              --currentReplFactor;
            }
          }
          if (currentReplFactor == desiredReplFactor) {
            break;
          }
        }
        if (currentReplFactor > desiredReplFactor) {
          // Now allow those which are perfectly good as well:
          for (auto const& it : reversedPlannedServers) {
            auto const pair = *overview.find(it);
            if (pair.second >= 0 &&
                static_cast<size_t>(pair.second) >= shardsLikeMe.size() &&
                pair.first != planned[0].copyString()) {
              if (!Job::isInServerList(_snapshot, toBeCleanedPrefix, pair.first, true) &&
                  !Job::isInServerList(_snapshot, cleanedPrefix, pair.first, true)) {
                chosenToRemove.insert(pair.first);
                --currentReplFactor;
              }
            }
            if (currentReplFactor == desiredReplFactor) {
              break;
            }
          }
        }
      }
    }
  }
  std::vector<std::string> kept;
  for (VPackSlice srv : VPackArrayIterator(planned)) {
    std::string serverName = srv.copyString();
    if (chosenToRemove.find(serverName) == chosenToRemove.end()) {
      kept.push_back(serverName);
    }
  }

  // Copy todo to finished:
  Builder todo, trx;

  // Get todo entry
  {
    VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo.second) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC("4fac6", INFO, Logger::SUPERVISION) << "Failed to get key " + toDoPrefix + _jobId +
                                                    " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC("95927", WARN, Logger::SUPERVISION)
            << e.what() << ": " << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }

  // Enter pending, remove todo, block toserver
  {
    VPackArrayBuilder listOfTransactions(&trx);

    {
      VPackObjectBuilder objectForMutation(&trx);

      addPutJobIntoSomewhere(trx, "Finished", todo.slice()[0]);
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);

      // --- Plan changes
      doForAllShards(_snapshot, _database, shardsLikeMe,
                     [&trx, &chosenToRemove](Slice plan, Slice current, std::string& planPath, std::string& curPath) {
                       trx.add(VPackValue(planPath));
                       {
                         VPackArrayBuilder serverList(&trx);
                         for (VPackSlice srv : VPackArrayIterator(plan)) {
                           if (chosenToRemove.find(srv.copyString()) ==
                               chosenToRemove.end()) {
                             trx.add(srv);
                           }
                         }
                       }
                     });

      addIncreasePlanVersion(trx);
    }  // mutation part of transaction done
    // Preconditions
    {
      VPackObjectBuilder precondition(&trx);
      // --- Check that Planned servers are still as we expect
      addPreconditionUnchanged(trx, planPath, planned);
      addPreconditionShardNotBlocked(trx, _shard);
      for (auto const& srv : kept) {
        addPreconditionServerHealth(trx, srv, "GOOD");
      }
    }  // precondition done
  }    // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = FINISHED;
    LOG_TOPIC("9ec40", DEBUG, Logger::SUPERVISION)
        << "Pending: RemoveFollower(s) to shard " << _shard << " in collection "
        << _collection;
    return true;
  }

  LOG_TOPIC("f2df8", INFO, Logger::SUPERVISION)
      << "Start precondition failed for RemoveFollower Job " + _jobId;
  return false;
}

JOB_STATUS RemoveFollower::status() {
  if (_status != PENDING) {
    return _status;
  }

  TRI_ASSERT(false);  // PENDING is not an option for this job, since it
                      // travels directly from ToDo to Finished or Failed
  return _status;
}

arangodb::Result RemoveFollower::abort(std::string const& reason) {
  Result result;
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    result = Result(TRI_ERROR_INTERNAL,
                    "Failed aborting RemoveFollower job beyond pending stage");
    return result;
  }
  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted:" + reason);
    return result;
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return result;
}
