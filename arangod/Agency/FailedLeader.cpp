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

#include "FailedLeader.h"

#include "Agent.h"
#include "Job.h"

using namespace arangodb::consensus;

FailedLeader::FailedLeader(
  Node const& snapshot, Agent* agent, std::string const& jobId,
  std::string const& creator, std::string const& agencyPrefix,
  std::string const& database, std::string const& collection,
  std::string const& shard, std::string const& from, std::string const& to) :
  Job(snapshot, agent, jobId, creator, agencyPrefix), _database(database),
  _collection(collection), _shard(shard), _from(from), _to(to) {
  
  try {
    if (exists()) {
      if (!status()) {  
        start();        
      } 
    } else {            
      create();
      start();
    }
  } catch (...) {
    if (_shard == "") {
      _shard = _snapshot(pendingPrefix + _jobId + "/shard").getString();
    }
    finish("Shards/" + _shard, false);
  }
  
}

FailedLeader::~FailedLeader() {}

bool FailedLeader::create () const {

  LOG_TOPIC(INFO, Logger::AGENCY) << "Todo: change leadership for " + _shard
    + " from " + _from + " to " + _to;
  
  std::string path = _agencyPrefix + toDoPrefix + _jobId;
  
  Builder todo;
  todo.openArray();
  todo.openObject();
  todo.add(path, VPackValue(VPackValueType::Object));
  todo.add("creator", VPackValue(_creator));
  todo.add("type", VPackValue("failedLeader"));
  todo.add("database", VPackValue(_database));
  todo.add("collection", VPackValue(_collection));
  todo.add("shard", VPackValue(_shard));
  todo.add("fromServer", VPackValue(_from));
  todo.add("toServer", VPackValue(_to));
  todo.add("isLeader", VPackValue(true));    
  todo.add("jobId", VPackValue(_jobId));
  todo.add("timeCreated",
           VPackValue(timepointToString(std::chrono::system_clock::now())));
  todo.close(); todo.close(); todo.close();
  
  write_ret_t res = transact(_agent, todo);
  
  if (res.accepted && res.indices.size()==1 && res.indices[0]) {
    return true;
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
  return false;
  
}

bool FailedLeader::start() const {
  
  // DBservers
  std::string planPath =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
    curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Node const& current = _snapshot(curPath);
  
  if (current.slice().length() == 1) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to change leadership for shard " + _shard  + " from " + _from
      + " to " + _to + ". No in-sync followers:" + current.slice().toJson();
    return false;
  }
  
  // Copy todo to pending
  Builder todo, pending;
  
  // Get todo entry
  todo.openArray();
  try {
    _snapshot(toDoPrefix + _jobId).toBuilder(todo);
  } catch (std::exception const&) {
    LOG_TOPIC(INFO, Logger::AGENCY) <<
      "Failed to get key " + toDoPrefix + _jobId + " from agency snapshot";
    return false;
  }
  todo.close();
  
  // Transaction
  pending.openArray();
  
  // Apply
  // --- Add pending entry
  pending.openObject();
  pending.add(_agencyPrefix + pendingPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("timeStarted",
              VPackValue(timepointToString(std::chrono::system_clock::now())));
  for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
    pending.add(obj.key.copyString(), obj.value);
  }
  pending.close();
  
  // --- Remove todo entry
  pending.add(_agencyPrefix + toDoPrefix + _jobId,
                VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("delete"));
  pending.close();
  
  // --- Cyclic shift in sync servers
  pending.add(_agencyPrefix + planPath, VPackValue(VPackValueType::Array));
  for (size_t i = 1; i < current.slice().length(); ++i) {
    pending.add(current.slice()[i]);
  }
  pending.add(current.slice()[0]);
  pending.close();
  
  // --- Block shard
  pending.add(_agencyPrefix +  blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();
  
  // --- Increment Plan/Version
  pending.add(_agencyPrefix +  planVersion,
              VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("increment"));
  pending.close();
  
  pending.close();
  
  // Precondition
  // --- Check that Current servers are as we expect
  pending.openObject();
  pending.add(_agencyPrefix + curPath, VPackValue(VPackValueType::Object));
  pending.add("old", current.slice());
  pending.close();
  
  // --- Check if shard is not blocked
  pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("oldEmpty", VPackValue(true));
  pending.close();
  
  pending.close(); pending.close();
  
  // Transact 
  write_ret_t res = transact(_agent, pending);
  
  if (res.accepted && res.indices.size()==1 && res.indices[0]) {
    
    LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Change leadership " + _shard
      + " from " + _from + " to " + _to;
    return true;
  }    
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;
  return false;
  
}


unsigned FailedLeader::status () const {
  
  Node const& target = _snapshot("/Target");
  
  if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {
    
    return TODO;
    
  } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {

    Node const& job = _snapshot(pendingPrefix + _jobId);
    std::string database = job("database").toJson(),
      collection = job("collection").toJson(),
      shard = job("shard").toJson();
    
    std::string planPath = planColPrefix + database + "/" + collection
      + "/shards/" + shard,
      curPath = curColPrefix + database + "/" + collection + "/" + shard
      + "/servers";
    
    Node const& planned = _snapshot(planPath);
    Node const& current = _snapshot(curPath);

    if (planned.slice()[0] == current.slice()[0]) {

      if (finish("Shards/" + shard)) {
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


