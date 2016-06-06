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

#include "Agent.h"
#include "Job.h"

using namespace arangodb::consensus;

MoveShard::MoveShard (Node const& snapshot, Agent* agent,
                      std::string const& jobId, std::string const& creator,
                      std::string const& prefix, std::string const& database,
                      std::string const& collection, std::string const& shard,
                      std::string const& from, std::string const& to) : 
  Job(snapshot, agent, jobId, creator, prefix), _database(database),
  _collection(collection), _shard(shard), _from(from), _to(to) {

  try {

    if (exists()) {

      _database = _snapshot(pendingPrefix + _jobId + "/database").getString();
      _collection =
        _snapshot(pendingPrefix + _jobId + "/collection").getString();
      _from = _snapshot(pendingPrefix + _jobId + "/fromServer").getString();
      _to = _snapshot(pendingPrefix + _jobId + "/toServer").getString();
      _shard = _snapshot(pendingPrefix + _jobId + "/shard").getString();

      if (!status()) {  
        start();        
      }
      
    } else {
      
      create();
      start();
      
    }
    
  } catch (std::exception const& e) {

    LOG(WARN) << "MoveShard: Catastrophic failure: " << e.what();
    finish("Shards/" + _shard, false);

  }

  
}

MoveShard::~MoveShard () {}

bool MoveShard::create () const {
  
  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Todo: Move shard " + _shard + " from " + _from + " to " << _to;

  std::string path, now(timepointToString(std::chrono::system_clock::now()));

  // DBservers
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
    curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Slice current = _snapshot(curPath).slice();
  TRI_ASSERT(current.isArray());
  TRI_ASSERT(current[0].isString());
  
  Builder job;
  job.openArray();
  job.openObject();
  
  if (_from == _to) {
    path = _agencyPrefix + failedPrefix + _jobId;
    job.add("timeFinished", VPackValue(now));
    job.add("result",
            VPackValue("Source and destination of moveShard must be different"));
  } else {
    path = _agencyPrefix + toDoPrefix + _jobId;
  }
  
  job.add(path, VPackValue(VPackValueType::Object));
  job.add("creator", VPackValue(_creator));
  job.add("type", VPackValue("failedLeader"));
  job.add("database", VPackValue(_database));
  job.add("collection", VPackValue(_collection));
  job.add("shard", VPackValue(_shard));
  job.add("fromServer", VPackValue(_from));
  job.add("toServer", VPackValue(_to));
  job.add("isLeader", VPackValue(current[0].copyString() == _from));    
  job.add("jobId", VPackValue(_jobId));
  job.add("timeCreated", VPackValue(now));

  job.close(); job.close(); job.close();
  
  write_ret_t res = transact(_agent, job);

  if (res.accepted && res.indices.size()==1 && res.indices[0]) {
    return true;
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
  return false;

}


bool MoveShard::start() const {

  // DBservers
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
    curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Slice current = _snapshot(curPath).slice();
  
  // Copy todo to pending
  Builder todo, pending;

  // Get todo entry
  todo.openArray();
  _snapshot(toDoPrefix + _jobId).toBuilder(todo);
  todo.close();

  // Enter peding, remove todo, block toserver
  pending.openArray();
    
  // --- Add pending
  pending.openObject();
  pending.add(_agencyPrefix + pendingPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("timeStarted",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
  for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
    pending.add(obj.key.copyString(), obj.value);
  }
  pending.close();
    
  // --- Delete todo
  pending.add(_agencyPrefix + toDoPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("delete"));
  pending.close();
    
  // --- Block shard
  pending.add(_agencyPrefix +  blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();

  // --- Plan changes
  pending.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
  if (current[0].copyString() == _from) { // Leader
    pending.add(VPackValue(std::string("_") + current[0].copyString()));
    pending.add(VPackValue(_to));
    for (size_t i = 1; i < current.length(); ++i) {
      pending.add(current[i]);
    }
  } else { // Follower
    for (auto const& srv : VPackArrayIterator(current)) {
      pending.add(srv);
    }
    pending.add(VPackValue(_to));
  }
  pending.close();
  
  // --- Increment Plan/Version
  pending.add(_agencyPrefix +  planVersion,
              VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("increment"));
  pending.close();
  
  pending.close();
  
  // Preconditions
  // --- Check that Current servers are as we expect
  pending.openObject();
  pending.add(_agencyPrefix + curPath, VPackValue(VPackValueType::Object));
  pending.add("old", current);
  pending.close();
  
  // --- Check if shard is not blocked
  pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("oldEmpty", VPackValue(true));
  pending.close();
  
  pending.close(); pending.close();
    
  // Transact to agency
  write_ret_t res = transact(_agent, pending);
    
  if (res.accepted && res.indices.size()==1 && res.indices[0]) {
    
    LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Move shard " + _shard
      + " from " + _from + " to " + _to;
    return true;
  }    
  
  LOG_TOPIC(INFO, Logger::AGENCY) << "Start precondition failed for " + _jobId;
  return false;

}

unsigned MoveShard::status () const {
  
  Node const& target = _snapshot("/Target");
  if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {
    return TODO;
  } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {

    std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
    std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";
    
    Slice current = _snapshot(curPath).slice(),
      plan = _snapshot(curPath).slice();

    if (current == plan) {

      if ((current[0].copyString())[0] == '_') { // Leader

        Builder cyclic;
        cyclic.openArray();
        cyclic.openObject();
        // --- Plan changes
        cyclic.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
        for (size_t i = 1; i < current.length(); ++i) {
          cyclic.add(current[i]);
        }
        std::string disabledLeader = current[0].copyString();
        disabledLeader = disabledLeader.substr(1,disabledLeader.size()-1);
        cyclic.add(VPackValue(disabledLeader));
        cyclic.close();
        cyclic.close(); cyclic.close();
        transact(_agent, cyclic);
        
        return PENDING;
        
      } else if (current[current.length()-1].copyString()[0] == '_') { // Leader

        Builder remove;
        remove.openArray();
        remove.openObject();
        // --- Plan changes
        remove.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
        for (size_t i = 0; i < current.length()-1; ++i) {
          remove.add(current[i]);
        }
        remove.close();
        remove.close(); remove.close();
        transact(_agent, remove);
        
        return PENDING;
        
      } else {

        bool found = false;
        for (auto const& srv : VPackArrayIterator(current)) {
          if (srv.copyString() == _from) {
            found = true;
            break;
          }
        }

        if (found) {
          
          Builder remove;
          remove.openArray();
          remove.openObject();
          // --- Plan changes
          remove.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
          for (auto const& srv : VPackArrayIterator(current)) {
            if (srv.copyString() != _from) {
              remove.add(srv);
            }
          }
          remove.close();
          remove.close(); remove.close();
          transact(_agent, remove);
          
          return PENDING;
          
        }
        
      }

      if (finish("Shards/" + _shard)) {
        return FINISHED;
      }
      
    }
    
    return PENDING;
  } else if (target.exists(std::string("/Finished/")  + _jobId).size() == 2) {
    return FINISHED;
  } else if (target.exists(std::string("/Failed/")  + _jobId).size() == 2) {
    return FAILED;
  }
  
  return NOTFOUND;
    
}

