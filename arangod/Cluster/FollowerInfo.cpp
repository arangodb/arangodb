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
/// @author Max Neunhoeffer
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "FollowerInfo.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"

#include "Basics/ScopeGuard.h"

using namespace arangodb;

static std::string const inline reportName(bool isRemove) {
  if (isRemove) {
    return "FollowerInfo::remove";
  } else {
    return "FollowerInfo::add";
  }
}

static std::string CurrentShardPath(arangodb::LogicalCollection& col) {
  // Agency path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  return "Current/Collections/" + col.vocbase().name() + "/" +
         std::to_string(col.planId()) + "/" + col.name();
}

static VPackSlice CurrentShardEntry(arangodb::LogicalCollection& col, VPackSlice current) {
  return current.get(std::vector<std::string>(
      {AgencyCommManager::path(), "Current", "Collections",
       col.vocbase().name(), std::to_string(col.planId()), col.name()}));
}

static std::string PlanShardPath(arangodb::LogicalCollection& col) {
  return "Plan/Collections/" + col.vocbase().name() + "/" +
         std::to_string(col.planId()) + "/shards/" + col.name();
}

static VPackSlice PlanShardEntry(arangodb::LogicalCollection& col, VPackSlice plan) {
  return plan.get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Collections", col.vocbase().name(),
       std::to_string(col.planId()), "shards", col.name()}));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower to a shard, this is only done by the server side
/// of the "get-in-sync" capabilities. This reports to the agency under
/// `/Current` but in asynchronous "fire-and-forget" way.
////////////////////////////////////////////////////////////////////////////////

Result FollowerInfo::add(ServerID const& sid) {
  TRI_IF_FAILURE("FollowerInfo::add") {
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED,
            "unable to add follower"};
  }

  MUTEX_LOCKER(locker, _agencyMutex);

  std::shared_ptr<std::vector<ServerID>> v;

  {
    WRITE_LOCKER(writeLocker, _dataLock);
    // First check if there is anything to do:
    for (auto const& s : *_followers) {
      if (s == sid) {
        return {TRI_ERROR_NO_ERROR};  // Do nothing, if follower already there
      }
    }
    // Fully copy the vector:
    v = std::make_shared<std::vector<ServerID>>(*_followers);
    v->push_back(sid);  // add a single entry
    _followers = v;     // will cast to std::vector<ServerID> const
    {
      // insertIntoCandidates
      if (std::find(_failoverCandidates->begin(), _failoverCandidates->end(), sid) ==
          _failoverCandidates->end()) {
        auto nextCandidates = std::make_shared<std::vector<ServerID>>(*_failoverCandidates);
        nextCandidates->push_back(sid);  // add a single entry
        _failoverCandidates = nextCandidates;  // will cast to std::vector<ServerID> const
      }
    }
#ifdef DEBUG_SYNC_REPLICATION
    if (!AgencyCommManager::MANAGER) {
      return {TRI_ERROR_NO_ERROR};
    }
#endif
  }

  // Now tell the agency
  auto agencyRes = persistInAgency(false);
  if (agencyRes.ok() || agencyRes.is(TRI_ERROR_CLUSTER_NOT_LEADER)) {
    // Not a leader is expected
    return agencyRes;
  }
  // Real error, report

  std::string errorMessage =
      "unable to add follower in agency, timeout in agency CAS operation for "
      "key " +
      _docColl->vocbase().name() + "/" + std::to_string(_docColl->planId()) +
      ": " + TRI_errno_string(agencyRes.errorNumber());
  LOG_TOPIC("6295b", ERR, Logger::CLUSTER) << errorMessage;
  agencyRes.reset(agencyRes.errorNumber(), std::move(errorMessage));
  return agencyRes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower from a shard, this is only done by the
/// server if a synchronous replication request fails. This reports to
/// the agency under `/Current`. This method can fail, which is critical,
/// because we cannot drop a follower ourselves and not report this to the
/// agency, since then a failover to a not-in-sync follower might happen.
/// way. The method fails silently, if the follower information has
/// since been dropped (see `dropFollowerInfo` below).
////////////////////////////////////////////////////////////////////////////////

