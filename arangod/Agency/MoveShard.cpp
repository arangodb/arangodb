////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "MoveShard.h"

#include "Agency/AgencyPaths.h"
#include "Agency/AgentInterface.h"
#include "Agency/Job.h"
#include "Agency/Node.h"
#include "Basics/StaticStrings.h"
#include "Basics/TimeString.h"
#include "Cluster/ClusterHelpers.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::velocypack;

constexpr auto PARENT_JOB_ID = "parentJob";
constexpr auto EXPECTED_TARGET_VERSION = "expectedTargetVersion";

MoveShard::MoveShard(Node const& snapshot, AgentInterface* agent,
                     std::string const& jobId, std::string const& creator,
                     std::string const& database, std::string const& collection,
                     ShardID const& shard, std::string const& from,
                     std::string const& to, bool isLeader, bool remainsFollower,
                     bool tryUndo)
    : Job(NOTFOUND, snapshot, agent, jobId, creator),
      _database(database),
      _collection(collection),
      _shard(shard),
      _from(id(from)),
      _to(id(to)),
      _isLeader(
          isLeader),  // will be initialized properly when information known
      _remainsFollower(remainsFollower),
      _toServerIsFollower(false),
      _tryUndo(tryUndo) {}

MoveShard::MoveShard(Node const& snapshot, AgentInterface* agent,
                     JOB_STATUS status, std::string const& jobId)
    : Job(status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto tmp_database = _snapshot.hasAsString(path + "database");
  auto tmp_collection = _snapshot.hasAsString(path + "collection");
  auto tmp_from = _snapshot.hasAsString(path + "fromServer");
  auto tmp_to = _snapshot.hasAsString(path + "toServer");
  auto tmp_shard = _snapshot.hasAsString(path + "shard");
  auto tmp_isLeader = _snapshot.hasAsBool(path + "isLeader");
  auto tmp_remainsFollower = _snapshot.hasAsBool(path + "remainsFollower");
  auto tmp_creator = _snapshot.hasAsString(path + "creator");
  auto tmp_parent = _snapshot.hasAsString(path + PARENT_JOB_ID);
  auto tmp_targetVersion = _snapshot.hasAsUInt(path + EXPECTED_TARGET_VERSION);
  auto tmp_tryUndo = _snapshot.hasAsBool(path + "tryUndo");

  if (tmp_database && tmp_collection && tmp_from && tmp_to && tmp_shard &&
      tmp_creator && tmp_isLeader) {
    _database = tmp_database.value();
    _collection = tmp_collection.value();
    _from = tmp_from.value();
    _to = tmp_to.value();
    _shard = ShardID{tmp_shard.value()};
    _isLeader = *tmp_isLeader;
    _remainsFollower = tmp_remainsFollower.value_or(_isLeader);
    _toServerIsFollower = false;
    _creator = tmp_creator.value();
    if (tmp_parent) {
      _parentJobId = std::move(*tmp_parent);
    }
    if (tmp_targetVersion) {
      _expectedTargetVersion = *tmp_targetVersion;
    }
    if (tmp_tryUndo) {
      _tryUndo = tmp_tryUndo.value();
    }
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    err << "database = " << tmp_database.has_value() << " ";
    err << "collection = " << tmp_collection.has_value() << " ";
    err << "from = " << tmp_from.has_value() << " ";
    err << "to = " << tmp_to.has_value() << " ";
    err << "shard = " << tmp_shard.has_value() << " ";
    err << "creator = " << tmp_creator.has_value() << " ";
    err << "isLeader = " << tmp_isLeader.has_value() << " ";
    err << "tryUndo = " << tmp_tryUndo.has_value() << " ";
    LOG_TOPIC("cfbc3", ERR, Logger::SUPERVISION) << err.str();
    moveShardFinish(false, false, err.str());
    _status = FAILED;
  }
}

MoveShard::~MoveShard() = default;

void MoveShard::run(bool& aborts) { runHelper(_to, _shard, aborts); }

