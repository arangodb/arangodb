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
      
      if (_shard == "") {
        _shard = _snapshot(pendingPrefix + _jobId + "/shard").getString();
      }
      if (_database == "") {
        _database = _snapshot(pendingPrefix + _jobId + "/database").getString();
      }
      if (_collection == "") {
        _collection =
          _snapshot(pendingPrefix + _jobId + "/collection").getString();
      }
      if (_from == "") {
        _from = _snapshot(pendingPrefix + _jobId + "/fromServer").getString();
      }
      if (_to == "") {
        _to = _snapshot(pendingPrefix + _jobId + "/toServer").getString();
      }

      if (!status()) {  
        start();        
      }
      
    } else {
      
      create();
      start();
      
    }
    
  } catch (...) {
    finish("Shards/" + _shard, false);
  }

  
}

MoveShard::~MoveShard () {}

bool MoveShard::create () const {
  
  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Todo: Move shard " + _shard + " from " + _from + " to " << _to;

  std::string path, now(timepointToString(std::chrono::system_clock::now()));

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
  job.add("isLeader", VPackValue(true));    
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

  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Pending: Move shard " + _shard + " from " + _from + " to " << _to;

  Builder todo, pending;

  // Copy todo to pending
/*  Builder todo, pending;
    
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
    
  // --- Block toServer
  pending.add(_agencyPrefix +  blockedServersPrefix + _server,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();

  // --- Announce in Sync that server is cleaning out
  pending.add(_agencyPrefix + serverStatePrefix + _server,
              VPackValue(VPackValueType::Object));
  pending.add("cleaning", VPackValue(true));
  pending.close();
      
  pending.close();
    
  // Preconditions
  // --- Check that toServer not blocked
  pending.openObject();
  pending.add(_agencyPrefix +  blockedServersPrefix + _server,
              VPackValue(VPackValueType::Object));
  pending.add("oldEmpty", VPackValue(true));
  pending.close();

  pending.close(); pending.close();
    
  // Transact to agency
  write_ret_t res = transact(_agent, pending);
    
  if (res.accepted && res.indices.size()==1 && res.indices[0]) {
      
    LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Clean out server " + _server;
      
    Node::Children const& databases =
      _snapshot("/Plan/Collections").children();
      
    size_t sub = 0;
    
    for (auto const& database : databases) {
      for (auto const& collptr : database.second->children()) {
        Node const& collection = *(collptr.second);
        Node const& replicationFactor = collection("replicationFactor");
        if (replicationFactor.slice().getUInt() > 1) {
          for (auto const& shard : collection("shards").children()) {
            VPackArrayIterator dbsit(shard.second->slice());
            
            MoveShard (
              _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
              _jobId, _agencyPrefix, database.first, collptr.first,
              shard.first, _server, _server);
            
          } 
        }
      }
    }
    
    return true;
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;
*/
  return false;    

}

unsigned MoveShard::status () const {
  return 0;
}

