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
#include "Agency/JobContext.h"

#include <algorithm>
#include <vector>

using namespace arangodb::consensus;

FailedLeader::FailedLeader(Node const& snapshot, AgentInterface* agent,
                           std::string const& jobId, std::string const& creator,
                           std::string const& database,
                           std::string const& collection,
                           std::string const& shard, std::string const& from)
    : Job(NOTFOUND, snapshot, agent, jobId, creator), _database(database),
      _collection(collection), _shard(shard), _from(from) {}

FailedLeader::FailedLeader(
  Node const& snapshot, AgentInterface* agent, JOB_STATUS status,
  std::string const& jobId) : Job(status, snapshot, agent, jobId) {
  
  // Get job details from agency:
  try {
    std::string path = pos[status] + _jobId + "/";
    _database = _snapshot(path + "database").getString();
    _collection = _snapshot(path + "collection").getString();
    _from = _snapshot(path + "fromServer").getString();
    try {
      // set only if already started
      _to = _snapshot(path + "toServer").getString();
    } catch (...) {}
    _shard = _snapshot(path + "shard").getString();
    _creator = _snapshot(path + "creator").getString();
    _created = stringToTimepoint(_snapshot(path + "timeCreated").getString());
  } catch (std::exception const& e) {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency: " << e.what();
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish("", _shard, false, err.str());
    _status = FAILED;
  }
  
}

FailedLeader::~FailedLeader() {}

void FailedLeader::run() {
  runHelper("", _shard);
}

void FailedLeader::rollback() {

  // Create new plan servers (exchange _to and _from)
  std::string planPath
    = planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto const& planned = _snapshot(planPath).slice();
  std::shared_ptr<Builder> payload = nullptr;

  if (_status == PENDING) { // Only payload if pending. Otherwise just fail.
    VPackBuilder rb;
    if (!_to.empty()) {
      { VPackArrayBuilder r(&rb);
        for (auto const i : VPackArrayIterator(planned)) {
          TRI_ASSERT(i.isString());
          auto istr = i.copyString();
          if (istr == _from) {
            rb.add(VPackValue(_to));
          } else if (istr == _to) {
            rb.add(VPackValue(_from));
          } else {
            rb.add(i);
          }
        }
      }
    } else {
      rb.add(planned);
    }
  
    auto cs = clones(_snapshot, _database, _collection, _shard);
  
    // Transactions
    payload = std::make_shared<Builder>();
    { VPackObjectBuilder b(payload.get());
      for (auto const c : cs) {
        payload->add(planColPrefix + _database + "/" + c.collection + "/shards/" +
                     c.shard, rb.slice());
      }
    }
  }

  finish("", _shard, false, "Timed out.", payload);
  
}


bool FailedLeader::create(std::shared_ptr<VPackBuilder> b) {

  using namespace std::chrono;
  LOG_TOPIC(INFO, Logger::SUPERVISION)
    << "Create failedLeader for " + _shard + " from " + _from;
  
  _jb = std::make_shared<Builder>();
  { VPackArrayBuilder transaction(_jb.get());
    { VPackObjectBuilder operations(_jb.get());
      // Todo entry
      _jb->add(VPackValue(toDoPrefix + _jobId));
      { VPackObjectBuilder todo(_jb.get());
        _jb->add("creator", VPackValue(_creator));
        _jb->add("type", VPackValue("failedLeader"));
        _jb->add("database", VPackValue(_database));
        _jb->add("collection", VPackValue(_collection));
        _jb->add("shard", VPackValue(_shard));
        _jb->add("fromServer", VPackValue(_from));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add(
          "timeCreated", VPackValue(timepointToString(system_clock::now())));
      }}}
  write_ret_t res = singleWriteTransaction(_agent, *_jb);  
  return (res.accepted && res.indices.size() == 1 && res.indices[0]);
  
}


