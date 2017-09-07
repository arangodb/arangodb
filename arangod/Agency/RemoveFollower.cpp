////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
#include "Random/RandomGenerator.h"

using namespace arangodb::consensus;

RemoveFollower::RemoveFollower(Node const& snapshot, AgentInterface* agent,
                               std::string const& jobId,
                               std::string const& creator,
                               std::string const& database,
                               std::string const& collection,
                               std::string const& shard)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard) {}

RemoveFollower::RemoveFollower(Node const& snapshot, AgentInterface* agent,
                         JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  try {
    std::string path = pos[status] + _jobId + "/";
    _database = _snapshot.get(path + "database").getString();
    _collection = _snapshot.get(path + "collection").getString();
    _shard = _snapshot.get(path + "shard").getString();
    _creator = _snapshot.get(path + "creator").getString();
  } catch (std::exception const& e) {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency: " << e.what();
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish("", _shard, false, err.str());
    _status = FAILED;
  }
}

RemoveFollower::~RemoveFollower() {}

void RemoveFollower::run() {
  runHelper("", _shard);
}

bool RemoveFollower::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Todo: RemoveFollower(s) "
    << " to shard " << _shard << " in collection " << _collection;

  bool selfCreate = (envelope == nullptr); // Do we create ourselves?

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
  { VPackObjectBuilder guard(_jb.get());
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

  write_ret_t res = singleWriteTransaction(_agent, *_jb);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool RemoveFollower::start() {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", "", true, "collection has been dropped in the meantime");
    return false;
  }
  Node collection = _snapshot.get(planColPrefix + _database + "/" + _collection);
  if (collection.has("distributeShardsLike")) {
    finish("", "", false,
           "collection must not have 'distributeShardsLike' attribute");
    return false;
  }
  
  // Look at Plan:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  Slice planned = _snapshot.get(planPath).slice();

  TRI_ASSERT(planned.isArray());

  // First check that we still have too many followers for the current
  // `replicationFactor`:
  size_t desiredReplFactor = collection.get("replicationFactor").getUInt();
  size_t actualReplFactor = planned.length();
  if (actualReplFactor <= desiredReplFactor) {
    finish("", "", true, "job no longer necessary, have few enough replicas");
    return true;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "shard " << _shard
      << " is currently locked, not starting RemoveFollower job " << _jobId;
    return false;
  }

  // Find the group of shards that are distributed alike:
  std::vector<Job::shard_t> shardsLikeMe
      = clones(_snapshot, _database, _collection, _shard);

  // Now find some new servers to remove:
  std::unordered_map<std::string, int> overview;   // get an overview over the servers
    // -1 : not "GOOD", can be in sync, or leader, or not
    // >=0: number of servers for which it is in sync or confirmed leader
  bool leaderBad = false;
  for (auto const& srv : VPackArrayIterator(planned)) {
    std::string serverName = srv.copyString();
    if (checkServerGood(_snapshot, serverName) == "GOOD") {
      overview.emplace(serverName, 0);
    } else {
      overview.emplace(serverName, -1);
      if (serverName == planned[0].copyString()) {
        leaderBad = true;
      }
    }
  }
  doForAllShards(_snapshot, _database, shardsLikeMe,
    [this, &planned, &overview, &leaderBad](Slice plan,
                                            Slice current,
                                            std::string& planPath) {
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
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "shard " << _shard
      << " does not have a leader that has confirmed leadership, waiting, "
      "jobId=" << _jobId;
    return false;
  }

  // Check that we have enough:
  if (inSyncCount < desiredReplFactor) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "shard " << _shard
      << " does not have enough in sync followers to remove one, waiting, "
      "jobId=" << _jobId;
    return false;
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
    for (auto const& pair : overview) {
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
      for (auto const& pair : overview) {
        if (pair.second >= 0 &&
            static_cast<size_t>(pair.second) < shardsLikeMe.size()) {
          chosenToRemove.insert(pair.first);
          --currentReplFactor;
        }
        if (currentReplFactor == desiredReplFactor) {
          break;
        }
      }
      if (currentReplFactor > desiredReplFactor) {
        // Finally choose servers that are in sync, but are no leader:
        for (auto const& pair : overview) {
          if (pair.second >= 0 &&
              static_cast<size_t>(pair.second) >= shardsLikeMe.size() &&
              pair.first != planned[0].copyString()) {
            chosenToRemove.insert(pair.first);
            --currentReplFactor;
          }
          if (currentReplFactor == desiredReplFactor) {
            break;
          }
        }
      }
    }
  }
  std::vector<std::string> kept;
  for (auto const& srv : VPackArrayIterator(planned)) {
    std::string serverName = srv.copyString();
    if (chosenToRemove.find(serverName) == chosenToRemove.end()) {
      kept.push_back(serverName);
    }
  }

  // Copy todo to finished:
  Builder todo, trx;

  // Get todo entry
  { VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      try {
        _snapshot.get(toDoPrefix + _jobId).toBuilder(todo);
      } catch (std::exception const&) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC(INFO, Logger::SUPERVISION)
          << "Failed to get key " + toDoPrefix + _jobId + " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC(WARN, Logger::SUPERVISION) << e.what() << ": "
          << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }
  
  // Enter pending, remove todo, block toserver
  { VPackArrayBuilder listOfTransactions(&trx);

    { VPackObjectBuilder objectForMutation(&trx);

      addPutJobIntoSomewhere(trx, "Finished", todo.slice()[0]);
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);

      // --- Plan changes
      doForAllShards(_snapshot, _database, shardsLikeMe,
        [this, &trx, &chosenToRemove](Slice plan, Slice current,
                                      std::string& planPath) {
          trx.add(VPackValue(planPath));
          { VPackArrayBuilder serverList(&trx);
            for (auto const& srv : VPackArrayIterator(plan)) {
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
    { VPackObjectBuilder precondition(&trx);
      // --- Check that Planned servers are still as we expect
      addPreconditionUnchanged(trx, planPath, planned);
      addPreconditionShardNotBlocked(trx, _shard);
      for (auto const& srv : kept) {
        addPreconditionServerGood(trx, srv);
      }
    }   // precondition done
  }  // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, trx);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = FINISHED;
    LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Pending: RemoveFollower(s) to shard " << _shard << " in collection "
      << _collection;
    return true;
  }
  
  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Start precondition failed for RemoveFollower Job " + _jobId;
  return false;
}

JOB_STATUS RemoveFollower::status() {
  if (_status != PENDING) {
    return _status;
  }

  TRI_ASSERT(false);   // PENDING is not an option for this job, since it
                       // travels directly from ToDo to Finished or Failed
  return _status;
}

arangodb::Result RemoveFollower::abort() {

  Result result;
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    result = Result(1, "Failed aborting RemoveFollower job beyond pending stage");
    return result;
  }
  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted");
    return result;
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return result;
  
}

