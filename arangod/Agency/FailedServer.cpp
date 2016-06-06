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

#include "FailedServer.h"

#include "Agent.h"
#include "FailedLeader.h"
#include "Job.h"

using namespace arangodb::consensus;

FailedServer::FailedServer(Node const& snapshot,
                           Agent* agent,
                           std::string const& jobId,
                           std::string const& creator,
                           std::string const& agencyPrefix,
                           std::string const& server) :
  Job(snapshot, agent, jobId, creator, agencyPrefix),
  _server(server) {
  
  if (_server == "") {
    try {
      _server = _snapshot(toDoPrefix + _jobId + "/server").getString();
    } catch (...) {}
  } 

  if (_server == "") {
    try {
      _server = _snapshot(pendingPrefix + _jobId + "/server").getString();
    } catch (...) {}
  }

  if (_server != "") {
    try {
      if (exists()) {
        if (status() == TODO) {
          start();        
        } 
      } else {            
        create();
        start();
      }
    } catch (...) {
      finish("DBServers/" + _server, false);
    }
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "FailedServer job with id " <<
      jobId << " failed catastrophically. Cannot find server id.";
  }
  
}

FailedServer::~FailedServer () {}

bool FailedServer::start() const {
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Trying to start FailedLeader job" + _jobId + " for server " + _server;

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

  // Prepare peding entry, block toserver
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
      
    LOG_TOPIC(INFO, Logger::AGENCY) <<
      "Pending: DB Server " + _server + " failed.";
      
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
              
            // Only proceed if leader and create job
            if ((*dbsit.begin()).copyString() != _server) {
              continue;
            }

            FailedLeader(
              _snapshot, _agent, _jobId + "-" + std::to_string(sub++), _jobId,
              _agencyPrefix, database.first, collptr.first, shard.first,
              _server, shard.second->slice()[1].copyString());
              
          } 
        }
      }
    }
          
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;

  return false;
    
}

bool FailedServer::create () const {

  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Todo: DB Server " + _server + " failed.";

  std::string path = _agencyPrefix + toDoPrefix + _jobId;

  Builder todo;
  todo.openArray();
  todo.openObject();
  todo.add(path, VPackValue(VPackValueType::Object));
  todo.add("type", VPackValue("failedServer"));
  todo.add("server", VPackValue(_server));
  todo.add("jobId", VPackValue(_jobId));
  todo.add("creator", VPackValue(_creator));
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
  
unsigned FailedServer::status () const {
    
  Node const& target = _snapshot("/Target");
    
  if        (target.exists(std::string("/ToDo/")     + _jobId).size() == 2) {

    return TODO;

  } else if (target.exists(std::string("/Pending/")  + _jobId).size() == 2) {

    Node::Children const subJobs = _snapshot(pendingPrefix).children();

    size_t found = 0;

    for (auto const& subJob : subJobs) {
      if (!subJob.first.compare(0, _jobId.size()+1, _jobId + "-")) {
        found++;
        Node const& sj = *(subJob.second);
        std::string subJobId = sj("jobId").slice().copyString();
        std::string creator  = sj("creator").slice().copyString();
        FailedLeader(_snapshot, _agent, subJobId, creator, _agencyPrefix);
      }
    }

    if (!found) {
      if (finish("DBServers/" + _server)) {
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
  


