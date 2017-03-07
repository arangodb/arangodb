////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/Agent.h"
#include "Agency/Job.h"

using namespace arangodb::consensus;

MoveShard::MoveShard(Node const& snapshot, Agent* agent,
                     std::string const& jobId, std::string const& creator,
                     std::string const& database,
                     std::string const& collection, std::string const& shard,
                     std::string const& from, std::string const& to)
    : Job(snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(id(from)),
      _to(id(to)),
      _isLeader(false)   // will be initialized properly when information known
{ }

MoveShard::~MoveShard() {}

void MoveShard::run() {
  try {
    JOB_STATUS js = status();

    if (js == TODO) {
      start();
    } else if (js == NOTFOUND) {
      if (create()) {
        start();
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << ": " << __FILE__ << ":" << __LINE__;
    finish("Shards/" + _shard, false, e.what());
  }
}

bool MoveShard::create(std::shared_ptr<VPackBuilder> b) {

  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Todo: Move shard " + _shard + " from " + _from + " to " << _to;
  
  std::string path;
  std::string now(timepointToString(std::chrono::system_clock::now()));
  
  // DBservers
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  Slice plan = _snapshot(planPath).slice();
  TRI_ASSERT(plan.isArray());
  TRI_ASSERT(plan[0].isString());

  _isLeader = plan[0].copyString() == _from;

  _jb = std::make_shared<Builder>();
  { VPackArrayBuilder guard(_jb.get());
    { VPackObjectBuilder guard2(_jb.get());
      if (_from == _to) {
        path = agencyPrefix + failedPrefix + _jobId;
        _jb->add("timeFinished", VPackValue(now));
        _jb->add(
            "result",
            VPackValue("Source and destination of moveShard must be different"));
      } else {
        path = agencyPrefix + toDoPrefix + _jobId;
      }
      _jb->add("creator", VPackValue(_creator));
      _jb->add("type", VPackValue("moveShard"));
      _jb->add("database", VPackValue(_database));
      _jb->add("collection", VPackValue(_collection));
      _jb->add("shard", VPackValue(_shard));
      _jb->add("fromServer", VPackValue(_from));
      _jb->add("toServer", VPackValue(_to));
      _jb->add("isLeader", VPackValue(_isLeader));
      _jb->add("jobId", VPackValue(_jobId));
      _jb->add("timeCreated", VPackValue(now));
    }
  }

  write_ret_t res = transact(_agent, *_jb);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
  return false;
}

bool MoveShard::start() {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Are we distributeShardsLiking other shard? Then fail miserably.
  Node collection("dummy");
  try {
    collection = _snapshot(planColPrefix + _database + "/" + _collection);
  }
  catch (...) {
    finish("Shards/" + _shard, false,
           "collection has been dropped in the meantime");
    return false;
  }
  std::string distributeShardsLike
      = collection("distributeShardsLike").getString();
  if (!distributeShardsLike.empty()) {
    finish("Shards/" + _shard, false,
           "collection must not have 'distributeShardsLike' attribute");
    return false;
  }
  
  // Check that the shard is not locked:
  auto shardLocks = _snapshot(blockedShardsPrefix).children();
  auto it = shardLocks.find(_shard);
  if (it != shardLocks.end()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "shard " << _shard << " is currently"
      " locked, not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that the toServer is not locked:
  auto serverLocks = _snapshot(blockedServersPrefix).children();
  it = serverLocks.find(_to);
  if (it != serverLocks.end()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "server " << _to << " is currently"
      " locked, not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that _to is not in `Target/CleanedServers`:
  VPackBuilder cleanedServersBuilder;
  try {
    auto cleanedServersNode = _snapshot(cleanedPrefix);
    cleanedServersNode.toBuilder(cleanedServersBuilder);
  }
  catch (...) {
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
        finish("Shards/" + _shard, false,
               "toServer must not be in `Target/CleanedServers`");
        return false;
      }
    }
  }

  // Check that _to is not in `Target/FailedServers`:
  VPackBuilder failedServersBuilder;
  try {
    auto failedServersNode = _snapshot(failedServersPrefix);
    failedServersNode.toBuilder(failedServersBuilder);
  }
  catch (...) {
    // ignore this check
    failedServersBuilder.clear();
    {
      VPackArrayBuilder guard(&failedServersBuilder); 
    }
  }
  VPackSlice failedServers = failedServersBuilder.slice();
  if (failedServers.isArray()) {
    for (auto const& x : VPackArrayIterator(failedServers)) {
      if (x.isString() && x.copyString() == _to) {
        finish("Shards/" + _shard, false,
               "toServer must not be in `Target/FailedServers`");
        return false;
      }
    }
  }

  // DBservers
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
    curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";
  
  Slice current = _snapshot(curPath).slice();
  Slice planned = _snapshot(planPath).slice();
  
  TRI_ASSERT(current.isArray());
  TRI_ASSERT(planned.isArray());
  
  _isLeader = planned[0].copyString() == _from;

  for (auto const& srv : VPackArrayIterator(planned)) {
    TRI_ASSERT(srv.isString());
    if (srv.copyString() == _to) {
      finish("Shards/" + _shard, false,
             "toServer must not be planned for shard already.");
      return false;
    }
  }

  // Compute group to move shards together:
  std::vector<Job::shard_t> shardsLikeMe;
  try {
    shardsLikeMe = clones(_snapshot, _database, _collection, _shard);
  }
  catch (...) {
    shardsLikeMe.clear();
    shardsLikeMe.emplace_back(_collection, _shard);
  }
    
  // Copy todo to pending
  Builder todo, pending;

  // Get todo entry
  { VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      try {
        _snapshot(toDoPrefix + _jobId).toBuilder(todo);
      } catch (std::exception const&) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC(INFO, Logger::AGENCY)
          << "Failed to get key " + toDoPrefix + _jobId + " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(agencyPrefix + toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << ": " << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }
  
  // Enter pending, remove todo, block toserver
  { VPackArrayBuilder listOfTransactions(&pending);

    { VPackObjectBuilder objectForMutation(&pending);

      // --- Add pending
      pending.add(VPackValue(agencyPrefix + pendingPrefix + _jobId));
      { VPackObjectBuilder jobObjectInPending(&pending);
        pending.add("timeStarted",
                    VPackValue(timepointToString(std::chrono::system_clock::now())));
        for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
          pending.add(obj.key);
          pending.add(obj.value);
        }
      }   // job object complete

      // --- Delete todo
      pending.add(VPackValue(agencyPrefix + toDoPrefix + _jobId));
      { VPackObjectBuilder action(&pending);
        pending.add("op", VPackValue("delete"));
      }

      // --- Block shards
      for (auto const& pair : shardsLikeMe) {
        pending.add(VPackValue(agencyPrefix + blockedShardsPrefix + pair.shard));
        { VPackObjectBuilder guard(&pending);
          pending.add("jobId", VPackValue(_jobId));
        }
      }
    
      // --- Plan changes
      for (auto const& pair : shardsLikeMe) {
        
        planPath = planColPrefix + _database + "/" + pair.collection
          + "/shards/" + pair.shard;
        planned = _snapshot(planPath).slice();
      
        pending.add(VPackValue(agencyPrefix + planPath));
        { VPackArrayBuilder serverList(&pending);
          if (_isLeader) {  // Leader
            pending.add(planned[0]);
            pending.add(VPackValue(_to));
            for (size_t i = 1; i < planned.length(); ++i) {
              pending.add(planned[i]);
              TRI_ASSERT(planned[i].copyString() != _to);
            }
          } else {  // Follower
            for (auto const& srv : VPackArrayIterator(planned)) {
              pending.add(srv);
              TRI_ASSERT(srv.copyString() != _to);
            }
            pending.add(VPackValue(_to));
          }
        }
      }

      // --- Increment Plan/Version
      pending.add(VPackValue(agencyPrefix + planVersion));
      { VPackObjectBuilder guard(&pending);
        pending.add("op", VPackValue("increment"));
      }

    }  // mutation part of transaction done

    // Preconditions
    { VPackObjectBuilder precondition(&pending);

      // --- Check that Planned servers are still as we expect
      { VPackObjectBuilder planCheck(&pending);
        pending.add(VPackValue(agencyPrefix + planPath));
        { VPackObjectBuilder old(&pending);
          pending.add("old", planned);
        }
      }

      // --- Check that shard is not blocked
      pending.add(VPackValue(agencyPrefix + blockedShardsPrefix + _shard));
      { VPackObjectBuilder shardLockEmpty(&pending);
        pending.add("oldEmpty", VPackValue(true));
      }

      // --- Check that toServer is not blocked
      pending.add(VPackValue(agencyPrefix + blockedServersPrefix + _to));
      { VPackObjectBuilder shardLockEmpty(&pending);
        pending.add("oldEmpty", VPackValue(true));
      }

      // --- Check that FailedServers is still as before
      pending.add(VPackValue(agencyPrefix + failedServersPrefix));
      { VPackObjectBuilder failedServersCheck(&pending);
        pending.add("old", failedServers);
      }

      // --- Check that CleanedServers is still as before
      pending.add(VPackValue(agencyPrefix + cleanedPrefix));
      { VPackObjectBuilder cleanedServersCheck(&pending);
        pending.add("old", cleanedServers);
      }
    }   // precondition done

  }  // array for transaction done

  // Transact to agency
  write_ret_t res = transact(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Pending: Move shard " + _shard + " from " + _from + " to " + _to;
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Start precondition failed for MoveShard job " + _jobId;
  return false;
}

JOB_STATUS MoveShard::status() {
  auto status = exists();

  if (status == NOTFOUND) {
    return status;
  }

  // Get job details from agency
  try {
    _database = _snapshot(pos[status] + _jobId + "/database").getString();
    _collection =
      _snapshot(pos[status] + _jobId + "/collection").slice().copyString();
    _from = _snapshot(pos[status] + _jobId + "/fromServer").getString();
    _to = _snapshot(pos[status] + _jobId + "/toServer").getString();
    _shard = _snapshot(pos[status] + _jobId + "/shard").slice().copyString();
    _isLeader = _snapshot(pos[status] + _jobId + "/isLeader").slice().isTrue();
  } catch (std::exception const& e) {
    std::string err = 
      std::string("Failed to find job ") + _jobId + " in agency: " + e.what();
    LOG_TOPIC(ERR, Logger::AGENCY) << err;
    finish("Shards/" + _shard, false, err);
    return FAILED;
  }

  if (status != PENDING) {
    return status;
  }

  // check that shard still there, otherwise finish job
  std::string planPath = planColPrefix + _database + "/" + _collection;
  try {
    auto coll = _snapshot(planPath);
  } catch (std::exception const& e) {
    // Oops, collection is gone, simple finish job:
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << ": " << __FILE__ << ":" 
        << __LINE__;
    finish("Shards/" + _shard, true, "collection was dropped");
    return FINISHED;
  }
 
  if (_isLeader) {
    return pendingLeader();
  } else {
    return pendingFollower();
  }
}

JOB_STATUS MoveShard::pendingLeader() {
  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe;
  try {
    shardsLikeMe = clones(_snapshot, _database, _collection, _shard);
  }
  catch (...) {
    shardsLikeMe.clear();
    shardsLikeMe.emplace_back(_collection, _shard);
  }

  size_t done = 0;   // count the number of shards done
  for (auto const& collShard : shardsLikeMe) {
    std::string shard = collShard.shard;
    std::string collection = collShard.collection;

    std::string planPath =
      planColPrefix + _database + "/" + collection + "/shards/" + shard;
    std::string curPath = curColPrefix + _database + "/" + collection + "/" +
                        shard + "/servers";

    Slice current = _snapshot(curPath).slice();
    Slice plan = _snapshot(planPath).slice();

    if (compareServerLists(plan, current)) {
      // The case that current and plan are equivalent:

      if (current[0].copyString() == std::string("_") + _from) {
        // Retired leader
        
        Builder remove;  // remove
        remove.openArray();
        remove.openObject();
        // --- Plan changes
        remove.add(agencyPrefix + planPath, VPackValue(VPackValueType::Array));
        for (size_t i = 1; i < plan.length(); ++i) {
          remove.add(plan[i]);
        }
        remove.close();
        // --- Plan version
        remove.add(agencyPrefix + planVersion,
                   VPackValue(VPackValueType::Object));
        remove.add("op", VPackValue("increment"));
        remove.close();
        remove.close();
        remove.close();
        transact(_agent, remove);
        
      } else {
        bool foundFrom = false, foundTo = false;
        for (auto const& srv : VPackArrayIterator(current)) {
          std::string srv_str = srv.copyString();
          if (srv_str == _from) {
            foundFrom = true;
          }
          if (srv_str == _to) {
            foundTo = true;
          }
        }
        
        if (foundFrom && foundTo) {
          if (plan[0].copyString() == _from) {  // Leader
            
            Builder underscore;  // serverId -> _serverId
            underscore.openArray();
            underscore.openObject();
            // --- Plan changes
            underscore.add(agencyPrefix + planPath,
                           VPackValue(VPackValueType::Array));
            underscore.add(VPackValue(std::string("_") + plan[0].copyString()));
            for (size_t i = 1; i < plan.length(); ++i) {
              underscore.add(plan[i]);
            }
            underscore.close();
            
            // --- Plan version
            underscore.add(agencyPrefix + planVersion,
                           VPackValue(VPackValueType::Object));
            underscore.add("op", VPackValue("increment"));
            underscore.close();
            underscore.close();
            underscore.close();
            transact(_agent, underscore);
            
          } else {
            Builder remove;
            remove.openArray();
            remove.openObject();
            // --- Plan changes
            remove.add(agencyPrefix + planPath,
                       VPackValue(VPackValueType::Array));
            for (auto const& srv : VPackArrayIterator(plan)) {
              if (srv.copyString() != _from) {
                remove.add(srv);
              }
            }
            remove.close();
            // --- Plan version
            remove.add(agencyPrefix + planVersion,
                       VPackValue(VPackValueType::Object));
            remove.add("op", VPackValue("increment"));
            remove.close();
            remove.close();
            remove.close();
            transact(_agent, remove);
          }
          
        } else if (foundTo && !foundFrom) {
          done++;
        }
      }
    } else {

    }
    // FIXME: handle timeout if new server does not get in sync
    // FIXME: check again in detail that this implements the two different
    // FIXME: moveShard jobs in the design document, too many ifs here
    // FIXME: handle timeout if old leader does not retire
  }

  if (done == shardsLikeMe.size()) {
    if (finish("Shards/" + _shard)) {
      return FINISHED;
    }
  }
 
  return PENDING;
}

JOB_STATUS MoveShard::pendingFollower() {
  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe;
  try {
    shardsLikeMe = clones(_snapshot, _database, _collection, _shard);
  }
  catch (...) {
    shardsLikeMe.clear();
    shardsLikeMe.emplace_back(_collection, _shard);
  }

  size_t done = 0;   // count the number of shards done
  for (auto const& collShard : shardsLikeMe) {
    std::string shard = collShard.shard;
    std::string collection = collShard.collection;

    std::string planPath =
      planColPrefix + _database + "/" + collection + "/shards/" + shard;
    std::string curPath = curColPrefix + _database + "/" + collection + "/" +
                        shard + "/servers";

    Slice current = _snapshot(curPath).slice();
    Slice plan = _snapshot(planPath).slice();

    if (compareServerLists(plan, current)) {
      ++done;
    }
  }

  if (done < shardsLikeMe.size()) {
    // Not yet all in sync, consider timeout:
    // TODO: ...
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
          trx.add(VPackValue(agencyPrefix + planPath));
          { VPackArrayBuilder guard(&trx);
            for (auto const& srv : VPackArrayIterator(plan)) {
              if (srv.copyString() != _from) {
                trx.add(srv);
              }
            }
          }
          // Precondition: Plan still as it was
          precondition.add(VPackValue(agencyPrefix + planPath));
          { VPackObjectBuilder guard(&precondition);
            precondition.add(VPackValue("old"));
            precondition.add(plan);
          }
        }
      );

      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      _snapshot(pendingPrefix + _jobId).toBuilder(job);
      addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
      addPreconditionCollectionStillThere(precondition, _database, _collection);

      addIncreasePlanVersion(trx);
    }
    // Add precondition to transaction:
    trx.add(precondition.slice());
  }

  write_ret_t res = transact(_agent, trx);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return FINISHED;
  }

  return PENDING;
}

void MoveShard::abort() {
  // TO BE IMPLEMENTED
}

