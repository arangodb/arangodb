////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "FailedFollower.h"

#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"

using namespace arangodb::consensus;

FailedFollower::FailedFollower(Node const& snapshot, AgentInterface* agent,
                               std::string const& jobId, std::string const& creator,
                               std::string const& database, std::string const& collection,
                               std::string const& shard, std::string const& from)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(from) {}

FailedFollower::FailedFollower(Node const& snapshot, AgentInterface* agent,
                               JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");
  auto tmp_from = _snapshot.hasAsString(path + "fromServer");

  // set only if already started (test to prevent warning)
  if (_snapshot.has(path + "toServer")) {
    auto tmp_to = _snapshot.hasAsString(path + "toServer");
    _to = tmp_to.first;
  }

  auto tmp_shard = _snapshot.hasAsString(path + "shard");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");
  auto tmp_created = _snapshot.hasAsString(path + "timeCreated");

  if (tmp_database.second && tmp_collection.second && tmp_from.second &&
      tmp_shard.second && tmp_creator.second && tmp_created.second) {
    _database = tmp_database.first;
    _collection = tmp_collection.first;
    _from = tmp_from.first;
    // _to conditionally set above
    _shard = tmp_shard.first;
    _creator = tmp_creator.first;
    _created = stringToTimepoint(tmp_created.first);
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish("", _shard, false, err.str());
    _status = FAILED;
  }
}

FailedFollower::~FailedFollower() {}

void FailedFollower::run() { runHelper("", _shard); }

bool FailedFollower::create(std::shared_ptr<VPackBuilder> envelope) {
  using namespace std::chrono;
  LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Create failedFollower for " + _shard + " from " + _from;

  _created = system_clock::now();

  _jb = std::make_shared<Builder>();
  {
    VPackArrayBuilder transaction(_jb.get());
    {
      VPackObjectBuilder operations(_jb.get());
      // Todo entry
      _jb->add(VPackValue(toDoPrefix + _jobId));
      {
        VPackObjectBuilder todo(_jb.get());
        _jb->add("creator", VPackValue(_creator));
        _jb->add("type", VPackValue("failedFollower"));
        _jb->add("database", VPackValue(_database));
        _jb->add("collection", VPackValue(_collection));
        _jb->add("shard", VPackValue(_shard));
        _jb->add("fromServer", VPackValue(_from));
        _jb->add("jobId", VPackValue(_jobId));
        _jb->add("timeCreated", VPackValue(timepointToString(_created)));
      }
    }
  }

  write_ret_t res = singleWriteTransaction(_agent, *_jb);

  return (res.accepted && res.indices.size() == 1 && res.indices[0]);
}

