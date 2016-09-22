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

FailedLeader::FailedLeader(Node const& snapshot, Agent* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& agencyPrefix,
                           std::string const& database,
                           std::string const& collection,
                           std::string const& shard, std::string const& from,
                           std::string const& to)
    : Job(snapshot, agent, jobId, creator, agencyPrefix),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(from),
      _to(to) {
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

FailedLeader::~FailedLeader() {}

bool FailedLeader::create() {
  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Todo: failed Leader for " + _shard + " from " + _from + " to " + _to;

  std::string path = _agencyPrefix + toDoPrefix + _jobId;

  _jb = std::make_shared<Builder>();
  _jb->openArray();
  _jb->openObject();

  // Todo entry
  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("creator", VPackValue(_creator));
  _jb->add("type", VPackValue("failedLeader"));
  _jb->add("database", VPackValue(_database));
  _jb->add("collection", VPackValue(_collection));
  _jb->add("shard", VPackValue(_shard));
  _jb->add("fromServer", VPackValue(_from));
  _jb->add("toServer", VPackValue(_to));
  _jb->add("isLeader", VPackValue(true));
  _jb->add("jobId", VPackValue(_jobId));
  _jb->add("timeCreated",
           VPackValue(timepointToString(std::chrono::system_clock::now())));
  _jb->close();

  // Add shard to /arango/Target/FailedServers/<server> array
  path = _agencyPrefix + failedServersPrefix + "/" + _from;
  _jb->add(path, VPackValue(VPackValueType::Object));
  _jb->add("op", VPackValue("push"));
  _jb->add("new", VPackValue(_shard));
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

bool FailedLeader::start() {
  // DBservers
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  Node const& current = _snapshot(curPath);

  if (current.slice().length() == 1) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to change leadership for shard " +
                                          _shard + " from " + _from + " to " +
                                          _to + ". No in-sync followers:" +
                                          current.slice().toJson();
    return false;
  }

  // Copy todo to pending
  Builder todo, pending;

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
    todo.add(_jb->slice().get(_agencyPrefix + toDoPrefix + _jobId).valueAt(0));
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
  pending.add(_agencyPrefix + blockedShardsPrefix + _shard,
              VPackValue(VPackValueType::Object));
  pending.add("jobId", VPackValue(_jobId));
  pending.close();

  // --- Increment Plan/Version
  pending.add(_agencyPrefix + planVersion, VPackValue(VPackValueType::Object));
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

  pending.close();
  pending.close();

  // Transact
  write_ret_t res = transact(_agent, pending);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC(INFO, Logger::AGENCY) << "Pending: Change leadership " + _shard +
                                           " from " + _from + " to " + _to;
    return true;
  }

  LOG_TOPIC(INFO, Logger::AGENCY)
      << "Precondition failed for starting job " + _jobId;
  return false;
}

JOB_STATUS FailedLeader::status() {
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

      // Remove shard to /arango/Target/FailedServers/<server> array
      Builder del;
      del.openArray();
      del.openObject();
      std::string path = _agencyPrefix + failedServersPrefix + "/" + _from;
      del.add(path, VPackValue(VPackValueType::Object));
      del.add("op", VPackValue("erase"));
      del.add("val", VPackValue(_shard));
      del.close();
      del.close();
      del.close();
      write_ret_t res = transact(_agent, del);
  
      if (finish("Shards/" + shard)) {
        return FINISHED;
      }
    }
  }

  return status;
}