bool FailedLeader::start() {

  std::vector<std::string> existing =
    _snapshot.exists(planColPrefix + _database + "/" + _collection + "/" +
                     "distributeShardsLike");
  
  // Fail if got distributeShardsLike
  if (existing.size() == 5) {
    finish("", _shard, false, "Collection has distributeShardsLike");
    return false;
  }
  // Fail if collection gone
  else if (existing.size() < 4) { 
    finish("", _shard, true, "Collection " + _collection + " gone");
    return false;
  }

  // Get healthy in Sync follower common to all prototype + clones
  auto commonHealthyInSync =
    findNonblockedCommonHealthyInSyncFollower(
      _snapshot, _database, _collection, _shard);
  if (commonHealthyInSync.empty()) {
    return false;
  } else {
    _to = commonHealthyInSync;
  }
  
  LOG_TOPIC(INFO, Logger::SUPERVISION)
    << "Start failedLeader for " + _shard + " from " + _from + " to " + _to;  
  
  using namespace std::chrono;

  // Current servers vector
  auto const& current =
    _snapshot(
      curColPrefix + _database + "/" + _collection + "/" + _shard + "/servers")
    .slice();
  // Planned servers vector
  std::string planPath
    = planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto const& planned = _snapshot(planPath).slice();

  // Get todo entry
  Builder todo;
  { VPackArrayBuilder t(&todo);
    if (_jb == nullptr) {
      try {
        _snapshot(toDoPrefix + _jobId).toBuilder(todo);
      } catch (std::exception const&) {
        LOG_TOPIC(INFO, Logger::SUPERVISION)
          << "Failed to get key " + toDoPrefix + _jobId
          + " from agency snapshot";
        return false;
      }
    } else {
      todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
    }}

  // New plan vector excluding _to and _from
  std::vector<std::string> planv;
  for (auto const& i : VPackArrayIterator(planned)) {
    auto s = i.copyString();
    if (s != _from && s != _to) {
      planv.push_back(s);
    }
  }

  // Additional follower, if applicable
  auto additionalFollower = randomIdleGoodAvailableServer(_snapshot, planned);
  if (!additionalFollower.empty()) {
    planv.push_back(additionalFollower);
  }

  // Transactions
  Builder pending;
  
  { VPackArrayBuilder transactions(&pending);
    { VPackArrayBuilder transaction(&pending);
      
      // Operations ----------------------------------------------------------
      { VPackObjectBuilder operations(&pending);
        // Add pending entry
        pending.add(VPackValue(pendingPrefix + _jobId));
        { VPackObjectBuilder ts(&pending);
          pending.add("timeStarted", // start
                       VPackValue(timepointToString(system_clock::now())));
          pending.add("toServer", VPackValue(_to)); // toServer
          for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
            pending.add(obj.key.copyString(), obj.value);
          }
        }
        addRemoveJobFromSomewhere(pending, "ToDo", _jobId);
        // DB server vector -------
        Builder ns;
        { VPackArrayBuilder servers(&ns);
          ns.add(VPackValue(_to));  
          for (auto const& i : VPackArrayIterator(current)) {
            std::string s = i.copyString();
            if (s != _from && s != _to) {
              ns.add(i);
              planv.erase(
                std::remove(planv.begin(), planv.end(), s), planv.end());
            }
          }
          ns.add(VPackValue(_from));
          for (auto const& i : planv) {
            ns.add(VPackValue(i));
          }
        }
        for (auto const& clone :
               clones(_snapshot, _database, _collection, _shard)) {
          pending.add(
            planColPrefix + _database + "/"
            + clone.collection + "/shards/" + clone.shard, ns.slice());
        }
        addBlockShard(pending, _shard, _jobId);
        addIncreasePlanVersion(pending);
      }
      // Preconditions -------------------------------------------------------
      { VPackObjectBuilder preconditions(&pending);
        // Failed condition persists
        addPreconditionServerHealth(pending, _from, "FAILED");
        // Destination server still in good condition
        addPreconditionServerHealth(pending, _to, "GOOD"); 
        // Server list in plan still as before
        addPreconditionUnchanged(pending, planPath, planned);
        // Destination server should not be blocked by another job
        addPreconditionServerNotBlocked(pending, _to);
        // Shard to be handled is block by another job
        addPreconditionShardNotBlocked(pending, _shard);
      } // Preconditions -----------------------------------------------------
    }
  }

  // Abort job blocking server if abortable
  try {
    std::string jobId = _snapshot(blockedShardsPrefix + _shard).getString();
    if (!abortable(_snapshot, jobId)) {
      return false;
    } else {
      JobContext(PENDING, jobId, _snapshot, _agent).abort();
    }
  } catch (...) {}
  
  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
    << "FailedLeader transaction: " << pending.toJson();
  
  trans_ret_t res = generalTransaction(_agent, pending);
  
  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
    << "FailedLeader result: " << res.result->toJson();


  // Something went south. Let's see
  auto result = res.result->slice()[0];

  if (res.accepted && result.isNumber()) {
    return true;
  }

  TRI_ASSERT(result.isObject());

  if (!res.accepted) { // lost leadership
    LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Leadership lost! Job " << _jobId << " handed off.";
  }
  
  if (result.isObject()) {

    // Still failing _from?
    auto slice = result.get(
      std::vector<std::string>({
          agencyPrefix, "Supervision", "Health", _from, "Status"}));
    if (slice.isString() && slice.copyString() == "GOOD") {
      finish("", _shard, false, "Server " + _from + " no longer failing.");
      return false;
    }

    // Still healthy _to?
    slice = result.get(
      std::vector<std::string>(
        {agencyPrefix, "Supervision", "Health", _to, "Status"}));
    if (slice.isString() && slice.copyString() != "GOOD") {
      LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Will not failover from " << _from << " to " << _to
        << " as target server is no longer in good condition. Will retry.";
      return false;
    }

    // Snapshot and plan still in sync with respect to server list?
    slice = result.get(
      std::vector<std::string>({agencyPrefix, "Plan", "Collections", _database,
            _collection, "shards", _shard}));
    if (!slice.isNone()) {
      LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Plan no longer holds the expected server list. Will retry.";
    }

    // To server blocked by other job?
    slice = result.get(
      std::vector<std::string>(
        {agencyPrefix, "Supervision", "DBServers", _to}));
    if (!slice.isNone()) {
      LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Destination server " << _to << " meanwhile is blocked by job " <<
        slice.copyString();
    }

    // This shard blocked by other job?
    slice = result.get(
      std::vector<std::string>(
        {agencyPrefix, "Supervision", "Shards", _shard}));
    if (!slice.isNone()) {
      LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Shard  " << _shard << " meanwhile is blocked by job " <<
        slice.copyString();
    }
  }
    
  return false;
  
}

