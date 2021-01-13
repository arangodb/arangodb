////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AddFollower.h"

#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Basics/StaticStrings.h"
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
    LOG_TOPIC("4d260", ERR, Logger::SUPERVISION) << err.str();
    finish("", tmp_shard.first, false, err.str());
    _status = FAILED;
  }
}

AddFollower::~AddFollower() = default;

void AddFollower::run(bool& aborts) { runHelper("", _shard, aborts); }

bool AddFollower::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC("8f72c", INFO, Logger::SUPERVISION)
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

  write_ret_t res = singleWriteTransaction(_agent, *_jb, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return true;
  }

  _status = NOTFOUND;

  LOG_TOPIC("02cef", INFO, Logger::SUPERVISION) << "Failed to insert job " + _jobId;
  return false;
}

bool AddFollower::start(bool&) {
  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    finish("", "", true, "collection has been dropped in the meantime");
    return false;
  }
  Node const& collection =
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
  size_t desiredReplFactor = 1;
  auto replFact = collection.hasAsUInt(StaticStrings::ReplicationFactor);
  if (replFact.second) {
    desiredReplFactor = replFact.first;
  } else {
    auto replFact2 = collection.hasAsString(StaticStrings::ReplicationFactor);
    if (replFact2.second && replFact2.first == StaticStrings::Satellite) {
      // satellites => distribute to every server
      auto available = Job::availableServers(_snapshot);
      desiredReplFactor = Job::countGoodOrBadServersInList(_snapshot, available);
    }
  }

  VPackBuilder onlyFollowers;
  {
    VPackArrayBuilder guard(&onlyFollowers);
    bool first = true;
    for (auto const& pp : VPackArrayIterator(planned)) {
      if (!first) {
        onlyFollowers.add(pp);
      }
      first = false;
    }
  }
  size_t actualReplFactor
      = 1 + Job::countGoodOrBadServersInList(_snapshot, onlyFollowers.slice());
      // Leader plus good followers in plan
  if (actualReplFactor >= desiredReplFactor) {
    finish("", "", true, "job no longer necessary, have enough replicas");
    return true;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC("1de2b", DEBUG, Logger::SUPERVISION)
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

  // Exclude servers in failoverCandidates for some clone and those in Plan:
  auto shardsLikeMe = clones(_snapshot, _database, _collection, _shard);
  auto failoverCands = Job::findAllFailoverCandidates(
      _snapshot, _database, shardsLikeMe);
  it = available.begin();
  while (it != available.end()) {
    if (failoverCands.find(*it) != failoverCands.end()) {
      it = available.erase(it);
    } else {
      ++it;
    }
  }

  // Check that we have enough:
  if (available.size() < desiredReplFactor - actualReplFactor) {
    LOG_TOPIC("50086", DEBUG, Logger::SUPERVISION)
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
        LOG_TOPIC("24c50", INFO, Logger::SUPERVISION) << "Failed to get key " + toDoPrefix + _jobId +
                                                    " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC("15c37", WARN, Logger::SUPERVISION)
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
                     [&trx, &chosen](Slice plan, Slice current, std::string& planPath, std::string& curPath) {
                       trx.add(VPackValue(planPath));
                       {
                         VPackArrayBuilder serverList(&trx);
                         for (VPackSlice srv : VPackArrayIterator(plan)) {
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
      // Check that failoverCandidates are still as we inspected them:
      doForAllShards(_snapshot, _database, shardsLikeMe,
          [this, &trx](Slice plan, Slice current,
                           std::string& planPath,
                           std::string& curPath) {
            // take off "servers" from curPath and add
            // "failoverCandidates":
            std::string foCandsPath = curPath.substr(0, curPath.size() - 7);
            foCandsPath += StaticStrings::FailoverCandidates;
            auto foCands = this->_snapshot.hasAsSlice(foCandsPath);
            if (foCands.second) {
              addPreconditionUnchanged(trx, foCandsPath, foCands.first);
            }
          });
      addPreconditionShardNotBlocked(trx, _shard);
      for (auto const& srv : chosen) {
        addPreconditionServerHealth(trx, srv, "GOOD");
      }
    }  // precondition done
  }    // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    _status = FINISHED;
    LOG_TOPIC("961a4", INFO, Logger::SUPERVISION) << "Finished: Addfollower(s) to shard "
                                         << _shard << " in collection " << _collection;
    return true;
  }

  LOG_TOPIC("63ba4", INFO, Logger::SUPERVISION)
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

arangodb::Result AddFollower::abort(std::string const& reason) {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                  "Failed aborting addFollower job beyond pending stage");
  }

  // Can now only be TODO or PENDING
  if (_status == TODO) {
    finish("", "", false, "job aborted:" + reason);
    return Result{};
  }

  TRI_ASSERT(false);  // cannot happen, since job moves directly to FINISHED
  return Result{};
}
