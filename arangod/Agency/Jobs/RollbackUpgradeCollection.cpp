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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "Agency/Jobs/RollbackUpgradeCollection.h"

#include "Agency/Agent.h"
#include "Agency/Job.h"
#include "Agency/JobContext.h"
#include "Basics/StaticStrings.h"
#include "Cluster/AgencyPaths.h"
#include "Cluster/Maintenance/MaintenanceStrings.h"
#include "VocBase/LogicalCollection.h"

namespace {
using UpgradeState = arangodb::LogicalCollection::UpgradeStatus::State;

bool preparePendingJob(arangodb::velocypack::Builder& job, std::string const& jobId,
                       arangodb::consensus::Node const& snapshot) {
  using arangodb::consensus::toDoPrefix;

  auto [garbage, found] = snapshot.hasAsBuilder(toDoPrefix + jobId, job);
  if (!found) {
    // Just in case, this is never going to happen, since we will only
    // call the start() method if the job is already in ToDo.
    LOG_TOPIC("2483b", INFO, arangodb::Logger::SUPERVISION)
        << "Failed to get key " + toDoPrefix + jobId + " from agency snapshot";
    return false;
  }

  return true;
}

void prepareStartTransaction(arangodb::velocypack::Builder& trx,
                             arangodb::consensus::Node const& snapshot,
                             std::string const& database, std::string const& collection,
                             std::string const& jobId, std::string const& failedId,
                             arangodb::velocypack::Slice toDoJob) {
  using namespace arangodb::velocypack;

  std::string collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;

  auto [holder, found] = snapshot.hasAsString(collectionLock);
  bool mustLock = !found || (holder != jobId && holder != failedId);
  bool stealLock = holder == failedId;

  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      if (mustLock) {
        // lock collection
        trx.add(collectionLock, Value(jobId));
      }

      // and add the upgrade flag
      trx.add(collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS,
              arangodb::LogicalCollection::UpgradeStatus::stateToValue(::UpgradeState::Prepare));

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);

      // then move job from todo to pending
      arangodb::consensus::Job::addPutJobIntoSomewhere(trx, "Pending", toDoJob);
      arangodb::consensus::Job::addRemoveJobFromSomewhere(trx, "ToDo", jobId);
    }
    {
      ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(Value(collectionPath));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }

      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        if (mustLock && stealLock) {
          // failed job had it locked, we should steal it
          trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(failedId));
        } else if (mustLock) {
          // and we can write lock it
          trx.add(arangodb::consensus::PREC_CAN_WRITE_LOCK, Value(true));
        } else {
          // we already have it locked
          trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(jobId));
        }
      }
    }
  }
}

::UpgradeState getTargetPhase(arangodb::consensus::Node const& snapshot,
                              std::string const& database, std::string const& collection) {
  std::string const path = "/Plan/Collections/" + database + "/" + collection +
                           "/" + arangodb::maintenance::UPGRADE_STATUS;
  arangodb::velocypack::Builder builder;
  auto [garbage, found] = snapshot.hasAsBuilder(path, builder);
  if (found) {
    return arangodb::LogicalCollection::UpgradeStatus::stateFromSlice(builder.slice());
  }

  return ::UpgradeState::ToDo;
}

