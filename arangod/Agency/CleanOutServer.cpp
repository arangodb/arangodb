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
    LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
    finish("DBServers/" + _server, false, e.what());
  }

}

CleanOutServer::~CleanOutServer () {}

JOB_STATUS CleanOutServer::status () {

  auto status = exists();

  if (status != NOTFOUND) { // Get job details from agency

    try {
      _server = _snapshot(pos[status] + _jobId + "/server").getString();
    } catch (std::exception const& e) {
      std::stringstream err;
      err << "Failed to find job " << _jobId << " in agency: " << e.what();
      LOG_TOPIC(ERR, Logger::AGENCY) << err.str();
      finish("DBServers/" + _server, false, err.str());
      return FAILED;
    }
    
  }

  if (status == PENDING) {
  
    Node::Children const todos = _snapshot(toDoPrefix).children();
    Node::Children const pends = _snapshot(pendingPrefix).children();
    size_t found = 0;

    for (auto const& subJob : todos) {
      if (!subJob.first.compare(0, _jobId.size()+1, _jobId + "-")) {
        found++;
      }
    }
    for (auto const& subJob : pends) {
      if (!subJob.first.compare(0, _jobId.size()+1, _jobId + "-")) {
        found++;
      }
    }

    if (found == 0) {
      // Put server in /Target/CleanedServers:
      Builder reportTrx;
      {
        VPackArrayBuilder guard(&reportTrx);
        {
          VPackObjectBuilder guard3(&reportTrx);
          reportTrx.add(VPackValue(_agencyPrefix + "/Target/CleanedServers"));
          {
            VPackObjectBuilder guard4(&reportTrx);
            reportTrx.add("op", VPackValue("push"));
            reportTrx.add("new", VPackValue(_server));
          }
        }
      }
      // Transact to agency
      write_ret_t res = transact(_agent, reportTrx);
      
      if (res.accepted && res.indices.size() == 1 && res.indices[0] != 0) {
        LOG_TOPIC(INFO, Logger::AGENCY) << "Have reported " << _server 
          << " in /Target/CleanedServers";
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to report " << _server
          << " in /Target/CleanedServers";
      }
    
      if (finish("DBServers/" + _server)) {
        return FINISHED;
      }
    }

  }
  
  return status;

}

bool CleanOutServer::create () {
  return false;
}

bool CleanOutServer::start() {

  // Copy todo to pending
  Builder todo, pending;
    
  // Get todo entry
  todo.openArray();
  _snapshot(toDoPrefix + _jobId).toBuilder(todo);
  todo.close();

  // Enter pending, remove todo, block toserver
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
      finish("DBServers/" + _server, false);
      LOG(WARN) << __FILE__<<__LINE__ ;
      return false;
    }

    // Schedule shard relocations
    if (!scheduleMoveShards()) {
      finish("DBServers/" + _server, false, "Could not schedule MoveShard.");
      return false;
    }
    
    return true;
    
  }
  
  LOG_TOPIC(INFO, Logger::AGENCY) <<
    "Precondition failed for starting job " + _jobId;

  return false;
    
}

bool CleanOutServer::scheduleMoveShards() {

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
  size_t sub = 0;
  
  for (auto const& database : databases) {
    for (auto const& collptr : database.second->children()) {
      Node const& collection = *(collptr.second);
      for (auto const& shard : collection("shards").children()) {
        bool found = false;
        VPackArrayIterator dbsit(shard.second->slice());

        // Only shards, which are affected 
        for (auto const& dbserver : dbsit) {
          if (dbserver.copyString() == _server) {
            found = true;
            break;
          }
        }
        if (!found) {
          continue;
        }

        // Only destinations, which are not already holding this shard
        std::vector<std::string> myServers = availServers;
        for (auto const& dbserver : dbsit) {
          myServers.erase(
            std::remove(myServers.begin(), myServers.end(),
                        dbserver.copyString()), myServers.end());
        }

        // Among those a random destination
        std::string toServer;
        if (myServers.empty()) {
          LOG_TOPIC(ERR, Logger::AGENCY) << "No servers remain as target for "
              << "MoveShard";
          return false;
        }

        try {
          toServer = myServers.at(rand() % myServers.size());
        } catch (...) {
          LOG_TOPIC(ERR, Logger::AGENCY) <<
            "Range error picking destination for shard " + shard.first;
        }

        // Schedule move
        MoveShard (
          _snapshot, _agent, _jobId + "-" + std::to_string(sub++), _jobId,
          _agencyPrefix, database.first, collptr.first, shard.first, _server,
          toServer);
        
      }
    }
  }
  
  return true;
}

bool CleanOutServer::checkFeasibility () {

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
