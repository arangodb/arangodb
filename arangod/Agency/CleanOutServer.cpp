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
    if (exists()) {
      if (status() == TODO) {  
        start();        
      } 
    }
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "CleanOutServer job with id " <<
      jobId << " failed catastrophically. Cannot find server id.";
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
  LOG(WARN) << __FILE__<<__LINE__ ;

    // Check if we can get things done in the first place
    if (!checkFeasibility()) {
      finish("DBServers/" + _server, false);
      LOG(WARN) << __FILE__<<__LINE__ ;
      return false;
    }

      LOG(WARN) << __FILE__<<__LINE__ ;


    // Schedule shard relocations
    scheduleMoveShards();
  LOG(WARN) << __FILE__<<__LINE__ ;
    
    return true;
    
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;

  return false;
    
}

bool CleanOutServer::scheduleMoveShards() const {

  Node::Children const& dbservers = _snapshot("/Plan/DBServers").children();

  std::vector<std::string> availServers;
  for (auto const server : dbservers) {
    if (_snapshot.exists(
          serverStatePrefix + server.first + "/cleaning").size() < 4 &&
        _snapshot.exists(cleanedPrefix + server.first).size() < 3 &&
        _server != server.first) {
      availServers.push_back(server.first);
    }
  }
  
  Node::Children const& databases = _snapshot("/Plan/Collections").children();
  size_t count = 0;
  
  for (auto const& database : databases) {
    for (auto const& collptr : database.second->children()) {
      Node const& collection = *(collptr.second);
      for (auto const& shard : collection("shards").children()) {
        VPackArrayIterator dbsit(shard.second->slice());
        for (auto const& dbserver : dbsit) {
          if (dbserver.copyString() == _server) {
            MoveShard (_snapshot, _agent, _jobId, _creator, database.first,
                       collptr.first, shard.first, _server,
                       availServers.at(count%availServers.size()));
            count++;
          }
        }
      }
    }
  }  
  
  return true;
}

bool CleanOutServer::checkFeasibility () const {

  uint64_t numCleaned = 0;
  // Check if server is already in cleaned servers: fail!
  if (_snapshot.exists("/Target/CleanedServers").size()==2) {
    Node::Children const& cleanedServers =
      _snapshot("/Target/CleanedServers").children();
    for (auto const cleaned : cleanedServers) {
      if (cleaned.first == _server) {
        LOG_TOPIC(ERR, Logger::AGENCY) << _server
                                       << " has been cleaned out already!";
        return false;
      }
    }
    numCleaned = cleanedServers.size();
  }

  // Determine number of available servers
  Node::Children const& dbservers = _snapshot("/Plan/DBServers").children();
  uint64_t nservers = dbservers.size() - numCleaned - 1,
    maxReplFact = 1;

  std::vector<std::string> tooLargeCollections;
  std::vector<uint64_t> tooLargeFactors;

  // Find conflictings collections
  Node::Children const& databases = _snapshot("/Plan/Collections").children();
  for (auto const& database : databases) {
    for (auto const& collptr : database.second->children()) {
      try {
        uint64_t replFact = (*collptr.second)("replicationFactor").getUInt();
        if (replFact > nservers) {
          tooLargeCollections.push_back(collptr.first);
          tooLargeFactors.push_back(replFact);
        }
        if (replFact > maxReplFact) {
          maxReplFact = replFact;
        }
      } catch (...) {}
    }
  }

  // Report problem
  if (maxReplFact > nservers) {
    std::stringstream collections;
    std::stringstream factors;

    for (auto const collection : tooLargeCollections) {
      collections << collection << " ";
    }
    for (auto const factor : tooLargeFactors) {
      factors << std::to_string(factor) << " ";
    }
    
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Cannot accomodate shards " << collections.str()
      << "with replication factors " << factors.str()
      << "after cleaning out server " << _server;
    return false;
  }

  return true;
  
}