std::pair<bool, bool> checkShard(arangodb::consensus::Node const& snapshot,
                                 std::string const& database,
                                 std::string const& collection, std::string const& shard,
                                 std::unordered_set<std::string> const& plannedServers,
                                 ::UpgradeState targetPhase, std::string& errorMessage) {
  using namespace arangodb::velocypack;
  using UpgradeStatus = arangodb::LogicalCollection::UpgradeStatus;

  bool allMatch = true;
  bool haveError = false;

  std::string const statusPath = "/Current/Collections/" + database + "/" +
                                 collection + "/" + shard + "/" +
                                 arangodb::maintenance::UPGRADE_STATUS;
  Builder builder;
  auto [garbage, found] = snapshot.hasAsBuilder(statusPath, builder);
  if (found && !builder.slice().isObject()) {
    haveError = true;
  } else if (!found || builder.slice().isNone()) {
    allMatch = false;
  } else {
    UpgradeStatus status;
    std::tie(status, haveError) = UpgradeStatus::fromSlice(builder.slice());
    if (!haveError) {
      if (!status.errorMessage().empty()) {
        haveError = true;
        errorMessage = status.errorMessage();
      } else {
        UpgradeStatus::Map const& map = status.map();
        Builder b;
        {
          ObjectBuilder g(&b);
          status.toVelocyPack(b, false);
        }
        for (std::string const& server : plannedServers) {
          UpgradeStatus::Map::const_iterator it = map.find(server);
          if (it == map.end() || it->second != targetPhase) {
            allMatch = false;
            break;
          }
        }
      }
    }
  }

  return std::make_pair(allMatch, haveError);
}

std::pair<bool, bool> checkAllShards(arangodb::consensus::Node const& snapshot,
                                     std::string const& database, std::string const& collection,
                                     ::UpgradeState targetPhase, std::string& errorMessage) {
  using namespace arangodb::velocypack;

  bool allMatch = true;
  bool haveError = false;

  std::string const shardsPath = "/Plan/Collections/" + database + "/" +
                                 collection + "/" + arangodb::maintenance::SHARDS;
  Builder builder;
  auto [gargate, found] = snapshot.hasAsBuilder(shardsPath, builder);
  if (!found || !builder.slice().isObject()) {
    haveError = true;
  } else {
    for (ObjectIteratorPair shardPair : ObjectIterator(builder.slice())) {
      if (!shardPair.key.isString() || !shardPair.value.isArray()) {
        haveError = true;
        break;
      }
      std::string shard = shardPair.key.copyString();
      std::unordered_set<std::string> plannedServers;
      for (Slice server : ArrayIterator(shardPair.value)) {
        if (server.isString()) {
          plannedServers.emplace(server.copyString());
        } else {
          haveError = true;
          break;
        }
      }
      if (haveError) {
        break;
      }
      std::tie(allMatch, haveError) =
          ::checkShard(snapshot, database, collection, shard, plannedServers,
                       targetPhase, errorMessage);
      if (haveError || !allMatch) {
        break;
      }
    }
  }

  TRI_IF_FAILURE("RollbackUpgradeCollection::HaveShardError") {
    haveError = true;
  }

  return std::make_pair(allMatch, haveError);
}

std::pair<bool, bool> haveFinalizedInShard(arangodb::consensus::Node const& snapshot,
                                           std::string const& database,
                                           std::string const& collection,
                                           std::string const& shard,
                                           std::unordered_set<std::string> const& plannedServers) {
  using namespace arangodb::velocypack;
  using UpgradeStatus = arangodb::LogicalCollection::UpgradeStatus;

  bool haveFinalized = false;
  bool haveError = false;

  std::string const statusPath = "/Current/Collections/" + database + "/" +
                                 collection + "/" + shard + "/" +
                                 arangodb::maintenance::UPGRADE_STATUS;
  Builder builder;
  auto [garbage, found] = snapshot.hasAsBuilder(statusPath, builder);
  if (found && !builder.slice().isObject()) {
    haveError = true;
    LOG_DEVEL << "D";
  } else if (!found || builder.slice().isNone()) {
    haveFinalized = false;
  } else {
    UpgradeStatus status;
    std::tie(status, haveError) = UpgradeStatus::fromSlice(builder.slice());
    if (!haveError) {
      UpgradeStatus::Map const& map = status.map();
      for (std::string const& server : plannedServers) {
        UpgradeStatus::Map::const_iterator it = map.find(server);
        if (it != map.end() && it->second != ::UpgradeState::Finalize) {
          haveFinalized = true;
          break;
        }
      }
    } else {
      LOG_DEVEL << "F";
    }
  }

  return std::make_pair(haveFinalized, haveError);
}

