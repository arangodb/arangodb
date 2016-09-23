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

#include "UnassumedLeadership.h"

#include "Agent.h"
#include "Job.h"

using namespace arangodb::consensus;

UnassumedLeadership::UnassumedLeadership(
    Node const& snapshot, Agent* agent, std::string const& jobId,
    std::string const& creator, std::string const& agencyPrefix,
    std::string const& database, std::string const& collection,
    std::string const& shard, std::string const& server)
    : Job(snapshot, agent, jobId, creator, agencyPrefix),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(server) {
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
    finish("Shards/" + _shard, false, e.what());
  }
}

UnassumedLeadership::~UnassumedLeadership() {}

bool UnassumedLeadership::create() {
  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Todo: Find new leader for to be created shard " << _shard;

  std::string path = _agencyPrefix + toDoPrefix + _jobId;

  _jb = std::make_shared<Builder>();
  _jb->openArray();
  _jb->openObject();
  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("creator", VPackValue(_creator));
  _jb->add("type", VPackValue("unassumedLeadership"));
  _jb->add("database", VPackValue(_database));
  _jb->add("collection", VPackValue(_collection));
  _jb->add("shard", VPackValue(_shard));
  _jb->add("jobId", VPackValue(_jobId));
  _jb->add("timeCreated",
           VPackValue(timepointToString(std::chrono::system_clock::now())));
  _jb->close();
  _jb->close();
  _jb->close();

  write_ret_t res = transact(_agent, *_jb);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to insert job " + _jobId;
  return false;
}

bool UnassumedLeadership::start() {
  std::string planPath = planColPrefix + _database + "/" + _collection;
  std::string curPath = curColPrefix + _database + "/" + _collection;

  Node const& current = _snapshot(curPath);

  // Copy todo to pending
  Builder todo, pending;

  // Find target server
  if (!reassignShard()) {
    return false;
  }

  // Get todo entry
  todo.openArray();
  if (_jb == nullptr) {
    try {
      _snapshot(toDoPrefix + _jobId).toBuilder(todo);
    } catch (std::exception const&) {
      LOG_TOPIC(INFO, Logger::AGENCY) << "Failed to get key " + toDoPrefix +
                                             _jobId + " from agency snapshot";
      return false;
    }
  } else {
    todo.add(_jb->slice()[0].valueAt(0));
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
  pending.add("fromServer", VPackValue(_from));
  pending.add("toServer", VPackValue(_to));
  for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
    pending.add(obj.key.copyString(), obj.value);
  }
  pending.close();

  // --- Remove todo entry
  pending.add(_agencyPrefix + toDoPrefix + _jobId,
              VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("delete"));
  pending.close();

  // --- Increment Plan/Version
  pending.add(_agencyPrefix + planVersion, VPackValue(VPackValueType::Object));
  pending.add("op", VPackValue("increment"));
  pending.close();

  // --- Reassign DBServer
  std::string path = planPath + "/shards/" + _shard;
  pending.add(_agencyPrefix + path, VPackValue(VPackValueType::Array));
  for (const auto& dbserver : VPackArrayIterator(_snapshot(path).slice())) {
    if (dbserver.copyString() == _from) {
      pending.add(VPackValue(_to));
    } else {
      pending.add(dbserver);
    }
  }
  pending.close();
  pending.close();

  // Precondition
  // --- Check that Current servers are as we expect
  pending.openObject();
  pending.add(_agencyPrefix + curPath, VPackValue(VPackValueType::Object));
  pending.add("old", current.slice());
  pending.close();

  pending.close();
  pending.close();

  // Transact
  write_ret_t res = transact(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(INFO, Logger::AGENCY)
        << "Pending: Reassign creating leader for " + _shard + " from " +
               _from + " to " + _to;
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Precondition failed for starting job " + _jobId;

  return false;
}

bool UnassumedLeadership::reassignShard() {
  std::vector<std::string> availServers;

  // Get servers from plan
  Node::Children const& dbservers = _snapshot("/Plan/DBServers").children();
  for (auto const& srv : dbservers) {
    availServers.push_back(srv.first);
  }
  
  // Remove this server
  availServers.erase(
    std::remove(availServers.begin(), availServers.end(), _from),
    availServers.end());
  
  // Remove cleaned from list
  for (auto const& srv :
         VPackArrayIterator(_snapshot("/Target/CleanedServers").slice())) {
    availServers.erase(
      std::remove(availServers.begin(), availServers.end(), srv.copyString()),
      availServers.end());
  }
  
  // Remove failed from list
  for (auto const& srv :
         VPackObjectIterator(_snapshot("/Target/FailedServers").slice())) {
    availServers.erase(
      std::remove(
        availServers.begin(), availServers.end(), srv.key.copyString()),
      availServers.end());
  }
  
// Only destinations, which are not already holding this shard
  std::string path =
    planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto const& plannedservers = _snapshot(path);
  for (auto const& dbserver : VPackArrayIterator(plannedservers.slice())) {
    availServers.erase(
      std::remove(
        availServers.begin(), availServers.end(), dbserver.copyString()),
      availServers.end());
  }
  
  // Minimum 1 DB server must remain
  if (availServers.empty()) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "No DB servers left as target.";
    return false;
  }

  // Among those a random destination
  try {
    _to = availServers.at(rand() % availServers.size());
    LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Will reassign creation of " + _shard + " to " + _to;
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCY)
        << "Range error picking destination for shard " + _shard;
    return false;
  }

  return true;
}

JOB_STATUS UnassumedLeadership::status() {
  auto status = exists();

  if (status != NOTFOUND) {  // Get job details from agency

    try {
      _database = _snapshot(pos[status] + _jobId + "/database").getString();
      _collection = _snapshot(pos[status] + _jobId + "/collection").getString();
      _from = _snapshot(pos[status] + _jobId + "/fromServer").getString();
      _to = _snapshot(pos[status] + _jobId + "/toServer").getString();
      _shard = _snapshot(pos[status] + _jobId + "/shard").getString();
    } catch (std::exception const& e) {
      std::stringstream err;
      err << "Failed to find job " << _jobId << " in agency: " << e.what();
      LOG_TOPIC(ERR, Logger::AGENCY) << err.str();
      finish("Shards/" + _shard, false, err.str());
      return FAILED;
    }
  }

  if (status == PENDING) {
    Node const& job = _snapshot(pendingPrefix + _jobId);
    std::string database = job("database").toJson(),
                collection = job("collection").toJson(),
                shard = job("shard").toJson();

    std::string planPath = planColPrefix + database + "/" + collection +
                           "/shards/" + shard,
                curPath = curColPrefix + database + "/" + collection + "/" +
                          shard + "/servers";

    Node const& planned = _snapshot(planPath);
    Node const& current = _snapshot(curPath);

    if (planned.slice()[0] == current.slice()[0]) {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Done reassigned creation of " + _shard + " to " + _to;
      if (finish("Shards/" + shard)) {
        return FINISHED;
      }
    }
  }

  return status;
}