JOB_STATUS FailedLeader::status() {

  if(!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", _shard, true, "Collection " + _collection + " gone");
    return FINISHED;
  }

  // Timedout after 77 minutes
  if (std::chrono::system_clock::now() - _created > std::chrono::seconds(4620)) {
    rollback();
  }

  if (_status != PENDING) {
    return _status;
  }

  Node const& job = _snapshot(pendingPrefix + _jobId);
  std::string database = job("database").getString(),
    shard = job("shard").getString();
  
  bool done = false;
  for (auto const& clone : clones(_snapshot, _database, _collection, _shard)) {
    auto sub = database + "/" + clone.collection;
    if(_snapshot(planColPrefix + sub + "/shards/" + clone.shard).slice()[0] !=
       _snapshot(curColPrefix + sub + "/" + clone.shard + "/servers").slice()[0]) {
      LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "FailedLeader waiting for " << sub + "/" + shard;
      break;
    }
    done = true;
  }
  
  if (done) {
    // Remove shard to /arango/Target/FailedServers/<server> array
    Builder del;
    { VPackArrayBuilder a(&del);
      { VPackObjectBuilder o(&del);
        del.add(VPackValue(failedServersPrefix + "/" + _from));
        { VPackObjectBuilder erase(&del);
          del.add("op", VPackValue("erase"));
          del.add("val", VPackValue(_shard));
        }}}
    
    write_ret_t res = singleWriteTransaction(_agent, del);
    if (finish("", shard)) {
      LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Finished failedLeader for " + _shard + " from " + _from + " to " + _to;  
        return FINISHED;
    }
  }
  
  return _status;
}


arangodb::Result FailedLeader::abort() {
  // job is only abortable when it is in ToDo
  if (_status != TODO) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting failedFollower job beyond todo stage");
  } else {
    finish("", "", false, "job aborted");
    return Result();
  }
}