bool MoveShard::create(std::shared_ptr<VPackBuilder> envelope) {
  LOG_TOPIC("02579", DEBUG, Logger::SUPERVISION)
      << "Todo: Move shard " << _shard << " from " << _from << " to " << _to;

  bool selfCreate = (envelope == nullptr);  // Do we create ourselves?

  if (selfCreate) {
    _jb = std::make_shared<Builder>();
  } else {
    _jb = envelope;
  }

  std::string now(timepointToString(std::chrono::system_clock::now()));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // DBservers
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;

  auto planNode = _snapshot.get(planPath);
  TRI_ASSERT(planNode != nullptr);
  auto planBuilder = planNode->toBuilder();
  Slice plan = planBuilder.slice();
  TRI_ASSERT(plan.isArray());
  TRI_ASSERT(plan[0].isString());
#endif

  if (selfCreate) {
    _jb->openArray();
    _jb->openObject();
  }

  _jb->add(
      VPackValue(_from == _to ? failedPrefix + _jobId : toDoPrefix + _jobId));
  {
    VPackObjectBuilder guard2(_jb.get());
    if (_from == _to) {
      _jb->add("timeFinished", VPackValue(now));
      _jb->add(
          "result",
          VPackValue("Source and destination of moveShard must be different"));
    }
    _jb->add("creator", VPackValue(_creator));
    _jb->add("type", VPackValue("moveShard"));
    _jb->add("database", VPackValue(_database));
    _jb->add("collection", VPackValue(_collection));
    _jb->add("shard", VPackValue(_shard));
    _jb->add("fromServer", VPackValue(_from));
    _jb->add("toServer", VPackValue(_to));
    _jb->add("isLeader", VPackValue(_isLeader));
    _jb->add("remainsFollower", VPackValue(_remainsFollower));
    _jb->add("jobId", VPackValue(_jobId));
    _jb->add("timeCreated", VPackValue(now));
    _jb->add("tryUndo", VPackValue(_tryUndo));
    if (!_parentJobId.empty()) {
      _jb->add(PARENT_JOB_ID, VPackValue(_parentJobId));
    }
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

  LOG_TOPIC("cb317", INFO, Logger::SUPERVISION)
      << "Failed to insert job " << _jobId;
  return false;
}

bool MoveShard::checkLeaderFollowerCurrent(
    std::vector<Job::shard_t> const& shardsLikeMe) {
  bool ok = true;
  for (auto const& s : shardsLikeMe) {
    auto sharedPath = _database + "/" + s.collection + "/";
    auto currentServersPath = curColPrefix + sharedPath + s.shard + "/servers";
    auto serverList = _snapshot.hasAsArray(currentServersPath);
    if (serverList && serverList->size() > 0) {
      if (_from != (*serverList)[0].stringView()) {
        LOG_TOPIC("55261", DEBUG, Logger::SUPERVISION)
            << "MoveShard: From server " << _from
            << " has not yet assumed leadership for collection " << s.collection
            << " shard " << s.shard
            << ", delaying start of MoveShard job for shard " << _shard;
        ok = false;
        break;
      }
      bool toFound = false;
      for (auto server : *serverList) {
        if (_to == server.stringView()) {
          toFound = true;
          break;
        }
      }
      if (!toFound) {
        LOG_TOPIC("55262", DEBUG, Logger::SUPERVISION)
            << "MoveShard: To server " << _to
            << " is not in sync for collection " << s.collection << " shard "
            << s.shard << ", delaying start of MoveShard job for shard "
            << _shard;
        ok = false;
        break;
      }
    } else {
      LOG_TOPIC("55263", INFO, Logger::SUPERVISION)
          << "MoveShard: Did not find a non-empty server list in Current "
             "for collection "
          << s.collection << " and shard " << s.shard;
      ok = false;  // not even a server list found
    }
  }
  return ok;
}

bool MoveShard::start(bool&) {
  if (considerCancellation()) {
    return false;
  }

  // If anything throws here, the run() method catches it and finishes
  // the job.

  // Check if the fromServer exists:
  if (!_snapshot.has(plannedServers + "/" + _from)) {
    moveShardFinish(false, false,
                    "fromServer does not exist as DBServer in Plan");
    return false;
  }

  // Check if the toServer exists:
  if (!_snapshot.has(plannedServers + "/" + _to)) {
    moveShardFinish(false, false,
                    "toServer does not exist as DBServer in Plan");
    return false;
  }

  // Are we distributeShardsLiking other shard? Then fail miserably.
  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    moveShardFinish(false, true, "collection has been dropped in the meantime");
    return false;
  }
  auto const& collection =
      _snapshot.get(planColPrefix + _database + "/" + _collection);
  if (collection && collection->has("distributeShardsLike")) {
    moveShardFinish(
        false, false,
        "collection must not have 'distributeShardsLike' attribute");
    return false;
  }

  // Check that the shard is not locked:
  if (_snapshot.has(blockedShardsPrefix + _shard)) {
    LOG_TOPIC("0ae5a", DEBUG, Logger::SUPERVISION)
        << "shard " << _shard
        << " is currently locked, not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that the toServer is not locked:
  if (auto jobId = _snapshot.hasAsString(blockedServersPrefix + _to); jobId) {
    LOG_TOPIC("de054", DEBUG, Logger::SUPERVISION)
        << "server " << _to << " is currently locked by " << *jobId
        << ", not starting MoveShard job " << _jobId;
    return false;
  }

  // Check that the toServer is in state "GOOD":
  std::string health = checkServerHealth(_snapshot, _to);
  if (health != Supervision::HEALTH_STATUS_GOOD) {
    if (health == Supervision::HEALTH_STATUS_BAD) {
      LOG_TOPIC("de055", DEBUG, Logger::SUPERVISION)
          << "server " << _to << " is currently " << health
          << ", not starting MoveShard job " << _jobId;
      return false;
    } else {  // FAILED
      moveShardFinish(false, false, "toServer is FAILED");
      return false;
    }
  }

  // Check that _to is not in `Target/CleanedServers`:
  VPackBuilder cleanedServersBuilder;
  auto cleanedServersNode =
      _snapshot.hasAsBuilder(cleanedPrefix, cleanedServersBuilder);
  if (!cleanedServersNode) {
    // ignore this check
    cleanedServersBuilder.clear();
    { VPackArrayBuilder guard(&cleanedServersBuilder); }
  }
  VPackSlice cleanedServers = cleanedServersBuilder.slice();
  if (cleanedServers.isArray()) {
    for (VPackSlice x : VPackArrayIterator(cleanedServers)) {
      if (x.isString() && x.copyString() == _to) {
        moveShardFinish(false, false,
                        "toServer must not be in `Target/CleanedServers`");
        return false;
      }
    }
  }

  // Check that _to is not in `Target/FailedServers`:
  VPackBuilder failedServersBuilder;
  auto failedServersNode =
      _snapshot.hasAsBuilder(failedServersPrefix, failedServersBuilder);
  if (!failedServersNode) {
    // ignore this check
    failedServersBuilder.clear();
    { VPackObjectBuilder guard(&failedServersBuilder); }
  }
  VPackSlice failedServers = failedServersBuilder.slice();
  if (failedServers.isObject()) {
    Slice found = failedServers.get(_to);
    if (!found.isNone()) {
      moveShardFinish(false, false,
                      "toServer must not be in `Target/FailedServers`");
      return false;
    }
  }

  // REPLICATION 2 CHANGES HERE
  if (isReplication2Database(_database)) {
    return startReplication2();
  }

  // Look at Plan:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto plannedBuilder = _snapshot.hasAsBuilder(planPath).value();
  Slice planned = plannedBuilder.slice();

  int found = -1;
  int foundTo = -1;
  int count = 0;
  _toServerIsFollower = false;
  if (planned.isArray()) {
    for (VPackSlice srv : VPackArrayIterator(planned)) {
      TRI_ASSERT(srv.isString());
      if (srv.copyString() == _to) {
        foundTo = count;
        if (!_isLeader) {
          moveShardFinish(false, false,
                          "toServer must not be planned for a following shard");
          return false;
        } else {
          _toServerIsFollower = true;
        }
      }
      if (srv.copyString() == _from) {
        found = count;
      }
      ++count;
    }
  } else {
    // This is a serious problem. Under no circumstances should the shard
    // be without planned database server. This is a failure.
    moveShardFinish(
        false, false,
        std::string("shard ") + _shard + " has no planned database servers");
    TRI_ASSERT(false);
  }
  if ((_isLeader && found != 0) || (!_isLeader && found < 1)) {
    if (_isLeader) {
      moveShardFinish(false, false,
                      "fromServer must be the leader in plan for shard");
    } else {
      moveShardFinish(false, false,
                      "fromServer must be a follower in plan for shard");
    }
    return false;
  }

  // Compute group to move shards together:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  if (foundTo <
      0) {  // _to not in Plan, then it must not be a failoverCandidate:
    auto failoverCands =
        Job::findAllFailoverCandidates(_snapshot, _database, shardsLikeMe);
    if (failoverCands.find(_to) != failoverCands.end()) {
      finish("", "", false,
             "toServer must not be in failoverCandidates for shard or any of "
             "its distributeShardsLike colleagues");
      return false;
    }
  }

  if (!_isLeader) {
    if (_remainsFollower) {
      moveShardFinish(false, false,
                      "remainsFollower is invalid without isLeader");
      return false;
    }
  } else {
    if (_toServerIsFollower && !_remainsFollower) {
      moveShardFinish(
          false, false,
          "remainsFollower must be true if the toServer is a follower");
      return false;
    }
  }

  if (_isLeader && _toServerIsFollower) {
    // Further checks, before we can begin, we must make sure that the
    // _fromServer has accepted its leadership already for all shards in the
    // shard group and that the _toServer is actually in sync. Otherwise,
    // if this job here asks the leader to resign, we would be stuck.
    // If the _toServer is not in sync, the job would take overly long.
    bool ok = checkLeaderFollowerCurrent(shardsLikeMe);
    if (!ok) {
      return false;  // Do not start job, but leave it in Todo.
                     // Log messages already written.
    }
  }

  // Copy todo to pending
  Builder todo, pending;

  // Get todo entry
  {
    VPackArrayBuilder guard(&todo);
    // When create() was done with the current snapshot, then the job object
    // will not be in the snapshot under ToDo, but in this case we find it
    // in _jb:
    if (_jb == nullptr) {
      auto tmp_todo = _snapshot.hasAsBuilder(toDoPrefix + _jobId, todo);
      if (!tmp_todo) {
        // Just in case, this is never going to happen, since we will only
        // call the start() method if the job is already in ToDo.
        LOG_TOPIC("2482a", INFO, Logger::SUPERVISION)
            << "Failed to get key " << toDoPrefix << _jobId
            << " from agency snapshot";
        return false;
      }
    } else {
      try {
        todo.add(_jb->slice()[0].get(toDoPrefix + _jobId));
      } catch (std::exception const& e) {
        // Just in case, this is never going to happen, since when _jb is
        // set, then the current job is stored under ToDo.
        LOG_TOPIC("34af0", WARN, Logger::SUPERVISION) << e.what();
        return false;
      }
    }
  }

  // We do not want to honour `_tryUndo` if this is not a leader swap,
  // i.e. !_isLeader
  if (_tryUndo && !_toServerIsFollower) {
    // Unfortunately, we have to rewrite the `todo` velocypack now,
    // we keep everything, but we unset _tryUndo:
    _tryUndo = false;
    Builder todo2;
    TRI_ASSERT(todo.slice().isArray() && todo.slice()[0].isObject());
    todo2.add(todo.slice());
    todo.clear();  // Builder does not have a swap!
    {
      VPackArrayBuilder guard0(&todo);
      VPackObjectBuilder guard(&todo);
      for (auto p : VPackObjectIterator(todo2.slice()[0])) {
        std::string_view key = p.key.stringView();
        if (key != "tryUndo") {
          todo.add(key, p.value);
        } else {
          todo.add("tryUndo", VPackValue(false));
        }
      }
    }
  }

  // Enter pending, remove todo, block toserver
  {
    VPackArrayBuilder listOfTransactions(&pending);

    {
      VPackObjectBuilder objectForMutation(&pending);

      addPutJobIntoSomewhere(pending, "Pending", todo.slice()[0]);
      addRemoveJobFromSomewhere(pending, "ToDo", _jobId);

      addBlockShard(pending, _shard, _jobId);
      addMoveShardToServerLock(pending);
      addMoveShardFromServerLock(pending);

      bool failed = false;

      // --- Plan changes
      doForAllShards(_snapshot, _database, shardsLikeMe,
                     [this, &pending, &failed](Slice plan, Slice current,
                                               std::string& planPath,
                                               std::string& curPath) {
                       pending.add(VPackValue(planPath));
                       {
                         VPackArrayBuilder serverList(&pending);
                         if (_isLeader) {
                           TRI_ASSERT(plan[0].copyString() != _to);
                           pending.add(plan[0]);
                           if (!_toServerIsFollower) {
                             pending.add(VPackValue(_to));
                           }
                           for (size_t i = 1; i < plan.length(); ++i) {
                             pending.add(plan[i]);
                           }
                         } else {
                           if (plan.isArray()) {
                             for (VPackSlice srv : VPackArrayIterator(plan)) {
                               pending.add(srv);
                               TRI_ASSERT(srv.copyString() != _to);
                             }
                             pending.add(VPackValue(_to));
                           } else {
                             LOG_TOPIC("3b2fd", WARN, Logger::SUPERVISION)
                                 << "plan entry not an array while iterating "
                                    "over all shard clones";
                             TRI_ASSERT(false);
                             failed = true;
                             return;
                           }
                         }
                       }
                     });

      if (failed) {  // Cannot start the job - we'll be back.
        return false;
      }

      addIncreasePlanVersion(pending);

    }  // mutation part of transaction done

    // Preconditions
    {
      VPackObjectBuilder precondition(&pending);

      // --- Check that Planned servers are still as we expect
      addPreconditionUnchanged(pending, planPath, planned);
      // Check that failoverCandidates are still as we inspected them:
      doForAllShards(
          _snapshot, _database, shardsLikeMe,
          [this, &pending](Slice plan, Slice current, std::string& planPath,
                           std::string& curPath) {
            // take off "servers" from curPath and add
            // "failoverCandidates":
            std::string foCandsPath = curPath.substr(0, curPath.size() - 7);
            foCandsPath += StaticStrings::FailoverCandidates;
            auto foCands = this->_snapshot.hasAsBuilder(foCandsPath);
            if (foCands) {
              addPreconditionUnchanged(pending, foCandsPath, foCands->slice());
            }
          });
      addPreconditionShardNotBlocked(pending, _shard);
      addMoveShardToServerCanLock(pending);
      addMoveShardFromServerCanLock(pending);
      addPreconditionServerHealth(pending, _to,
                                  Supervision::HEALTH_STATUS_GOOD);
      addPreconditionUnchanged(pending, failedServersPrefix, failedServers);
      addPreconditionUnchanged(pending, cleanedPrefix, cleanedServers);
    }  // precondition done

  }  // array for transaction done

  // Transact to agency
  write_ret_t res = singleWriteTransaction(_agent, pending, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("45120", DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " << _shard << " from " << _from << " to "
        << _to;
    return true;
  }

  LOG_TOPIC("0a925", DEBUG, Logger::SUPERVISION)
      << "Start precondition failed for MoveShard job " << _jobId;
  return false;
}

bool MoveShard::startReplication2() {
  // do the following checks:
  // - check if _to is not part of the replicated state
  // - check if _from is part of the replicated state
  // then perform the following:
  // - remove _from from target
  // - add _to to target
  // - update the targetVersion
  // - if leader move shard, set leader, otherwise if _from is leader, clear
  // Preconditions:
  //  - target version is as expected
  using namespace replication2;

  // The old mapping between stateId and shard is still used in ClusterInfo,
  // hence the value_or alternative.
  auto stateId = getReplicatedStateId(_snapshot, _database, _collection, _shard)
                     .value_or(LogicalCollection::shardIdToStateId(_shard));

  auto targetPath = targetRepStatePrefix + _database + "/" + to_string(stateId);
  auto target = readStateTarget(_snapshot, _database, stateId).value();

  bool const containsTo = target.participants.contains(_to);
  bool const containsFrom = target.participants.contains(_from);

  // if not change-leader-only move shard
  if (not(containsTo and containsFrom and _isLeader)) {
    // update target
    if (containsTo) {
      moveShardFinish(false, false,
                      "toServer must not be a follower in plan for shard");
      return false;
    }

    if (not containsFrom) {
      moveShardFinish(false, false,
                      "fromServer must be a follower in plan for shard");
      return false;
    }

    target.participants.erase(_from);
    target.participants.emplace(_to, ParticipantFlags{});
  }

  auto oldTargetVersion = target.version;
  auto expected = target.version.value_or(1) + 1;
  target.version = expected;
  if (_isLeader) {
    target.leader = _to;
  } else if (target.leader == _from) {
    target.leader.reset();
  }

  // write back to agency
  auto oldJob = _snapshot.hasAsBuilder(toDoPrefix + _jobId).value();
  Builder newJob;
  {
    VPackObjectBuilder ob(&newJob);
    newJob.add(EXPECTED_TARGET_VERSION, VPackValue(expected));
    newJob.add(VPackObjectIterator(oldJob.slice()));
  }

  Builder trx;
  {
    VPackArrayBuilder a(&trx);
    {
      VPackObjectBuilder b(&trx);
      addPutJobIntoSomewhere(trx, "Pending", newJob.slice());
      addRemoveJobFromSomewhere(trx, "ToDo", _jobId);

      addBlockShard(trx, _shard, _jobId);
      addMoveShardToServerLock(trx);
      addMoveShardFromServerLock(trx);
      trx.add(VPackValue(targetPath));
      velocypack::serialize(trx, target);
    }
    {
      VPackObjectBuilder b(&trx);
      addPreconditionShardNotBlocked(trx, _shard);
      addMoveShardToServerCanLock(trx);
      addMoveShardFromServerCanLock(trx);
      addPreconditionServerHealth(trx, _to, Supervision::HEALTH_STATUS_GOOD);

      {
        VPackObjectBuilder ob(&trx, targetPath + "/version");
        if (oldTargetVersion) {
          trx.add("old", VPackValue(*oldTargetVersion));
        } else {
          trx.add("oldEmpty", VPackValue(true));
        }
      }
    }
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("4512d", DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " << _shard << " from " << _from << " to "
        << _to;
    return true;
  }

  LOG_TOPIC("0a92d", DEBUG, Logger::SUPERVISION)
      << "Start precondition failed for MoveShard job " << _jobId;
  return false;
}

JOB_STATUS MoveShard::status() {
  if (_status == PENDING || _status == TODO) {
    if (considerCancellation()) {
      return FAILED;
    }
  }

  if (_status != PENDING) {
    return _status;
  }

  // check that shard still there, otherwise finish job
  std::string planPath = planColPrefix + _database + "/" + _collection;
  if (!_snapshot.has(planPath)) {
    // Oops, collection is gone, simple finish job:
    moveShardFinish(true, true, "collection was dropped");
    return FINISHED;
  }

  // REPLICATION 2 WAIT FOR CURRENT VERSION
  if (isReplication2Database(_database)) {
    return pendingReplication2();
  }

  return (_isLeader) ? pendingLeader() : pendingFollower();
}

std::optional<std::uint64_t> MoveShard::getShardSupervisionVersion() {
  // read
  // arango/Current/ReplicatedLogs/<database>/<replicated-state-id>/supervision/targetVersion
  // The old mapping between stateId and shard is still used in ClusterInfo,
  // hence the value_or alternative.
  auto stateId = getReplicatedStateId(_snapshot, _database, _collection, _shard)
                     .value_or(LogicalCollection::shardIdToStateId(_shard));
  using namespace cluster::paths;
  auto path = aliases::current()
                  ->replicatedLogs()
                  ->database(_database)
                  ->log(stateId)
                  ->supervision()
                  ->targetVersion();
  return _snapshot.hasAsUInt(path->str(SkipComponents{1}));
}

JOB_STATUS MoveShard::pendingReplication2() {
  // check if Current/Supervision/Version is bigger or equal to
  // _expectedTargetVersion.
  // if so, move job to done.
  // TODO if it was a leader move shard, clear leader?

  //_snapshot.hasAsUInt();
  auto currentSupervisionVersion = getShardSupervisionVersion();
  if (currentSupervisionVersion < _expectedTargetVersion) {
    return PENDING;  // not yet converged
  }

  Builder trx;
  {
    VPackArrayBuilder a(&trx);
    {
      VPackObjectBuilder b(&trx);
      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      addMoveShardToServerUnLock(trx);
      addMoveShardFromServerUnLock(trx);
      Builder job;
      std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
      addReleaseShard(trx, _shard);
      if (_tryUndo) {
        addUndoMoveShard(trx, job);
      }
    }
    {
      VPackObjectBuilder b(&trx);
      addPreconditionCollectionStillThere(trx, _database, _collection);
      addMoveShardToServerCanUnLock(trx);
      addMoveShardFromServerCanUnLock(trx);
    }
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("f8c21", DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " << _shard << " from " << _from << " to "
        << _to;
    return FINISHED;
  }

  LOG_TOPIC("521eb", DEBUG, Logger::SUPERVISION)
      << "Precondition failed for MoveShard job " << _jobId;
  return PENDING;
}

JOB_STATUS MoveShard::pendingLeader() {
  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  // Consider next step, depending on who is currently the leader
  // in the Plan:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto planBuilder = _snapshot.hasAsBuilder(planPath).value();
  Slice plan = planBuilder.slice();
  Builder trx;
  Builder pre;  // precondition
  bool finishedAfterTransaction = false;

  // Check if any of the servers in the Plan are FAILED, if so,
  // we abort:
  if (plan.isArray() &&
      Job::countGoodOrBadServersInList(_snapshot, plan) < plan.length()) {
    LOG_TOPIC("de056", DEBUG, Logger::SUPERVISION)
        << "MoveShard (leader): found FAILED server in Plan, aborting job, "
           "db: "
        << _database << " coll: " << _collection << " shard: " << _shard;
    abort("failed server in Plan");
    return FAILED;
  }

  if (plan[0].copyString() == _from) {
    // Still the old leader, let's check that the toServer is insync:
    size_t done = 0;  // count the number of shards for which _to is in sync:
    doForAllShards(_snapshot, _database, shardsLikeMe,
                   [this, &done](Slice plan, Slice current,
                                 std::string& planPath, std::string& curPath) {
                     // If current of any shard clone is not an array, the
                     // collection is still being created, as we run. We will
                     // come back next round as we don't increment "done". And
                     // thus return PENDING.
                     if (current.isArray()) {  // No additional action need.
                                               // done is not incremented
                       for (VPackSlice s : VPackArrayIterator(current)) {
                         if (s.copyString() == _to) {
                           ++done;
                           return;
                         }
                       }
                     } else {
                       LOG_TOPIC("edfc7", WARN, Logger::SUPERVISION)
                           << "missing current entry for " << _shard
                           << " or a clone, we'll be back";
#ifndef ARANGODB_USE_GOOGLE_TESTS
                       TRI_ASSERT(false);
#endif
                     }
                   });

    // Consider cancellation:
    if (done < shardsLikeMe.size()) {
      return considerCancellation() ? FAILED : PENDING;
    }

    // We need to ask the old leader to retire:
    {
      bool failed = false;
      VPackArrayBuilder trxArray(&trx);
      {
        VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        doForAllShards(_snapshot, _database, shardsLikeMe,
                       [this, &trx, &pre, &failed](Slice plan, Slice current,
                                                   std::string& planPath,
                                                   std::string& curPath) {
                         // Replace _from by "_" + _from
                         trx.add(VPackValue(planPath));
                         {
                           VPackArrayBuilder guard(&trx);
                           if (plan.isArray()) {
                             for (VPackSlice srv : VPackArrayIterator(plan)) {
                               if (srv.copyString() == _from) {
                                 trx.add(VPackValue("_" + srv.copyString()));
                               } else {
                                 trx.add(srv);
                               }
                             }
                           } else {
                             LOG_TOPIC("98371", WARN, Logger::SUPERVISION)
                                 << "failed iterating over planned servers for "
                                 << _shard << " or a clone, retrying";
                             failed = true;
                             TRI_ASSERT(false);
                             return;
                           }
                         }
                         // Precondition: Plan still as it was
                         pre.add(VPackValue(planPath));
                         {
                           VPackObjectBuilder guard(&pre);
                           pre.add(VPackValue("old"));
                           pre.add(plan);
                         }
                       });
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addIncreasePlanVersion(trx);
        if (failed) {
          return PENDING;
        }
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
  } else if (plan[0].copyString() == "_" + _from) {
    // Retired old leader, let's check that the fromServer has retired:
    size_t done = 0;  // count the number of shards for which leader has retired
    doForAllShards(
        _snapshot, _database, shardsLikeMe,
        [this, &done](Slice plan, Slice current, std::string& planPath,
                      std::string& curPath) {
          if (current.length() > 0 && current[0].copyString() == "_" + _from) {
            ++done;
          }
        });

    // Consider cancellation:
    if (done < shardsLikeMe.size()) {
      return considerCancellation() ? FAILED : PENDING;
    }

    // We need to switch leaders:
    {
      // First make sure that the server we want to go to is still in
      // Current for all shards. This is important, since some transaction
      // which the leader has still executed before its resignation might
      // have dropped a follower for some shard, and this could have been
      // our new leader. In this case we must abort and go back to the
      // original leader, which is still perfectly safe.
      for (auto const& sh : shardsLikeMe) {
        auto const shardPath =
            curColPrefix + _database + "/" + sh.collection + "/" + sh.shard;
        auto const tmp = _snapshot.hasAsArray(shardPath + "/servers");
        if (tmp) {  // safe iterator below
          bool found = false;
          for (auto const& server : *tmp) {
            if (server.isEqualString(_to)) {
              found = true;
              break;
            }
          }

          if (!found) {
            // _to server no longer replica of this shard
            abort(_to + " no longer holds a replica of " + shardPath);
            return FAILED;
          }
        } else {
          // this shard is either gone or worse
          abort(shardPath + " no longer has replica");
          return FAILED;
        }
      }
      VPackArrayBuilder trxArray(&trx);
      {
        bool failed = false;
        VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        doForAllShards(_snapshot, _database, shardsLikeMe,
                       [this, &trx, &pre, &failed](Slice plan, Slice current,
                                                   std::string& planPath,
                                                   std::string& curPath) {
                         // Replace "_" + _from by _to and leave _from out:
                         trx.add(VPackValue(planPath));
                         {
                           VPackArrayBuilder guard(&trx);
                           if (plan.isArray()) {
                             for (VPackSlice srv : VPackArrayIterator(plan)) {
                               if (srv.copyString() == "_" + _from) {
                                 trx.add(VPackValue(_to));
                               } else if (srv.copyString() != _to) {
                                 trx.add(srv);
                               }
                             }
                           } else {
                             LOG_TOPIC("3249e", WARN, Logger::SUPERVISION)
                                 << "failed to iterate through planned shard "
                                    "servers for shard "
                                 << _shard << " or one of its clones";
                             failed = true;
                             TRI_ASSERT(false);
                             return;
                           }
                           // add the old leader as follower in case of a
                           // rollback
                           trx.add(VPackValue(_from));
                         }
                         // Precondition: Plan still as it was
                         pre.add(VPackValue(planPath));
                         {
                           VPackObjectBuilder guard(&pre);
                           pre.add(VPackValue("old"));
                           pre.add(plan);
                         }
                       });
        if (failed) {
          return PENDING;
        }
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addPreconditionCurrentReplicaShardGroup(pre, _database, shardsLikeMe,
                                                _to);
        addIncreasePlanVersion(trx);
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
  } else if (plan[0].copyString() == _to) {
    // New leader in Plan, let's check that it has assumed leadership and
    // all but except the old leader are in sync:
    size_t done = 0;
    doForAllShards(
        _snapshot, _database, shardsLikeMe,
        [this, &done](Slice plan, Slice current, std::string& planPath,
                      std::string& curPath) {
          if (current.length() > 0 && current[0].copyString() == _to) {
            if (plan.length() < 3) {
              // This only happens for replicationFactor == 1, in which case
              // there are exactly 2 servers in the Plan at this stage.
              // But then we do not have to wait for any follower to get in
              // sync.
              ++done;
            } else {
              // New leader has assumed leadership, now check all but
              // the old leader:
              size_t found = 0;
              for (size_t i = 1; i < plan.length() - 1; ++i) {
                VPackSlice p = plan[i];
                if (current.isArray()) {  // found is not incremented, we'll
                                          // remain pending
                  for (auto const& c : VPackArrayIterator(current)) {
                    if (arangodb::basics::VelocyPackHelper::equal(p, c, true)) {
                      ++found;
                      break;
                    }
                  }
                } else {
                  LOG_TOPIC("3294e", WARN, Logger::SUPERVISION)
                      << "failed to iterate through current shard servers "
                         "for shard "
                      << _shard << " or one of its clones";
                  TRI_ASSERT(false);
                  return;  // we don't increment done and remain PENDING
                }
              }
              if (found >= plan.length() - 2) {
                ++done;
              }
            }
          }
        });

    // Consider cancellation
    if (done < shardsLikeMe.size()) {
      return considerCancellation() ? FAILED : PENDING;
    }

    // We need to end the job, Plan remains unchanged:
    {
      VPackArrayBuilder trxArray(&trx);
      {
        VPackObjectBuilder trxObject(&trx);
        VPackObjectBuilder preObject(&pre);
        bool failed = false;
        doForAllShards(
            _snapshot, _database, shardsLikeMe,
            [&trx, &pre, this, &failed](Slice plan, Slice current,
                                        std::string& planPath,
                                        std::string& curPath) {
              if (!_remainsFollower) {
                // Remove _from from the list of follower
                trx.add(VPackValue(planPath));
                {
                  VPackArrayBuilder guard(&trx);
                  if (plan.isArray()) {
                    for (VPackSlice srv : VPackArrayIterator(plan)) {
                      if (!srv.isEqualString(_from)) {
                        trx.add(srv);
                      }
                    }
                  } else {
                    LOG_TOPIC("37714", WARN, Logger::SUPERVISION)
                        << "failed to iterate over planned servers for "
                        << _shard << " or one of its clones";
                    failed = true;
                    return;
                  }
                }
              }

              // Precondition: Plan still as it was
              pre.add(VPackValue(planPath));
              {
                VPackObjectBuilder guard(&pre);
                pre.add(VPackValue("old"));
                pre.add(plan);
              }
            });
        if (failed) {
          return PENDING;
        }
        if (!_remainsFollower) {
          addIncreasePlanVersion(trx);
        }
        addPreconditionCollectionStillThere(pre, _database, _collection);
        addRemoveJobFromSomewhere(trx, "Pending", _jobId);
        Builder job;
        std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
        addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
        addReleaseShard(trx, _shard);
        addMoveShardToServerUnLock(trx);
        addMoveShardFromServerUnLock(trx);
        addMoveShardToServerCanUnLock(pre);
        addMoveShardFromServerCanUnLock(pre);

        if (_tryUndo) {
          addUndoMoveShard(trx, job);
        }
      }
      // Add precondition to transaction:
      trx.add(pre.slice());
    }
    finishedAfterTransaction = true;
  } else {
    // something seriously wrong here, fail job:
    moveShardFinish(true, false, "something seriously wrong");
    return FAILED;
  }

  // Transact to agency:
  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    LOG_TOPIC("ffc21", DEBUG, Logger::SUPERVISION)
        << "Pending: Move shard " << _shard << " from " << _from << " to "
        << _to;
    return (finishedAfterTransaction ? FINISHED : PENDING);
  }

  LOG_TOPIC("52feb", DEBUG, Logger::SUPERVISION)
      << "Precondition failed for MoveShard job " << _jobId;
  return PENDING;
}

JOB_STATUS MoveShard::pendingFollower() {
  // Check if any of the servers in the Plan are FAILED, if so,
  // we abort:
  std::string planPath =
      planColPrefix + _database + "/" + _collection + "/shards/" + _shard;
  auto planBuilder = _snapshot.hasAsBuilder(planPath).value();
  Slice plan = planBuilder.slice();
  if (plan.isArray() &&
      Job::countGoodOrBadServersInList(_snapshot, plan) < plan.length()) {
    LOG_TOPIC("f8c22", DEBUG, Logger::SUPERVISION)
        << "MoveShard (follower): found FAILED server in Plan, aborting "
           "job, db: "
        << _database << " coll: " << _collection << " shard: " << _shard;
    abort("failed server in Plan");
    return FAILED;
  }

  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  size_t done = 0;  // count the number of shards done
  doForAllShards(_snapshot, _database, shardsLikeMe,
                 [&done](Slice plan, Slice current, std::string& planPath,
                         std::string& curPath) {
                   if (ClusterHelpers::compareServerLists(plan, current)) {
                     ++done;
                   }
                 });

  if (done < shardsLikeMe.size()) {
    return considerCancellation() ? FAILED : PENDING;
  }

  // All in sync, so move on and remove the fromServer, for all shards,
  // and in a single transaction:
  done = 0;     // count the number of shards done
  Builder trx;  // to build the transaction
  Builder precondition;
  {
    VPackArrayBuilder arrayForTransactionPair(&trx);
    {
      VPackObjectBuilder transactionObj(&trx);
      VPackObjectBuilder preconditionObj(&precondition);
      bool failed = false;
      // All changes to Plan for all shards, with precondition:
      doForAllShards(
          _snapshot, _database, shardsLikeMe,
          [this, &trx, &precondition, &failed](Slice plan, Slice current,
                                               std::string& planPath,
                                               std::string& curPath) {
            // Remove fromServer from Plan:
            trx.add(VPackValue(planPath));
            {
              VPackArrayBuilder guard(&trx);
              if (plan.isArray()) {
                for (VPackSlice srv : VPackArrayIterator(plan)) {
                  if (srv.copyString() != _from) {
                    trx.add(srv);
                  }
                }
              } else {
                LOG_TOPIC("dbc18", WARN, Logger::SUPERVISION)
                    << "failed to iterate over planned servers for shard "
                    << _shard << " or one of its followers, we'll be back";
                failed = true;
                TRI_ASSERT(false);
                return;
              }
            }
            // Precondition: Plan still as it was
            precondition.add(VPackValue(planPath));
            {
              VPackObjectBuilder guard(&precondition);
              precondition.add(VPackValue("old"));
              precondition.add(plan);
            }
          });
      if (failed) {
        return PENDING;
      }

      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Finished", job.slice(), "");
      addPreconditionCollectionStillThere(precondition, _database, _collection);
      addReleaseShard(trx, _shard);
      addMoveShardToServerUnLock(trx);
      addMoveShardFromServerUnLock(trx);
      addMoveShardToServerCanUnLock(precondition);
      addMoveShardFromServerCanUnLock(precondition);

      addIncreasePlanVersion(trx);
    }
    // Add precondition to transaction:
    trx.add(precondition.slice());
  }

  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (res.accepted && res.indices.size() == 1 && res.indices[0]) {
    return FINISHED;
  }

  return PENDING;
}

arangodb::Result MoveShard::abort(std::string const& reason) {
  arangodb::Result result;

  // We can assume that the job is either in ToDo or in Pending.
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    result = Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                    "Failed aborting moveShard beyond pending stage");
    return result;
  }

  // Can now only be TODO or PENDING.
  if (_status == TODO) {
    // Do NOT remove, just cause it seems obvious!
    // We're working off a snapshot.
    // Make sure ToDo is still actually to be done
    auto todoPrec = std::make_shared<Builder>();
    {
      VPackArrayBuilder b(todoPrec.get());
      { VPackObjectBuilder o(todoPrec.get()); }  // nothing to declare
      {
        VPackObjectBuilder path(
            todoPrec.get());  // expect jobs still to be sitting in ToDo
        todoPrec->add(VPackValue(toDoPrefix + _jobId));
        {
          VPackObjectBuilder guard(todoPrec.get());
          todoPrec->add("oldEmpty", VPackValue(false));
        }
      }
    }

    if (finish("", "", false, "job aborted (1): " + reason, todoPrec)) {
      return result;
    }
    _status = PENDING;
    // If the above finish failed, then we must be in PENDING
  }

  if (!_snapshot.has(planColPrefix + _database + "/" + _collection)) {
    moveShardFinish(true, false, "collection was dropped");
    return Result{};
  }

  // Can now only be PENDING
  // Find the other shards in the same distributeShardsLike group:
  std::vector<Job::shard_t> shardsLikeMe =
      clones(_snapshot, _database, _collection, _shard);

  // If we move the leader: Once the toServer has been put into the Plan
  // as leader, we always abort by moving forwards:
  // We can no longer abort by reverting to where we started, if any of the
  // shards of the distributeShardsLike group has already gone to new leader
  if (_isLeader) {
    auto const& plan = _snapshot.hasAsArray(planColPrefix + _database + "/" +
                                            _collection + "/shards/" + _shard);
    if (plan && plan->operator[](0).copyString() == _to) {
      LOG_TOPIC("72a82", INFO, Logger::SUPERVISION)
          << "MoveShard can no longer abort through reversion to where it "
             "started. Flight forward, leaving Plan as it is now.";
      moveShardFinish(
          true, false,
          "job aborted (2) - new leader already in place: " + reason);
      return result;
    }
  }

  Builder trx;  // to build the transaction

  // Now look after a PENDING job:
  {
    VPackArrayBuilder arrayForTransactionPair(&trx);
    {
      bool failed = false;
      VPackObjectBuilder transactionObj(&trx);
      if (_isLeader) {
        // All changes to Plan for all shards:
        doForAllShards(
            _snapshot, _database, shardsLikeMe,
            [this, &trx, &failed](Slice plan, Slice current,
                                  std::string& planPath, std::string& curPath) {
              // Restore leader to be _from:
              trx.add(VPackValue(planPath));
              {
                VPackArrayBuilder guard(&trx);
                trx.add(VPackValue(_from));
                if (plan.isArray()) {
                  for (VPackSlice srv : VPackArrayIterator(plan)) {
                    // from could be in plan as <from> or <_from>. Exclude
                    // to server always.
                    if (srv.isEqualString(_from) ||
                        srv.isEqualString("_" + _from) ||
                        srv.isEqualString(_to)) {
                      continue;
                    }
                    trx.add(srv);
                  }
                } else {
                  LOG_TOPIC("2e7b9", WARN, Logger::SUPERVISION)
                      << "failed to iterate over planned servers for shard "
                      << _shard << " or a clone";
                  failed = true;
                  TRI_ASSERT(false);
                  return;
                }
                // Add to server last. Will be removed by removeFollower if
                // too many.
                trx.add(VPackValue(_to));
              }
            });
      } else {
        // All changes to Plan for all shards:
        doForAllShards(
            _snapshot, _database, shardsLikeMe,
            [this, &trx, &failed](Slice plan, Slice current,
                                  std::string& planPath, std::string& curPath) {
              // Remove toServer from Plan:
              trx.add(VPackValue(planPath));
              {
                VPackArrayBuilder guard(&trx);
                if (plan.isArray()) {
                  for (VPackSlice srv : VPackArrayIterator(plan)) {
                    if (!srv.isEqualString(_to)) {
                      trx.add(srv);
                    }
                  }
                } else {
                  LOG_TOPIC("2eb79", WARN, Logger::SUPERVISION)
                      << "failed to iterate over planned servers for shard "
                      << _shard << " or a clone";
                  failed = true;
                  TRI_ASSERT(false);
                  return;
                }
              }
            });
      }
      if (failed) {
        result =
            Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                   std::string("no planned servers means a failure to abort ") +
                       _jobId);
        return result;
      }
      addRemoveJobFromSomewhere(trx, "Pending", _jobId);
      Builder job;
      std::ignore = _snapshot.hasAsBuilder(pendingPrefix + _jobId, job);
      addPutJobIntoSomewhere(trx, "Failed", job.slice(),
                             "job aborted (3): " + reason);
      addReleaseShard(trx, _shard);
      addMoveShardToServerUnLock(trx);
      addMoveShardFromServerUnLock(trx);
      addIncreasePlanVersion(trx);
    }
    {
      VPackObjectBuilder preconditionObj(&trx);
      addMoveShardToServerCanUnLock(trx);
      addMoveShardFromServerCanUnLock(trx);
      // If the collection is gone in the meantime, we do nothing here, but
      // the round will move the job to Finished anyway:
      addPreconditionCollectionStillThere(trx, _database, _collection);
    }
  }

  write_ret_t res = singleWriteTransaction(_agent, trx, false);

  if (!res.accepted) {
    result = Result(TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
                    std::string("Lost leadership"));
    return result;
  } else if (res.indices[0] == 0) {
    // Precondition failed
    if (_isLeader) {
      // Tough luck. Things have changed. We'll move on
      LOG_TOPIC("513e6", INFO, Logger::SUPERVISION)
          << "Precondition failed on MoveShard::abort() for shard " << _shard
          << " of collection " << _collection
          << ", if the collection has been deleted in the meantime, the "
             "job will be finished soon, if this message repeats, tell us.";
      result = Result(
          TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
          std::string("Precondition failed while aborting moveShard job ") +
              _jobId);
      return result;
      // We intentionally do not move the job object to Failed or Finished
      // here! The failed precondition can either be one of the read locks,
      // which suggests a fundamental problem, and in which case we will log
      // this message in every round of the supervision. Or the collection
      // has been dropped since we took the snapshot, in this case we will
      // move the job to Finished in the next round.
    }
    result = Result(
        TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
        std::string("Precondition failed while aborting moveShard job ") +
            _jobId);
  }

  return result;
}

void MoveShard::addMoveShardToServerLock(Builder& ops) const {
  addReadLockServer(ops, _to, _jobId);
}

void MoveShard::addMoveShardFromServerLock(Builder& ops) const {
  if (!isSubJob()) {
    addReadLockServer(ops, _from, _jobId);
  }
}

void MoveShard::addMoveShardToServerUnLock(Builder& ops) const {
  addReadUnlockServer(ops, _to, _jobId);
}

void MoveShard::addMoveShardToServerCanLock(Builder& precs) const {
  addPreconditionServerReadLockable(precs, _to, _jobId);
}

void MoveShard::addMoveShardFromServerCanLock(Builder& precs) const {
  if (isSubJob()) {
    addPreconditionServerWriteLocked(precs, _from, _parentJobId);
  } else {
    addPreconditionServerReadLockable(precs, _from, _jobId);
  }
}

void MoveShard::addMoveShardFromServerUnLock(Builder& ops) const {
  if (!isSubJob()) {
    addReadUnlockServer(ops, _from, _jobId);
  }
}

void MoveShard::addMoveShardToServerCanUnLock(Builder& ops) const {
  addPreconditionServerReadLocked(ops, _to, _jobId);
}

void MoveShard::addMoveShardFromServerCanUnLock(Builder& ops) const {
  if (!isSubJob()) {
    addPreconditionServerReadLocked(ops, _from, _jobId);
  }
}

bool MoveShard::moveShardFinish(bool unlock, bool success,
                                std::string const& msg) {
  std::shared_ptr<VPackBuilder> payload;

  if (unlock) {
    payload = std::make_shared<VPackBuilder>();
    {
      VPackArrayBuilder env(payload.get());
      {
        VPackObjectBuilder trx(payload.get());
        addReleaseShard(*payload, _shard);
        addMoveShardToServerUnLock(*payload);
        addMoveShardFromServerUnLock(*payload);
      }
      {
        VPackObjectBuilder precs(payload.get());
        addMoveShardFromServerCanUnLock(*payload);
        addMoveShardToServerCanUnLock(*payload);
      }
    }
  }

  return finish("", "", success, msg, std::move(payload));
}

void MoveShard::addUndoMoveShard(Builder& ops, Builder const& job) const {
  // If we are here, we have `_isLeader` set to `true`. We also know
  // that this was a leader swap with an in sync follower, so we can
  // assume that.
  std::string path = returnLeadershipPrefix + _shard;
  // Briefly check that the place in Target is still empty:
  if (_snapshot.has(path)) {
    // This should not happen, since we have a lock on the `_toServer`.
    LOG_TOPIC("abbcc", WARN, Logger::SUPERVISION)
        << "failed to schedule undo job for shard " << _shard
        << " since there was already one.";
    return;
  }
  std::string now(timepointToString(std::chrono::system_clock::now()));
  std::string deadline(timepointToString(std::chrono::system_clock::now() +
                                         std::chrono::minutes(20)));
  auto rebootId = _snapshot.hasAsUInt(basics::StringUtils::concatT(
      curServersKnown, _from, "/", StaticStrings::RebootId));
  if (!rebootId) {
    // This should not happen, since we should have a rebootId for this.
    LOG_TOPIC("abbcd", WARN, Logger::SUPERVISION)
        << "failed to schedule undo job for shard " << _shard
        << " since there was no rebootId for server " << _from;
    return;
  }
  ops.add(VPackValue(path));
  {
    VPackObjectBuilder guard(&ops);
    ops.add("timeStamp", VPackValue(now));
    ops.add("removeIfNotStartedBy", VPackValue(deadline));
    ops.add("rebootId", VPackValue(rebootId.value()));
    ops.add("moveShard", job.slice());
  }
}