std::pair<bool, bool> haveAnyFinalized(arangodb::consensus::Node const& snapshot,
                                       std::string const& database,
                                       std::string const& collection) {
  using namespace arangodb::velocypack;

  bool haveFinalized = false;
  bool haveError = false;

  std::string const shardsPath = "/Plan/Collections/" + database + "/" +
                                 collection + "/" + arangodb::maintenance::SHARDS;
  Builder builder;
  auto [garbage, found] = snapshot.hasAsBuilder(shardsPath, builder);
  if (!found || !builder.slice().isObject()) {
    haveError = true;
    LOG_DEVEL << "A";
  } else {
    for (ObjectIteratorPair shardPair : ObjectIterator(builder.slice())) {
      if (!shardPair.key.isString() || !shardPair.value.isArray()) {
        haveError = true;
        LOG_DEVEL << "B";
        break;
      }
      std::string shard = shardPair.key.copyString();
      std::unordered_set<std::string> plannedServers;
      for (Slice server : ArrayIterator(shardPair.value)) {
        if (server.isString()) {
          plannedServers.emplace(server.copyString());
        } else {
          haveError = true;
          LOG_DEVEL << "C";
          break;
        }
      }
      if (haveError) {
        break;
      }
      std::tie(haveFinalized, haveError) =
          ::haveFinalizedInShard(snapshot, database, collection, shard, plannedServers);
      if (haveError || haveFinalized) {
        break;
      }
    }
  }

  return std::make_pair(haveFinalized, haveError);
}

void prepareSetTargetPhaseTransaction(arangodb::velocypack::Builder& trx,
                                      std::string const& database,
                                      std::string const& collection,
                                      std::string const& jobId, ::UpgradeState targetPhase) {
  using namespace arangodb::velocypack;

  std::string collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string collectionStatus = collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS;
  std::string collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // and add the upgrade flag
      trx.add(collectionStatus,
              arangodb::LogicalCollection::UpgradeStatus::stateToValue(targetPhase));

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);
    }
    {
      ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(Value(collectionPath));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }

      // and we have it write locked
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(jobId));
      }
    }
  }
}

void prepareReleaseTransaction(arangodb::velocypack::Builder& trx,
                               arangodb::consensus::Node const& snapshot,
                               std::string const& database, std::string const& collection,
                               std::string const& jobId) {
  using namespace arangodb::velocypack;

  std::string collectionPath = "/Plan/Collections/" + database + "/" + collection;
  std::string collectionStatus = collectionPath + "/" + arangodb::maintenance::UPGRADE_STATUS;
  std::string collectionLock = collectionPath + "/" + arangodb::maintenance::LOCK;
  {
    ArrayBuilder listofTransactions(&trx);
    {
      ObjectBuilder mutations(&trx);

      // remove the upgrade flag
      trx.add(Value(collectionStatus));
      {
        ObjectBuilder status(&trx);
        trx.add("op", Value("delete"));
      }

      std::string currentPath = "/Current/Collections/" + database + "/" + collection;
      Builder builder;
      auto [garbage, found] = snapshot.hasAsBuilder(currentPath, builder);
      if (found && builder.slice().isObject()) {
        for (ObjectIteratorPair pair : ObjectIterator(builder.slice())) {
          if (pair.value.isObject()) {
            Slice status = pair.value.get(arangodb::maintenance::UPGRADE_STATUS);
            if (!status.isNone()) {
              std::string const statusPath =
                  currentPath + "/" + pair.key.copyString() + "/" +
                  arangodb::maintenance::UPGRADE_STATUS;
              trx.add(Value(statusPath));
              {
                ObjectBuilder status(&trx);
                trx.add("op", Value("delete"));
              }
            }
          }
        }
      }

      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add("op", Value("delete"));
      }

      // make sure we don't try to rewrite history
      arangodb::consensus::Job::addIncreasePlanVersion(trx);
      arangodb::consensus::Job::addIncreaseCurrentVersion(trx);
    }
    {
      ObjectBuilder preconditions(&trx);

      // collection exists
      trx.add(Value(collectionPath));
      {
        ObjectBuilder collection(&trx);
        trx.add("oldEmpty", Value(false));
      }

      // and we have it write locked
      trx.add(Value(collectionLock));
      {
        ObjectBuilder lock(&trx);
        trx.add(arangodb::consensus::PREC_IS_WRITE_LOCKED, Value(jobId));
      }
    }
  }
}
}  // namespace