Result FollowerInfo::remove(ServerID const& sid) {
  TRI_IF_FAILURE("FollowerInfo::remove") {
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED,
            "unable to remove follower"};
  }

  if (application_features::ApplicationServer::isStopping()) {
    // If we are already shutting down, we cannot be trusted any more with
    // such an important decision like dropping a follower.
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  LOG_TOPIC("ce460", DEBUG, Logger::CLUSTER)
      << "Removing follower " << sid << " from " << _docColl->name();

  MUTEX_LOCKER(locker, _agencyMutex);
  WRITE_LOCKER(canWriteLocker, _canWriteLock);
  WRITE_LOCKER(writeLocker, _dataLock);  // the data lock has to be locked until this function completes
                                         // because if the agency communication does not work
                                         // local data is modified again.

  // First check if there is anything to do:
  if (std::find(_followers->begin(), _followers->end(), sid) == _followers->end()) {
    TRI_ASSERT(std::find(_failoverCandidates->begin(), _failoverCandidates->end(),
                         sid) == _failoverCandidates->end());
    return {TRI_ERROR_NO_ERROR};  // nothing to do
  }
  // Both lists have to be in sync at any time!
  TRI_ASSERT(std::find(_failoverCandidates->begin(), _failoverCandidates->end(),
                       sid) != _failoverCandidates->end());
  auto oldFollowers = _followers;
  auto oldFailovers = _failoverCandidates;
  {
    auto v = std::make_shared<std::vector<ServerID>>();
    TRI_ASSERT(!_followers->empty());  // well we found the element above \o/
    v->reserve(_followers->size() - 1);
    std::remove_copy(_followers->begin(), _followers->end(),
                     std::back_inserter(*v.get()), sid);
    _followers = v;  // will cast to std::vector<ServerID> const
  }
  {
    auto v = std::make_shared<std::vector<ServerID>>();
    TRI_ASSERT(!_failoverCandidates->empty());  // well we found the element above \o/
    v->reserve(_failoverCandidates->size() - 1);
    std::remove_copy(_failoverCandidates->begin(), _failoverCandidates->end(),
                     std::back_inserter(*v.get()), sid);
    _failoverCandidates = v;  // will cast to std::vector<ServerID> const
  }
#ifdef DEBUG_SYNC_REPLICATION
  if (!AgencyCommManager::MANAGER) {
    return {TRI_ERROR_NO_ERROR};
  }
#endif
  Result agencyRes = persistInAgency(true);
  if (agencyRes.ok()) {
    // +1 for the leader (me)
    if (_followers->size() + 1 < _docColl->minReplicationFactor()) {
      _canWrite = false;
    }
    // we are finished
    LOG_TOPIC("be0cb", DEBUG, Logger::CLUSTER)
        << "Removing follower " << sid << " from " << _docColl->name() << "succeeded";
    return agencyRes;
  }
  if (agencyRes.is(TRI_ERROR_CLUSTER_NOT_LEADER)) {
    // Next run in Maintenance will fix this.
    return agencyRes;
  }

  // rollback:
  _followers = oldFollowers;
  _failoverCandidates = oldFailovers;
  std::string errorMessage =
      "unable to remove follower from agency, timeout in agency CAS operation "
      "for key " +
      _docColl->vocbase().name() + "/" + std::to_string(_docColl->planId()) +
      ": " + TRI_errno_string(agencyRes.errorNumber());
  LOG_TOPIC("a0dcc", ERR, Logger::CLUSTER) << errorMessage;
  agencyRes.resetErrorMessage<std::string>(std::move(errorMessage));
  return agencyRes;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief clear follower list, no changes in agency necessary
//////////////////////////////////////////////////////////////////////////////

void FollowerInfo::clear() {
  WRITE_LOCKER(canWriteLocker, _canWriteLock);
  WRITE_LOCKER(writeLocker, _dataLock);
  _followers = std::make_shared<std::vector<ServerID>>();
  _failoverCandidates = std::make_shared<std::vector<ServerID>>();
  _canWrite = false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief check whether the given server is a follower
//////////////////////////////////////////////////////////////////////////////

bool FollowerInfo::contains(ServerID const& sid) const {
  READ_LOCKER(readLocker, _dataLock);
  auto const& f = *_followers;
  return std::find(f.begin(), f.end(), sid) != f.end();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Take over leadership for this shard.
///        Also inject information of a insync followers that we knew about
///        before a failover to this server has happened
////////////////////////////////////////////////////////////////////////////////

void FollowerInfo::takeOverLeadership(std::vector<std::string> const& previousInsyncFollowers) {
  // This function copies over the information taken from the last CURRENT into a local vector.
  // Where we remove the old leader and ourself from the list of followers
  WRITE_LOCKER(canWriteLocker, _canWriteLock);
  WRITE_LOCKER(writeLocker, _dataLock);
  // Reset local structures, if we take over leadership we do not know anything!
  _followers = std::make_shared<std::vector<ServerID>>();
  _failoverCandidates = std::make_shared<std::vector<ServerID>>();
  // We disallow writes until the first write.
  _canWrite = false;
  // Take over leadership
  _theLeader = "";
  _theLeaderTouched = true;
  TRI_ASSERT(_failoverCandidates != nullptr && _failoverCandidates->empty());
  if (previousInsyncFollowers.size() > 1) {
    auto ourselves = arangodb::ServerState::instance()->getId();
    auto failoverCandidates =
        std::make_shared<std::vector<ServerID>>(previousInsyncFollowers);
    auto myEntry =
        std::find(failoverCandidates->begin(), failoverCandidates->end(), ourselves);
    // We are a valid failover follower
    TRI_ASSERT(myEntry != failoverCandidates->end());
    // The first server is a different leader! (For some reason the job can be
    // triggered twice) TRI_ASSERT(myEntry != failoverCandidates->begin());
    failoverCandidates->erase(myEntry);
    // Put us in front, put old leader somewhere, we do not really care
    _failoverCandidates = failoverCandidates;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Update the current information in the Agency. We update the failover-
///        list with the newest values, after this the guarantee is that
///        _followers == _failoverCandidates
////////////////////////////////////////////////////////////////////////////////
bool FollowerInfo::updateFailoverCandidates() {
  MUTEX_LOCKER(agencyLocker, _agencyMutex);
  // Acquire _canWriteLock first
  WRITE_LOCKER(canWriteLocker, _canWriteLock);
  // Next acquire _dataLock
  WRITE_LOCKER(dataLocker, _dataLock);
  if (_canWrite) {
// Short circuit, we have multiple writes in the above write lock
// The first needs to do things and flips _canWrite
// All followers can return as soon as the lock is released
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_failoverCandidates->size() == _followers->size());
    std::vector<string> diff;
    std::set_intersection(_failoverCandidates->begin(),
                          _failoverCandidates->end(), _followers->begin(),
                          _followers->end(), std::back_inserter(diff));
    TRI_ASSERT(diff.empty());
#endif
    return _canWrite;
  }
  TRI_ASSERT(_followers->size() + 1 >= _docColl->minReplicationFactor());
  // Update both lists (we use a copy here, as we are modifying them in other places individually!)
  _failoverCandidates = std::make_shared<std::vector<ServerID> const>(*_followers);
  // Just be sure
  TRI_ASSERT(_failoverCandidates.get() != _followers.get());
  TRI_ASSERT(_failoverCandidates->size() == _followers->size());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::vector<string> diff;
  std::set_intersection(_failoverCandidates->begin(), _failoverCandidates->end(),
                        _followers->begin(), _followers->end(), std::back_inserter(diff));
  TRI_ASSERT(diff.empty());
#endif
  Result res = persistInAgency(true);
  if (!res.ok()) {
    // We could not persist the update in the agency.
    // Collection left in RO mode.
    LOG_TOPIC("7af00", INFO, Logger::CLUSTER)
        << "Could not persist insync follower for " << _docColl->vocbase().name()
        << "/" << std::to_string(_docColl->planId())
        << " keep RO-mode for now, next write will retry.";
    TRI_ASSERT(!_canWrite);
  } else {
    _canWrite = true;
  }
  return _canWrite;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Persist information in Current
////////////////////////////////////////////////////////////////////////////////
Result FollowerInfo::persistInAgency(bool isRemove) const {
  // Now tell the agency
  TRI_ASSERT(_docColl != nullptr);
  std::string curPath = CurrentShardPath(*_docColl);
  std::string planPath = PlanShardPath(*_docColl);
  AgencyComm ac;
  do {
    AgencyReadTransaction trx(std::vector<std::string>(
        {AgencyCommManager::path(planPath), AgencyCommManager::path(curPath)}));
    AgencyCommResult res = ac.sendTransactionWithFailover(trx);
    if (res.successful()) {
      TRI_ASSERT(res.slice().isArray() && res.slice().length() == 1);
      VPackSlice resSlice = res.slice()[0];
      // Let's look at the results, note that both can be None!
      velocypack::Slice planEntry = PlanShardEntry(*_docColl, resSlice);
      velocypack::Slice currentEntry = CurrentShardEntry(*_docColl, resSlice);

      if (!currentEntry.isObject()) {
        LOG_TOPIC("01896", ERR, Logger::CLUSTER)
            << reportName(isRemove) << ", did not find object in " << curPath;
        if (!currentEntry.isNone()) {
          LOG_TOPIC("57c84", ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        if (!planEntry.isArray() || planEntry.length() == 0 || !planEntry[0].isString() ||
            !planEntry[0].isEqualString(ServerState::instance()->getId())) {
          LOG_TOPIC("42231", INFO, Logger::CLUSTER)
              << reportName(isRemove)
              << ", did not find myself in Plan: " << _docColl->vocbase().name()
              << "/" << std::to_string(_docColl->planId())
              << " (can happen when the leader changed recently).";
          if (!planEntry.isNone()) {
            LOG_TOPIC("ffede", INFO, Logger::CLUSTER) << "Found: " << planEntry.toJson();
          }
          return {TRI_ERROR_CLUSTER_NOT_LEADER};
        } else {
          auto newValue = newShardEntry(currentEntry);
          AgencyWriteTransaction trx;
          trx.preconditions.push_back(
              AgencyPrecondition(curPath, AgencyPrecondition::Type::VALUE, currentEntry));
          trx.preconditions.push_back(
              AgencyPrecondition(planPath, AgencyPrecondition::Type::VALUE, planEntry));
          trx.operations.push_back(AgencyOperation(curPath, AgencyValueOperationType::SET,
                                                   newValue.slice()));
          trx.operations.push_back(
              AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
          AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
          if (res2.successful()) {
            return {TRI_ERROR_NO_ERROR};
          }
        }
      }
    } else {
      LOG_TOPIC("b7333", WARN, Logger::CLUSTER)
          << reportName(isRemove) << ", could not read " << planPath << " and "
          << curPath << " in agency.";
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(500ms);
  } while (!application_features::ApplicationServer::isStopping());
  return TRI_ERROR_SHUTTING_DOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inject the information about "servers" and "failoverCandidates"
////////////////////////////////////////////////////////////////////////////////

void FollowerInfo::injectFollowerInfoInternal(VPackBuilder& builder) const {
  auto ourselves = arangodb::ServerState::instance()->getId();
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue(maintenance::SERVERS));
  {
    VPackArrayBuilder bb(&builder);
    builder.add(VPackValue(ourselves));
    for (auto const& f : *_followers) {
      builder.add(VPackValue(f));
    }
  }
  builder.add(VPackValue(StaticStrings::FailoverCandidates));
  {
    VPackArrayBuilder bb(&builder);
    builder.add(VPackValue(ourselves));
    for (auto const& f : *_failoverCandidates) {
      builder.add(VPackValue(f));
    }
  }
  TRI_ASSERT(builder.isOpenObject());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change JSON under
/// Current/Collection/<DB-name>/<Collection-ID>/<shard-ID>
/// to add or remove a serverID, if add flag is true, the entry is added
/// (if it is not yet there), otherwise the entry is removed (if it was
/// there).
////////////////////////////////////////////////////////////////////////////////

VPackBuilder FollowerInfo::newShardEntry(VPackSlice oldValue) const {
  VPackBuilder newValue;
  TRI_ASSERT(oldValue.isObject());
  {
    VPackObjectBuilder b(&newValue);
    // Copy all but SERVERS and FailoverCandidates.
    // They will be injected later.
    for (auto const& it : VPackObjectIterator(oldValue)) {
      if (!it.key.isEqualString(maintenance::SERVERS) &&
          !it.key.isEqualString(StaticStrings::FailoverCandidates)) {
        newValue.add(it.key);
        newValue.add(it.value);
      }
    }
    injectFollowerInfoInternal(newValue);
  }
  return newValue;
}