bool FailedFollower::start() {
  using namespace std::chrono;

  std::vector<std::string> existing =
      _snapshot.exists(planColPrefix + _database + "/" + _collection + "/" +
                       "distributeShardsLike");

  // Fail if got distributeShardsLike
  if (existing.size() == 5) {
    finish("", _shard, false, "Collection has distributeShardsLike");
    return false;
  }
  // Collection gone
  else if (existing.size() < 4) {
    finish("", _shard, true, "Collection " + _collection + " gone");
    return false;
  }

  // Planned servers vector
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto plannedPair = _snapshot.hasAsSlice(planPath);  // if missing, what?
  Slice const& planned = plannedPair.first;
  if (!plannedPair.second) {
    // not clear what servers should or should not get failover ... retry later
    return false;
  }

  // Get proper replacement
  _to = randomIdleGoodAvailableServer(_snapshot, planned);
  if (_to.empty()) {
    // retry later
    return false;
  }

  if (std::chrono::system_clock::now() - _created > std::chrono::seconds(4620)) {
    finish("", _shard, false, "Job timed out");
    return false;
  }

  LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Start failedFollower for " + _shard + " from " + _from + " to " + _to;

  // Copy todo to pending
  Builder todo;
  {
    VPackArrayBuilder a(&todo);
    if (_jb == nullptr) {
      auto jobIdNode = _snapshot.hasAsNode(toDoPrefix + _jobId);
      if (jobIdNode.second) {
        jobIdNode.first.toBuilder(todo);
      } else {
        LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to get key " + toDoPrefix + _jobId +
                                                    " from agency snapshot";
        return false;
      }
    } else {
      todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
    }
  }

  // Replace from by to in plan and that's it
  Builder ns;
  {
    VPackArrayBuilder servers(&ns);
    for (auto const& i : VPackArrayIterator(planned)) {
      auto s = i.copyString();
      ns.add(VPackValue((s != _from) ? s : _to));
    }
    // Add the old one at the end, just in case it comes back more quickly:
    ns.add(VPackValue(_from));
  }

  // Transaction
  Builder job;

  {
    VPackArrayBuilder transactions(&job);

    {
      VPackArrayBuilder transaction(&job);
      // Operations ----------------------------------------------------------
      {
        VPackObjectBuilder operations(&job);
        // Add finished entry -----
        job.add(VPackValue(finishedPrefix + _jobId));
        {
          VPackObjectBuilder ts(&job);
          job.add("timeStarted",  // start
                  VPackValue(timepointToString(system_clock::now())));
          job.add("timeFinished",  // same same :)
                  VPackValue(timepointToString(system_clock::now())));
          job.add("toServer", VPackValue(_to));  // toServer
          for (auto const& obj : VPackObjectIterator(todo.slice()[0])) {
            job.add(obj.key.copyString(), obj.value);
          }
        }
        addRemoveJobFromSomewhere(job, "ToDo", _jobId);
        // Plan change ------------
        for (auto const& clone : clones(_snapshot, _database, _collection, _shard)) {
          job.add(planColPrefix + _database + "/" + clone.collection +
                      "/shards/" + clone.shard,
                  ns.slice());
        }

        addIncreasePlanVersion(job);
      }
      // Preconditions -------------------------------------------------------
      {
        VPackObjectBuilder preconditions(&job);
        // Failed condition persists
        job.add(VPackValue(healthPrefix + _from + "/Status"));
        {
          VPackObjectBuilder stillExists(&job);
          job.add("old", VPackValue("FAILED"));
        }
        addPreconditionUnchanged(job, planPath, planned);
        // toServer not blocked
        addPreconditionServerNotBlocked(job, _to);
        // shard not blocked
        addPreconditionShardNotBlocked(job, _shard);
        // toServer in good condition
        addPreconditionServerHealth(job, _to, "GOOD");
      }
    }
  }

  // Abort job blocking shard if abortable
  //  (likely to not exist, avoid warning message by testing first)
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    auto jobId = _snapshot.hasAsString(blockedShardsPrefix + _shard);
    if (jobId.second && !abortable(_snapshot, jobId.first)) {
      return false;
    } else if (jobId.second) {
      JobContext(PENDING, jobId.first, _snapshot, _agent).abort();
    }
  }  // if

  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "FailedFollower start transaction: " << job.toJson();

  auto res = generalTransaction(_agent, job);

  LOG_TOPIC(DEBUG, Logger::SUPERVISION)
      << "FailedFollower start result: " << res.result->toJson();

  auto result = res.result->slice()[0];

  if (res.accepted && result.isNumber()) {
    return true;
  }

  TRI_ASSERT(result.isObject());

  auto slice = result.get(std::vector<std::string>(
      {agencyPrefix, "Supervision", "Health", _from, "Status"}));
  if (slice.isString() && slice.copyString() != "FAILED") {
    finish("", _shard, false, "Server " + _from + " no longer failing.");
  }

  slice = result.get(std::vector<std::string>(
      {agencyPrefix, "Supervision", "Health", _to, "Status"}));
  if (!slice.isString() || slice.copyString() != "GOOD") {
    LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Destination server " << _to << " is no longer in good condition";
  }

  slice = result.get(std::vector<std::string>(
      {agencyPrefix, "Plan", "Collections", _database, _collection, "shards", _shard}));
  if (!slice.isNone()) {
    LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Planned db server list is in mismatch with snapshot";
  }

  slice = result.get(
      std::vector<std::string>({agencyPrefix, "Supervision", "DBServers", _to}));
  if (!slice.isNone()) {
    LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Destination " << _to << " is now blocked by job " << slice.copyString();
  }

  slice = result.get(
      std::vector<std::string>({agencyPrefix, "Supervision", "Shards", _shard}));
  if (!slice.isNone()) {
    LOG_TOPIC(INFO, Logger::SUPERVISION)
        << "Shard " << _shard << " is now blocked by job " << slice.copyString();
  }

  return false;
}

JOB_STATUS FailedFollower::status() {
  // We can only be hanging around TODO. start === finished
  return TODO;
}

arangodb::Result FailedFollower::abort() {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting failedFollower job beyond pending stage");
  }

  Result result;
  // Can now only be TODO
  if (_status == TODO) {
    finish("", "", false, "job aborted");
    return result;
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return result;
}
