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
#include "MoveShard.h"

using namespace arangodb::consensus;

CleanOutServer::CleanOutServer (
  Node const& snapshot, Agent* agent, std::string const& jobId,
  std::string const& creator, std::string const& prefix,
  std::string const& server) : 
  Job(snapshot, agent, jobId, creator, prefix), _server(server) {

  if (exists()) {
    if (_server == "") {
      _server = _snapshot(pendingPrefix + _jobId + "/server").getString();
    }
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

    // Check if we can get things done in the first place
    if (!checkFeasibility()) {
      finish("DBServers/" + _server);
      return false;
    }

    // Schedule shard relocations
    scheduleMoveShards();
    
    return true;
    
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;

  return false;
    
}

bool CleanOutServer::scheduleMoveShards() const {
  return true;
}

bool CleanOutServer::checkFeasibility () const {

  // Check if server is already in cleaned servers: fail!
  Node::Children const& cleanedServers =
    _snapshot("/Target/CleanedServers").children();
  for (auto const cleaned : cleanedServers) {
    if (cleaned.first == _server) {
      LOG_TOPIC(ERR, Logger::AGENCY) << _server <<
        " has been cleaned out already!";
      return false;
    }
  }

  // Determine number of available servers
  Node::Children const& dbservers = _snapshot("/Plan/DBServers").children();
  uint64_t nservers = dbservers.size() - cleanedServers.size() - 1;

  // See if available servers after cleanout satisfy all replication factors
  Node::Children const& databases = _snapshot("/Plan/Collections").children();
  for (auto const& database : databases) {
    for (auto const& collptr : database.second->children()) {
      try {
        uint64_t replFactor = (*collptr.second)("replicationFactor").getUInt();
        if (replFactor > nservers) {
          LOG_TOPIC(ERR, Logger::AGENCY) <<
            "Cannot house all shard replics after cleaning out " << _server;
          return false;
        }
      } catch (...) {}
    }
  }

  return true;
  
}