namespace arangodb::consensus {

RollbackUpgradeCollection::RollbackUpgradeCollection(Supervision& supervision,
                                                     Node const& snapshot,
                                                     AgentInterface* agent, JOB_STATUS status,
                                                     std::string const& jobId)
    : Job(supervision, status, snapshot, agent, jobId) {
  // Get job details from agency:
  std::string path = pos[status] + _jobId + "/";
  auto [tmpDatabase, foundDatabase] = _snapshot.hasAsString(path + "database");
  auto [tmpCollection, foundCollection] =
      _snapshot.hasAsString(path + "collection");
  auto [tmpFailed, foundFailed] = _snapshot.hasAsString(path + "failedId");

  auto [tmpCreator, foundCreator] = _snapshot.hasAsString(path + "creator");
  auto [tmpCreated, foundCreated] = _snapshot.hasAsString(path + "timeCreated");

  auto [tmpChild, childFound] = _snapshot.hasAsBool(path + StaticStrings::IsSmartChild);

  if (foundDatabase && foundCollection && foundFailed && foundCreator && foundCreated) {
    _database = tmpDatabase;
    _collection = tmpCollection;
    _failedId = tmpFailed;
    _creator = tmpCreator;
    _created = stringToTimepoint(tmpCreated);
  } else {
    std::stringstream err;
    err << "Failed to find job " << _jobId << " in agency";
    LOG_TOPIC("4669d", ERR, Logger::SUPERVISION) << err.str();
    finish("", "", false, err.str());
    _status = FAILED;
  }

  _smartChild = childFound && tmpChild;

  _started = std::chrono::steady_clock::now();
}

RollbackUpgradeCollection::~RollbackUpgradeCollection() = default;

void RollbackUpgradeCollection::run(bool& aborts) { runHelper("", "", aborts); }

bool RollbackUpgradeCollection::create(std::shared_ptr<VPackBuilder> envelope) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "create not implemented for RollbackUpgradeCollection");
  return false;
}

bool RollbackUpgradeCollection::start(bool& aborts) {
  using namespace cluster::paths::aliases;

  velocypack::Builder pending;
  bool success = ::preparePendingJob(pending, _jobId, _snapshot);
  if (!success) {
    abort("could not retrieve job info");
    return false;
  }

  velocypack::Builder trx;
  ::prepareStartTransaction(trx, _snapshot, _database, _collection, _jobId,
                            _failedId, pending.slice());

  std::string messageIfError =
      "could not begin to rollback upgrade of collection '" + _collection + "'";

  TRI_IF_FAILURE("RollbackUpgradeCollection::StartJobTransaction") {
    abort(messageIfError);
    return false;
  }

  bool ok = writeTransaction(trx, messageIfError);
  if (ok) {
    _status = PENDING;
    LOG_TOPIC("45131", DEBUG, Logger::SUPERVISION)
        << "Pending: Rollback upgrade of collection '" + _collection + "'";

    auto [someFinalized, haveShardError] =
        ::haveAnyFinalized(_snapshot, _database, _collection);
    if (haveShardError) {
      abort("have a shard error, could not process rollback");
    }

    trx.clear();
    ::UpgradeState attemptedPhase = ::getTargetPhase(_snapshot, _database, _collection);
    ::UpgradeState targetPhase = (someFinalized && attemptedPhase != ::UpgradeState::Cleanup)
                                     ? ::UpgradeState::Rollback
                                     : ::UpgradeState::Cleanup;
    ::prepareSetTargetPhaseTransaction(trx, _database, _collection, _jobId, targetPhase);

    messageIfError =
        "could not set target phase correctly to rollback upgrade of "
        "collection '" +
        _collection + "'";

    TRI_IF_FAILURE("RollbackUpgradeCollection::SetInitialPhaseTransaction") {
      abort(messageIfError);
      return false;
    }

    ok = writeTransaction(trx, messageIfError);
    if (ok) {
      return true;
    }
  }

  return false;
}

