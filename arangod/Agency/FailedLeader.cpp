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

#include "Agency/Agent.h"
#include "Agency/Job.h"

#include <algorithm>
#include <vector>

using namespace arangodb::consensus;

FailedLeader::FailedLeader(Node const& snapshot, Agent* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& database,
                           std::string const& collection,
                           std::string const& shard, std::string const& from,
                           std::string const& to)
    : Job(snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(from),
      _to(to) {}

FailedLeader::~FailedLeader() {}

void FailedLeader::run() {
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
    LOG_TOPIC(DEBUG, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
    finish("Shards/" + _shard, false, e.what());
  }
}

bool FailedLeader::create(std::shared_ptr<VPackBuilder> b) {

  using namespace std::chrono;
  LOG_TOPIC(INFO, Logger::AGENCY)
    << "Handle failed Leader for " + _shard + " from " + _from + " to " + _to;

  _jb = std::make_shared<Builder>();

  { VPackArrayBuilder transaction(_jb.get());
    
    { VPackObjectBuilder operations(_jb.get());
      // Todo entry
      _jb->add(VPackValue(agencyPrefix + toDoPrefix + _jobId));
      { VPackObjectBuilder todo(_jb.get());
        _jb->add("creator", VPackValue(_creator));
        _jb->add("type", VPackValue("failedLeader"));
        _jb->add("database", VPackValue(_database));
        _jb->add("collection", VPackValue(_collection));
        _jb->add("shard", VPackValue(_shard));
        _jb->add("fromServer", VPackValue(_from));
        _jb->add("toServer", VPackValue(_to));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add(
          "timeCreated", VPackValue(timepointToString(system_clock::now())));
      }
      // Add shard to /arango/Target/FailedServers/<server> array
      _jb->add(VPackValue(agencyPrefix + failedServersPrefix + "/" + _from));
      { VPackObjectBuilder failed(_jb.get());
        _jb->add("op", VPackValue("push"));
        _jb->add("new", VPackValue(_shard));
      }}}
  
  write_ret_t res = transact(_agent, *_jb);
  
  return (res.accepted && res.indices.size() == 1 && res.indices[0]);
  
}

bool FailedLeader::start() {

  using namespace std::chrono;
  
  // DBservers
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  std::string curPath =
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers";

  auto const& current = _snapshot(curPath).slice();
  auto const& planned = _snapshot(planPath).slice();

  if (current.length() == 1) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to change leadership for shard " + _shard + " from " + _from 
      +  " to " + _to + ". No in-sync followers:" + current.toJson();
    return false;
  }

  // Get todo entry
  Builder todo;
  { VPackArrayBuilder t(&todo);
    if (_jb == nullptr) {
      try {
        _snapshot(toDoPrefix + _jobId).toBuilder(todo);
      } catch (std::exception const&) {
        LOG_TOPIC(INFO, Logger::AGENCY)
          << "Failed to get key " + toDoPrefix + _jobId + " from agency snapshot";
        return false;
      }
    } else {
      todo.add(_jb->slice().get(agencyPrefix + toDoPrefix + _jobId).valueAt(0));
    }}
  
  std::vector<std::string> planv;
  for (auto const& i : VPackArrayIterator(planned)) {
    auto s = i.copyString();
    if (s != _from && s != _to) {
      planv.push_back(i.copyString());
    }
  }

  // Transaction
  Builder pending;
  { VPackArrayBuilder transaction(&pending);
    
    // Operations ------------------------------------------------------------
    { VPackObjectBuilder operations(&pending);
      // Add pending entry
      pending.add(VPackValue(agencyPrefix + pendingPrefix + _jobId));
      { VPackObjectBuilder ts(&pending);
        pending.add("timeStarted",
                    VPackValue(timepointToString(system_clock::now())));
        for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
          pending.add(obj.key.copyString(), obj.value);
        }
      }
      // Remove todo entry
      pending.add(VPackValue(agencyPrefix + toDoPrefix + _jobId));
      { VPackObjectBuilder rem(&pending);
        pending.add("op", VPackValue("delete")); }
      // DB server vector
      pending.add(VPackValue(agencyPrefix + planPath));
      { VPackArrayBuilder dbs(&pending);
        pending.add(VPackValue(_to));
        for (auto const& i : VPackArrayIterator(current)) {
          std::string s = i.copyString();
          if (s != _from && s != _to) {
            pending.add(i);
            planv.erase(
              std::remove(planv.begin(), planv.end(), s), planv.end());
          }
        }
        pending.add(VPackValue(_from));
        for (auto const& i : planv) {
          pending.add(VPackValue(i));
        }}
      // Block shard
      pending.add(VPackValue(agencyPrefix + blockedShardsPrefix + _shard));
      { VPackObjectBuilder block(&pending);
        pending.add("jobId", VPackValue(_jobId)); }
      // Increment Plan/Version
      pending.add(VPackValue(agencyPrefix + planVersion));
      { VPackObjectBuilder version(&pending);
        pending.add("op", VPackValue("increment")); }} // Operations ---------

    // Preconditions ---------------------------------------------------------
    { VPackObjectBuilder preconditions(&pending);    
      // Current servers are as we expect
      pending.add(VPackValue(agencyPrefix + curPath));
      { VPackObjectBuilder cur(&pending);
        pending.add("old", current); }
      // Plan servers are as we expect
      pending.add(VPackValue(agencyPrefix + planPath));
      { VPackObjectBuilder plan(&pending);
        pending.add("old", planned); }
      // Shard is not blocked
      
      pending.add(VPackValue(agencyPrefix + blockedShardsPrefix + _shard));
      { VPackObjectBuilder blocked(&pending);
        pending.add("oldEmpty", VPackValue(true)); }} // Preconditions --------

  }

  // Transact
  write_ret_t res = transact(_agent, pending);

  return (res.accepted && res.indices.size() == 1 && res.indices[0]);

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
    auto const& planned = _snapshot(planPath);
    auto const& current = _snapshot(curPath);

    if (planned.slice()[0] == current.slice()[0]) {

      // Remove shard to /arango/Target/FailedServers/<server> array
      Builder del;
      { VPackArrayBuilder a(&del);
        { VPackObjectBuilder o(&del);
          del.add(VPackValue(agencyPrefix + failedServersPrefix + "/" + _from));
          { VPackObjectBuilder erase(&del);
            del.add("op", VPackValue("erase"));
            del.add("val", VPackValue(_shard));
          }}}

      write_ret_t res = transact(_agent, del);
      
      if (finish("Shards/" + shard)) {
        return FINISHED;
      }
    }

  }

  return status;
  
}

void FailedLeader::abort() {
  // TO BE IMPLEMENTED
}

