////////////////////////////////////////////////////////////////////////////////
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

#include "MoveShard.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Cluster/ClusterHelpers.h"

using namespace arangodb;
using namespace arangodb::consensus;

MoveShard::MoveShard(Node const& snapshot, AgentInterface* agent,
                     std::string const& jobId, std::string const& creator,
                     std::string const& database,
                     std::string const& collection, std::string const& shard,
                     std::string const& from, std::string const& to,
                     bool isLeader, bool remainsFollower)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(id(from)),
      _to(id(to)),
      _isLeader(isLeader), // will be initialized properly when information known
      _remainsFollower(remainsFollower)
{ }

MoveShard::MoveShard(Node const& snapshot, AgentInterface* agent,
                     std::string const& jobId, std::string const& creator,
                     std::string const& database,
                     std::string const& collection, std::string const& shard,
                     std::string const& from, std::string const& to,
                     bool isLeader)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(id(from)),
      _to(id(to)),
      _isLeader(isLeader), // will be initialized properly when information known
      _remainsFollower(isLeader)
{ }

MoveShard::MoveShard(Node const& snapshot, AgentInterface* agent,
                     JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {

  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");
  auto tmp_from = _snapshot.hasAsString(path + "fromServer");
  auto tmp_to = _snapshot.hasAsString(path + "toServer");
  auto tmp_shard = _snapshot.hasAsString(path + "shard");
  auto tmp_isLeader = _snapshot.hasAsSlice(path + "isLeader");
  auto tmp_remainsFollower = _snapshot.hasAsSlice(path + "remainsFollower");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");

  if (tmp_database.second && tmp_collection.second && tmp_from.second && tmp_to.second
      && tmp_shard.second && tmp_creator.second && tmp_isLeader.second) {
    _database = tmp_database.first;
    _collection = tmp_collection.first;
    _from = tmp_from.first;
    _to = tmp_to.first;
    _shard = tmp_shard.first;
    _isLeader = tmp_isLeader.first.isTrue();
    _remainsFollower = tmp_remainsFollower.second ? tmp_remainsFollower.first.isTrue() : _isLeader;
    _creator = tmp_creator.first;
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish("", _shard, false, err.str());
    _status = FAILED;
  }
}

MoveShard::~MoveShard() {}

void MoveShard::run() {
  runHelper(_to, _shard);
}

bool MoveShard::create(std::shared_ptr<VPackBuilder> envelope) {

  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
    << "Todo: Move shard " + _shard + " from " + _from + " to " << _to;

  bool selfCreate = (envelope == nullptr); // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string now(timepointToString(std::chrono::system_clock::now()));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // DBservers
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  Slice plan = _snapshot.hasAsSlice(planPath).first;
  TRI_ASSERT(plan.isArray());
  TRI_ASSERT(plan[0].isString());
#endif

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  _jb->add(VPackValue(_from == _to ? failedPrefix + _jobId
                                   : toDoPrefix + _jobId));
  { VPackObjectBuilder guard2(_jb.get());
    if (_from == _to) {
      _jb->add("timeFinished", VPackValue(now));
      _jb->add(
        "result",
        VPackValue("Source and destination of moveShard must be different"));
    }
    _jb->add("creator", VPackValue(_creator));
    _jb->add("type", VPackValue("moveShard"));
    _jb->add("database", VPackValue(_database));
    _jb->add("collection", VPackValue(_collection));
    _jb->add("shard", VPackValue(_shard));
    _jb->add("fromServer", VPackValue(_from));
    _jb->add("toServer", VPackValue(_to));
    _jb->add("isLeader", VPackValue(_isLeader));
    _jb->add("remainsFollower", VPackValue(_remainsFollower));
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

bool MoveShard::start() {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Check if the fromServer exists:
  if (!_snapshot.has(plannedServers + "/" + _from)) {
    finish("", "", false, "fromServer does not exist as DBServer in Plan");
    return false;
  }

  // Check if the toServer exists:
  if (!_snapshot.has(plannedServers + "/" + _to)) {
    finish("", "", false, "toServer does not exist as DBServer in Plan");
    return false;
  }

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", "", true, "collection has been dropped in the meantime");
    return false;
  }
  auto collection
    = _snapshot.hasAsNode(planColPrefix + _database + "/" + _collection);
  if (collection.second && collection.first.has("distributeShardsLike")) {
    finish("", "", false,
           "collection must not have 'distributeShardsLike' attribute");
    return false;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "shard " << _shard
      << " is currently locked, not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that the toServer is not locked:
  if (_snapshot.has(blockedServersPrefix + _to)) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "server " << _to << " is currently"
      " locked, not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that the toServer is in state "GOOD":
  std::string health = checkServerHealth(_snapshot, _to);
  if (health != "GOOD") {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION) << "server " << _to
      << " is currently " << health << ", not starting MoveShard job "
      << _jobId;
      return false;
  }

  // Check that _to is not in `Target/CleanedServers`:
  VPackBuilder cleanedServersBuilder;
  auto cleanedServersNode = _snapshot.hasAsBuilder(cleanedPrefix, cleanedServersBuilder);
  if (!cleanedServersNode.second) {
    // ignore this check
    cleanedServersBuilder.clear();
    {
      VPackArrayBuilder guard(&cleanedServersBuilder);
    }
  }
  VPackSlice cleanedServers = cleanedServersBuilder.slice();
  if (cleanedServers.isArray()) {
    for (auto const& x : VPackArrayIterator(cleanedServers)) {
      if (x.isString() && x.copyString() == _to) {
        finish("", "", false,
               "toServer must not be in `Target/CleanedServers`");
        return false;
      }
    }
  }

  // Check that _to is not in `Target/FailedServers`:
  VPackBuilder failedServersBuilder;
  auto failedServersNode = _snapshot.hasAsBuilder(failedServersPrefix, failedServersBuilder);
  if (!failedServersNode.second) {
    // ignore this check
    failedServersBuilder.clear();
    {
      VPackObjectBuilder guard(&failedServersBuilder);
    }
  }
  VPackSlice failedServers = failedServersBuilder.slice();
  if (failedServers.isObject()) {
    Slice found = failedServers.get(_to);
    if (!found.isNone()) {
      finish("", "", false, "toServer must not be in `Target/FailedServers`");
      return false;
    }
  }

  // Look at Plan:
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  Slice planned = _snapshot.hasAsSlice(planPath).first;
  TRI_ASSERT(planned.isArray());

  int found = -1;
  int count = 0;
  for (auto const& srv : VPackArrayIterator(planned)) {
    TRI_ASSERT(srv.isString());
    if (srv.copyString() == _to) {
      finish("", "", false, "toServer must not yet be planned for shard");
      return false;
    }
    if (srv.copyString() == _from) {
      found = count;
    }
    ++count;
  }
  if ((_isLeader && found != 0) ||
      (!_isLeader && found < 1)) {
    if (_isLeader) {
      finish("", "", false, "fromServer must be the leader in plan for shard");
    } else {
      finish("", "", false, "fromServer must be a follower in plan for shard");
    }
    return false;
  }

  if (!_isLeader && _remainsFollower) {
    finish("", "", false, "remainsFollower is invalid without isLeader");
    return false;
  }

  // Compute group to move shards together:
  std::vector<Job::shard_t> shardsLikeMe
      = clones(_snapshot, _database, _collection, _shard);

  // Copy todo to pending
  Builder todo, pending;

  // Get todo entry
  { VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo.second) {
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
  { VPackArrayBuilder listOfTransactions(&pending);

    { VPackObjectBuilder objectForMutation(&pending);

      addPutJobIntoSomewhere(pending, "Pending", todo.slice()[0]);
      addRemoveJobFromSomewhere(pending, "ToDo", _jobId);

      addBlockShard(pending, _shard, _jobId);
      addBlockServer(pending, _to, _jobId);

      // --- Plan changes
      doForAllShards(_snapshot, _database, shardsLikeMe,
        [this, &pending](Slice plan, Slice current, std::string& planPath) {
          pending.add(VPackValue(planPath));
          { VPackArrayBuilder serverList(&pending);
            if (_isLeader) {
              TRI_ASSERT(plan[0].copyString() != _to);
              pending.add(plan[0]);
              pending.add(VPackValue(_to));
              for (size_t i = 1; i < plan.length(); ++i) {
                pending.add(plan[i]);
                TRI_ASSERT(plan[i].copyString() != _to);
              }
            } else {
              for (auto const& srv : VPackArrayIterator(plan)) {
                pending.add(srv);
                TRI_ASSERT(srv.copyString() != _to);
              }
              pending.add(VPackValue(_to));
            }
          }
        });

      addIncreasePlanVersion(pending);
    }  // mutation part of transaction done

    // Preconditions
    { VPackObjectBuilder precondition(&pending);

      // --- Check that Planned servers are still as we expect
      addPreconditionUnchanged(pending, planPath, planned);
      addPreconditionShardNotBlocked(pending, _shard);
      addPreconditionServerNotBlocked(pending, _to);
      addPreconditionServerHealth(pending, _to, "GOOD");
      addPreconditionUnchanged(pending, failedServersPrefix, failedServers);
      addPreconditionUnchanged(pending, cleanedPrefix, cleanedServers);
    }   // precondition done

  }  // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " + _shard + " from " + _from + " to " + _to;
    return true;
  }

  LOG_TOPIC(INFO, Logger::SUPERVISION)
    << "Start precondition failed for MoveShard job " + _jobId;
  return false;
}

JOB_STATUS MoveShard::status() {
  if (_status != PENDING) {
    return _status;
  }

  // check that shard still there, otherwise finish job
  std::string planPath = planColPrefix + _database + "/" + _collection;
  if (!_snapshot.has(planPath)) {
    // Oops, collection is gone, simple finish job:
    finish("", _shard, true, "collection was dropped");
    return FINISHED;
  }

  if (_isLeader) {
    return pendingLeader();
  } else {
    return pendingFollower();
  }
}

JOB_STATUS MoveShard::pendingLeader() {
  auto considerTimeout = [&]() -> bool {
    // Not yet all in sync, consider timeout:
    std::string timeCreatedString
      = _snapshot.hasAsString(pendingPrefix + _jobId + "/timeCreated").first;
    Supervision::TimePoint timeCreated = stringToTimepoint(timeCreatedString);
    Supervision::TimePoint now(std::chrono::system_clock::now());
    if (now - timeCreated > std::chrono::duration<double>(10000.0)) {
      abort();
      return true;
    }
    return false;
  };

  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe
    = clones(_snapshot, _database, _collection, _shard);

  // Consider next step, depending on who is currently the leader
  // in the Plan:
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  Slice plan = _snapshot.hasAsSlice(planPath).first;
  Builder trx;
  Builder pre;  // precondition
  bool finishedAfterTransaction = false;

  if (plan[0].copyString() == _from) {
    // Still the old leader, let's check that the toServer is insync:
    size_t done = 0;   // count the number of shards for which _to is in sync:
    doForAllShards(_snapshot, _database, shardsLikeMe,
      [this, &done](Slice plan, Slice current, std::string& planPath) {
        for (auto const& s : VPackArrayIterator(current)) {
          if (s.copyString() == _to) {
            ++done;
            return;
          }
        }
      });

    // Consider timeout:
    if (done < shardsLikeMe.size()) {
      if (considerTimeout()) {
        return FAILED;
      }
      return PENDING;  // do not act
    }

    // We need to ask the old leader to retire:
    { VPackArrayBuilder trxArray(&trx);
      { VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        doForAllShards(_snapshot, _database, shardsLikeMe,
          [this, &trx, &pre](Slice plan, Slice current, std::string& planPath) {
            // Replace _from by "_" + _from
            trx.add(VPackValue(planPath));
            { VPackArrayBuilder guard(&trx);
              for (auto const& srv : VPackArrayIterator(plan)) {
                if (srv.copyString() == _from) {
                  trx.add(VPackValue("_" + srv.copyString()));
                } else {
                  trx.add(srv);
                }
              }
            }
            // Precondition: Plan still as it was
            pre.add(VPackValue(planPath));
            { VPackObjectBuilder guard(&pre);
              pre.add(VPackValue("old"));
              pre.add(plan);
            }
          });
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addIncreasePlanVersion(trx);
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
  } else if (plan[0].copyString() == "_" + _from) {
    // Retired old leader, let's check that the fromServer has retired:
    size_t done = 0;  // count the number of shards for which leader has retired
    doForAllShards(_snapshot, _database, shardsLikeMe,
      [this, &done](Slice plan, Slice current, std::string& planPath) {
        if (current.length() > 0 && current[0].copyString() == "_" + _from) {
          ++done;
        }
      });

    // Consider timeout:
    if (done < shardsLikeMe.size()) {
      if (considerTimeout()) {
        return FAILED;
      }
      return PENDING;   // do not act!
    }

    // We need to switch leaders:
    { VPackArrayBuilder trxArray(&trx);
      { VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        doForAllShards(_snapshot, _database, shardsLikeMe,
          [this, &trx, &pre](Slice plan, Slice current, std::string& planPath) {
            // Replace "_" + _from by _to and leave _from out:
            trx.add(VPackValue(planPath));
            { VPackArrayBuilder guard(&trx);
              for (auto const& srv : VPackArrayIterator(plan)) {
                if (srv.copyString() == "_" + _from) {
                  trx.add(VPackValue(_to));
                } else if (srv.copyString() != _to) {
                  trx.add(srv);
                }
              }
              // add the old leader as follower in case of a rollback
              trx.add(VPackValue(_from));
            }
            // Precondition: Plan still as it was
            pre.add(VPackValue(planPath));
            { VPackObjectBuilder guard(&pre);
              pre.add(VPackValue("old"));
              pre.add(plan);
            }
          });
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addIncreasePlanVersion(trx);
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
  } else if (plan[0].copyString() == _to) {
    // New leader in Plan, let's check that it has assumed leadership and
    // all but except the old leader are in sync:
    size_t done = 0;
    doForAllShards(_snapshot, _database, shardsLikeMe,
      [this, &done](Slice plan, Slice current, std::string& planPath) {
        if (current.length() > 0 && current[0].copyString() == _to) {
          if (plan.length() < 3) {
            // This only happens for replicationFactor == 1, in which case
            // there are exactly 2 servers in the Plan at this stage.
            // But then we do not have to wait for any follower to get in sync.
            ++done;
          } else {
            // New leader has assumed leadership, now check all but the
            // old leader:
            size_t found = 0;
            for (size_t i = 1; i < plan.length() - 1; ++i) {
              VPackSlice p = plan[i];
              for (auto const& c : VPackArrayIterator(current)) {
                if (arangodb::basics::VelocyPackHelper::compare(p, c, true)) {
                  ++found;
                  break;
                }
              }
            }
            if (found >= plan.length() - 2) {
              ++done;
            }
          }
        }
      });

    // Consider timeout:
    if (done < shardsLikeMe.size()) {
      if (considerTimeout()) {
        return FAILED;
      }
      return PENDING;   // do not act!
    }

    // We need to end the job, Plan remains unchanged:
    { VPackArrayBuilder trxArray(&trx);
      { VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        doForAllShards(_snapshot, _database, shardsLikeMe,
          [&trx, &pre, this](Slice plan, Slice current, std::string& planPath) {
            if (!_remainsFollower) {
              // Remove _from from the list of follower
              trx.add(VPackValue(planPath));
              { VPackArrayBuilder guard(&trx);
                for (auto const& srv : VPackArrayIterator(plan)) {
                  if (!srv.isEqualString(_from)) {
                    trx.add(srv);
                  }
                }
              }
            }

            // Precondition: Plan still as it was
            pre.add(VPackValue(planPath));
            { VPackObjectBuilder guard(&pre);
              pre.add(VPackValue("old"));
              pre.add(plan);
            }
          });
        if (!_remainsFollower) {
          addIncreasePlanVersion(trx);
        }
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addRemoveJobFromSomewhere(trx, "Pending", _jobId);
        Builder job;
        _snapshot.hasAsBuilder(pendingPrefix + _jobId,job);
        addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
        addReleaseShard(trx, _shard);
        addReleaseServer(trx, _to);
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
    finishedAfterTransaction = true;
  } else {
    // something seriously wrong here, fail job:
    finish("", _shard, false, "something seriously wrong");
    return FAILED;
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " + _shard + " from " + _from + " to " + _to;
    return (finishedAfterTransaction ? FINISHED : PENDING);
  }

  LOG_TOPIC(INFO, Logger::SUPERVISION)
    << "Precondition failed for MoveShard job " + _jobId;
  return PENDING;
}

JOB_STATUS MoveShard::pendingFollower() {
  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe
    = clones(_snapshot, _database, _collection, _shard);

  size_t done = 0;   // count the number of shards done
  doForAllShards(_snapshot, _database, shardsLikeMe,
    [&done](Slice plan, Slice current, std::string& planPath) {
      if (ClusterHelpers::compareServerLists(plan, current)) {
        ++done;
      }
    });

  if (done < shardsLikeMe.size()) {
    // Not yet all in sync, consider timeout:
    std::string timeCreatedString
      = _snapshot.hasAsString(pendingPrefix + _jobId + "/timeCreated").first;
    Supervision::TimePoint timeCreated = stringToTimepoint(timeCreatedString);
    Supervision::TimePoint now(std::chrono::system_clock::now());
    if (now - timeCreated > std::chrono::duration<double>(10000.0)) {
      abort();
      return FAILED;
    }
    return PENDING;
  }

  // All in sync, so move on and remove the fromServer, for all shards,
  // and in a single transaction:
  done = 0;   // count the number of shards done
  Builder trx;  // to build the transaction
  Builder precondition;
  { VPackArrayBuilder arrayForTransactionPair(&trx);
    { VPackObjectBuilder transactionObj(&trx);
      VPackObjectBuilder preconditionObj(&precondition);

      // All changes to Plan for all shards, with precondition:
      doForAllShards(_snapshot, _database, shardsLikeMe,
        [this, &trx, &precondition](Slice plan, Slice current,
                                    std::string& planPath) {
          // Remove fromServer from Plan:
          trx.add(VPackValue(planPath));
          { VPackArrayBuilder guard(&trx);
            for (auto const& srv : VPackArrayIterator(plan)) {
              if (srv.copyString() != _from) {
                trx.add(srv);
              }
            }
          }
          // Precondition: Plan still as it was
          precondition.add(VPackValue(planPath));
          { VPackObjectBuilder guard(&precondition);
            precondition.add(VPackValue("old"));
            precondition.add(plan);
          }
        }
      );

      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
      addPreconditionCollectionStillThere(precondition, _database, _collection);
      addReleaseShard(trx, _shard);
      addReleaseServer(trx, _to);

      addIncreasePlanVersion(trx);
    }
    // Add precondition to transaction:
    trx.add(precondition.slice());
  }

  write_ret_t res = singleWriteTransaction(_agent, trx);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return FINISHED;
  }

  return PENDING;
}

arangodb::Result MoveShard::abort() {

  arangodb::Result result;

  // We can assume that the job is either in ToDo or in Pending.
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    result = Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                    "Failed aborting moveShard beyond pending stage");
    return result;
  }

  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", true, "job aborted");
    return result;
  }

  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe
    = clones(_snapshot, _database, _collection, _shard);
  Builder trx;  // to build the transaction

  // Now look after a PENDING job:
  {
    VPackArrayBuilder arrayForTransactionPair(&trx);
    {
      VPackObjectBuilder transactionObj(&trx);
      if (_isLeader) {
        // All changes to Plan for all shards:
        doForAllShards(_snapshot, _database, shardsLikeMe,
          [this, &trx](Slice plan, Slice current, std::string& planPath) {
            // Restore leader to be _from:
            trx.add(VPackValue(planPath));
            { VPackArrayBuilder guard(&trx);
              trx.add(VPackValue(_from));
              VPackArrayIterator iter(plan);
              ++iter;  // skip the first
              while (iter.valid()) {
                trx.add(iter.value());
                ++iter;
              }
            }
          }
        );
      } else {
        // All changes to Plan for all shards:
        doForAllShards(_snapshot, _database, shardsLikeMe,
          [this, &trx](Slice plan, Slice current, std::string& planPath) {
            // Remove toServer from Plan:
            trx.add(VPackValue(planPath));
            { VPackArrayBuilder guard(&trx);
              for (auto const& srv : VPackArrayIterator(plan)) {
                if (srv.copyString() != _to) {
                  trx.add(srv);
                }
              }
            }
          }
        );
      }
      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Failed", job.slice(), "job aborted");
      addReleaseShard(trx, _shard);
      addReleaseServer(trx, _to);
      addIncreasePlanVersion(trx);
    }
  }

  write_ret_t res = singleWriteTransaction(_agent, trx);

  if (!res.accepted) {
    result =  Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                     std::string("Lost leadership"));
    return result;
  } else if(res.indices[0] == 0) {
    result =
      Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
             std::string("Precondition failed while aborting moveShard job ")
             + _jobId);
    return result;
  }

  return result;

}