JOB_STATUS RollbackUpgradeCollection::status() {
  if (_status != PENDING) {
    // either not started yet, or already failed/finished
    return _status;
  }

  ::UpgradeState targetPhase = ::getTargetPhase(_snapshot, _database, _collection);
  std::string errorMessage;
  auto [allMatch, haveShardError] =
      ::checkAllShards(_snapshot, _database, _collection, targetPhase, errorMessage);
  if (haveShardError) {
    if ((std::chrono::steady_clock::now() - _started) > std::chrono::seconds(120)) {
      // only abort if we've given the DBServers two minutes to clear out old
      // errors and react to the rollback request
      abort(errorMessage);
    }
  } else if (allMatch) {
    // move to next phase, or report finished if done
    if (targetPhase == ::UpgradeState::Rollback) {
      velocypack::Builder trx;
      ::prepareSetTargetPhaseTransaction(trx, _database, _collection, _jobId,
                                         ::UpgradeState::Cleanup);
      std::string messageIfError = "could not set target phase 'Cleanup'";
      TRI_IF_FAILURE("RollbackUpgradeCollection::SetCleanupTransaction") {
        abort(messageIfError);
        return _status;
      }
      [[maybe_unused]] bool ok = writeTransaction(trx, messageIfError);
    } else if (targetPhase == ::UpgradeState::Cleanup) {
      velocypack::Builder trx;
      ::prepareReleaseTransaction(trx, _snapshot, _database, _collection, _jobId);
      std::string messageIfError = "could not clean up old data after upgrade";
      TRI_IF_FAILURE("RollbackUpgradeCollection::ReleaseTransaction") {
        abort(messageIfError);
        return _status;
      }
      [[maybe_unused]] bool ok = writeTransaction(trx, messageIfError);
      if (ok) {
        finish("", "", true);
      }
    }
  }

  return _status;
}

arangodb::Result RollbackUpgradeCollection::abort(std::string const& reason) {
  // We can assume that the job is in ToDo or not there:
  if (_status == NOTFOUND || _status == FINISHED || _status == FAILED) {
    return Result(
        TRI_ERROR_SUPERVISION_GENERAL_FAILURE,
        "Failed aborting RollbackUpgradeCollection job beyond pending stage");
  }

  finish("", "", false, "job aborted: " + reason);
  return Result{};
}

std::string RollbackUpgradeCollection::jobPrefix() const {
  return _status == TODO ? toDoPrefix : pendingPrefix;
}

velocypack::Slice RollbackUpgradeCollection::job() const {
  if (_jb == nullptr) {
    return velocypack::Slice::noneSlice();
  } else {
    return _jb->slice()[0].get(jobPrefix() + _jobId);
  }
}

bool RollbackUpgradeCollection::writeTransaction(velocypack::Builder const& trx,
                                                 std::string const& errorMessage) {
  write_ret_t res = singleWriteTransaction(_agent, trx, true);
  if (!res.accepted || res.indices.size() != 1 || !res.indices[0]) {
    abort(errorMessage);
    return false;
  }
  return true;
}

}  // namespace arangodb::consensus