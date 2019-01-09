////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AddFollower.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::consensus;

AddFollower::AddFollower(Node const& snapshot, AgentInterface* agent,
                         std::string const& jobId, std::string const& creator,
                         std::string const& database,
                         std::string const& collection, std::string const& shard)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard) {}

AddFollower::AddFollower(Node const& snapshot, AgentInterface* agent,
                         JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");
  auto tmp_shard = _snapshot.hasAsString(path + "shard");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");

  if (tmp_database.second && tmp_collection.second && tmp_shard.second &&
      tmp_creator.second) {
    _database = tmp_database.first;
    _collection = tmp_collection.first;
    _shard = tmp_shard.first;
    _creator = tmp_creator.first;
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency.";
    LOG_TOPIC(ERR, Logger::SUPERVISION) << err.str();
    finish("", tmp_shard.first, false, err.str());
    _status = FAILED;
  }
}

AddFollower::~AddFollower() {}

void AddFollower::run() { runHelper("", _shard); }

bool AddFollower::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Todo: AddFollower(s) "
      << " to shard " << _shard << " in collection " << _collection;

  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string now(timepointToString(std::chrono::system_clock::now()));

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  _jb->add(VPackValue(toDoPrefix + _jobId));
  {
    VPackObjectBuilder guard(_jb.get());
    _jb->add("creator", VPackValue(_creator));
    _jb->add("type", VPackValue("addFollower"));
    _jb->add("database", VPackValue(_database));
    _jb->add("collection", VPackValue(_collection));
    _jb->add("shard", VPackValue(_shard));
    _jb->add("jobId", VPackValue(_jobId));
    _jb->add("timeCreated", VPackValue(now));
  }

  _status = TODO;

  if (!selfCreate) {
    return true;
  }

  _jb->close();  // transaction object
  _jb->close();  // close array

  write_ret_t res = singleWriteTransaction(_agent, *_jb);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool AddFollower::start() {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", "", true, "collection has been dropped in the meantime");
    return false;
  }
  Node collection =
      _snapshot.hasAsNode(planColPrefix + _database + "/" + _collection).first;
  if (collection.has("distributeShardsLike")) {
    finish("", "", false,
           "collection must not have 'distributeShardsLike' attribute");
    return false;
  }

  // Look at Plan:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  Slice planned = _snapshot.hasAsSlice(planPath).first;

  TRI_ASSERT(planned.isArray());

  // First check that we still have too few followers for the current
  // `replicationFactor`:
  size_t desiredReplFactor = collection.hasAsUInt("replicationFactor").first;
  size_t actualReplFactor = planned.length();
  if (actualReplFactor >= desiredReplFactor) {
    finish("", "", true, "job no longer necessary, have enough replicas");
    return true;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " is currently locked, not starting AddFollower job " << _jobId;
    return false;
  }

  // Now find some new servers to add:
  auto available = Job::availableServers(_snapshot);
  // Remove those already in Plan:
  for (VPackSlice server : VPackArrayIterator(planned)) {
    available.erase(std::remove(available.begin(), available.end(), server.copyString()),
                    available.end());
  }
  // Remove those that are not in state "GOOD":
  auto it = available.begin();
  while (it != available.end()) {
    if (checkServerHealth(_snapshot, *it) != "GOOD") {
      it = available.erase(it);
    } else {
      ++it;
    }
  }

  // Check that we have enough:
  if (available.size() < desiredReplFactor - actualReplFactor) {
    LOG_TOPIC(DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " does not have enough candidates to add followers, waiting, jobId=" << _jobId;
    return false;
  }

  // Randomly choose enough servers:
  std::vector<std::string> chosen;
  for (size_t i = 0; i < desiredReplFactor - actualReplFactor; ++i) {
    size_t pos = arangodb::RandomGenerator::interval(static_cast<int64_t>(0),
                                                     available.size() - 1);
    chosen.push_back(available[pos]);
    if (pos < available.size() - 1) {
      available[pos] = available[available.size() - 1];
    }
    available.pop_back();
  }

  // Now we can act, simply add all in chosen to all plans for all shards:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  // Copy todo to finished:
  Builder todo, trx;

  // Get todo entry
  {
    VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo.second) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC(INFO, Logger::SUPERVISION) << "Failed to get key " + toDoPrefix + _jobId +
                                                    " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC(WARN, Logger::SUPERVISION)
            << e.what() << ": " << __FILE__ << ":" << __LINE__;
        return false;
      }
    }
  }

  // Enter pending, remove todo, block toserver
  {
    VPackArrayBuilder listOfTransactions(&trx);

    {
      VPackObjectBuilder objectForMutation(&trx);

      addPutJobIntoSomewhere(trx, "Finished", todo.slice()[0]);
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);

      // --- Plan changes
      doForAllShards(_snapshot, _database, shardsLikeMe,
                     [&trx, &chosen](Slice plan, Slice current, std::string& planPath) {
                       trx.add(VPackValue(planPath));
                       {
                         VPackArrayBuilder serverList(&trx);
                         for (auto const& srv : VPackArrayIterator(plan)) {
                           trx.add(srv);
                         }
                         for (auto const& srv : chosen) {
                           trx.add(VPackValue(srv));
                         }
                       }
                     });

      addIncreasePlanVersion(trx);
    }  // mutation part of transaction done
    // Preconditions
    {
      VPackObjectBuilder precondition(&trx);
      // --- Check that Planned servers are still as we expect
      addPreconditionUnchanged(trx, planPath, planned);
      addPreconditionShardNotBlocked(trx, _shard);
      for (auto const& srv : chosen) {
        addPreconditionServerHealth(trx, srv, "GOOD");
      }
    }  // precondition done
  }    // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, trx);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = FINISHED;
    LOG_TOPIC(INFO, Logger::SUPERVISION) << "Finished: Addfollower(s) to shard "
                                         << _shard << " in collection " << _collection;
    return true;
  }

  LOG_TOPIC(INFO, Logger::SUPERVISION)
      << "Start precondition failed for AddFollower " + _jobId;
  return false;
}

JOB_STATUS AddFollower::status() {
  if (_status != PENDING) {
    return _status;
  }

  TRI_ASSERT(false);  // PENDING is not an option for this job, since it
                      // travels directly from ToDo to Finished or Failed
  return _status;
}

arangodb::Result AddFollower::abort() {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting addFollower job beyond pending stage");
  }

  Result result;
  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted");
    return result;
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return result;
}
