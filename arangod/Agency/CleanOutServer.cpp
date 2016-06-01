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

#include "CleanOutServer.h"

#include "Agent.h"
#include "Job.h"

using namespace arangodb::consensus;

CleanOutServer::CleanOutServer (
  Node const& snapshot, Agent* agent, std::string const& jobId,
  std::string const& creator, std::string const& prefix,
  std::string const& server) : 
  Job(snapshot, agent, jobId, creator, prefix), _server(server) {

  if (exists()) {
    if (status() == TODO) {  
      start();        
    } 
  }
  
}

CleanOutServer::~CleanOutServer () {}

unsigned CleanOutServer::status () const {
  return 0;
}

bool CleanOutServer::create () const {
  return false;
}

bool CleanOutServer::start() const {

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
              
            // Only proceed if leader and create job
            if ((*dbsit.begin()).copyString() != _server) {
/*                MoveShardFromLeader (
                  _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                  _jobId, _agencyPrefix, database.first, collptr.first,
                  shard.first, _server, shard.second->slice()[1].copyString());*/
              sub++;
            } else {
/*                MoveShardFromFollower (
                  _snapshot, _agent, _jobId + "-" + std::to_string(sub++),
                  _jobId, _agencyPrefix, database.first, collptr.first,
                  shard.first, _server, shard.second->slice()[1].copyString());*/
              sub++;
            }

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

