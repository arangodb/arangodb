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
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;


////////////////////////////////////////////////////////////////////////////////
/// @brief change JSON under
/// Current/Collection/<DB-name>/<Collection-ID>/<shard-ID>
/// to add or remove a serverID, if add flag is true, the entry is added
/// (if it is not yet there), otherwise the entry is removed (if it was
/// there).
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder newShardEntry(VPackSlice oldValue, ServerID const& sid, bool add) {
  VPackBuilder newValue;
  VPackSlice servers;
  {
    VPackObjectBuilder b(&newValue);
    // Now need to find the `servers` attribute, which is a list:
    for (auto const& it : VPackObjectIterator(oldValue)) {
      if (it.key.isEqualString("servers")) {
        servers = it.value;
      } else {
        newValue.add(it.key);
        newValue.add(it.value);
      }
    }
    newValue.add(VPackValue("servers"));
    if (servers.isArray() && servers.length() > 0) {
      VPackArrayBuilder bb(&newValue);
      newValue.add(servers[0]);
      VPackArrayIterator it(servers);
      bool done = false;
      for (++it; it.valid(); ++it) {
        if ((*it).isEqualString(sid)) {
          if (add) {
            newValue.add(*it);
            done = true;
          }
        } else {
          newValue.add(*it);
        }
      }
      if (add && !done) {
        newValue.add(VPackValue(sid));
      }
    } else {
      VPackArrayBuilder bb(&newValue);
      newValue.add(VPackValue(ServerState::instance()->getId()));
      if (add) {
        newValue.add(VPackValue(sid));
      }
    }
  }
  return newValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower to a shard, this is only done by the server side
/// of the "get-in-sync" capabilities. This reports to the agency under
/// `/Current` but in asynchronous "fire-and-forget" way.
////////////////////////////////////////////////////////////////////////////////

Result FollowerInfo::add(ServerID const& sid) {
  TRI_IF_FAILURE("FollowerInfo::add") {
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, "unable to add follower"};
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
  #ifdef DEBUG_SYNC_REPLICATION
    if (!AgencyCommManager::MANAGER) {
      return {TRI_ERROR_NO_ERROR};
    }
  #endif
  }
  std::string planId = std::to_string(_docColl->planId());
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = _docColl->vocbase().name() + "/" + planId + "/";
  std::string curPath = "Current/Collections/" + path + _docColl->name();
  // We also need the corresponding Plan entry, path is
  //   Plan/Collections/<dbName>/<collectionID>/shards/<shardID>
  std::string planPath = "Plan/Collections/" + path + "shards/" +
                         _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  do {
    AgencyReadTransaction trx(std::vector<std::string>(
        {AgencyCommManager::path(planPath), AgencyCommManager::path(curPath)}));
    AgencyCommResult res = ac.sendTransactionWithFailover(trx);

    if (res.successful()) {
      // Let's look at the results, note that both can be None!
      velocypack::Slice planEntry = res.slice()[0].get(
          std::vector<std::string>(
            {AgencyCommManager::path(), "Plan", "Collections",
             _docColl->vocbase().name(), planId, "shards", _docColl->name()}));
      velocypack::Slice currentEntry = res.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Current", "Collections",
           _docColl->vocbase().name(), planId, _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC("b753d", ERR, Logger::CLUSTER)
            << "FollowerInfo::add, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC("568de", ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        if (!planEntry.isArray() || planEntry.length() == 0 ||
            !planEntry[0].isString() ||
            !planEntry[0].isEqualString(ServerState::instance()->getId())) {
          LOG_TOPIC("54555", INFO, Logger::CLUSTER)
              << "FollowerInfo::add, did not find myself in Plan: " << path
              << " (can happen when the leader changed recently).";
          if (!planEntry.isNone()) {
            LOG_TOPIC("66762", INFO, Logger::CLUSTER) << "Found: " << planEntry.toJson();
          }
          return {TRI_ERROR_CLUSTER_NOT_LEADER};
        } else {
          auto newValue = newShardEntry(currentEntry, sid, true);
          AgencyWriteTransaction trx;
          trx.preconditions.push_back(
              AgencyPrecondition(curPath, AgencyPrecondition::Type::VALUE, currentEntry));
          trx.preconditions.push_back(
              AgencyPrecondition(planPath, AgencyPrecondition::Type::VALUE, planEntry));
          trx.operations.push_back(
              AgencyOperation(curPath, AgencyValueOperationType::SET, newValue.slice()));
          trx.operations.push_back(
              AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
          AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
          if (res2.successful()) {
            return {TRI_ERROR_NO_ERROR};
          } else {
            LOG_TOPIC("daeda", WARN, Logger::CLUSTER)
                << "FollowerInfo::add, could not cas key " << path;
          }
        }
      }
    } else {
      LOG_TOPIC("dcf54", WARN, Logger::CLUSTER)
          << "FollowerInfo::add, could not read " << planPath
          << " and " << curPath << " in agency.";
    }
    std::this_thread::sleep_for(std::chrono::microseconds(500000));
  } while (TRI_microtime() < startTime + 3600 &&
           application_features::ApplicationServer::isRetryOK());
  // This is important, give it 1h if needed. We really do not want to get
  // into the position to not accept a shard getting-in-sync just because
  // we cannot talk to the agency temporarily.
  int errorCode = (application_features::ApplicationServer::isRetryOK()) ? TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED : TRI_ERROR_SHUTTING_DOWN;
  std::string errorMessage = "unable to add follower in agency, timeout in agency CAS operation for key " + path + ": " + TRI_errno_string(errorCode);
  LOG_TOPIC("6295b", ERR, Logger::CLUSTER) << errorMessage;

  return {errorCode, std::move(errorMessage)};
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
    return {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED, "unable to remove follower"};
  }

  if (application_features::ApplicationServer::isStopping()) {
    // If we are already shutting down, we cannot be trusted any more with
    // such an important decision like dropping a follower.
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  LOG_TOPIC("ce460", DEBUG, Logger::CLUSTER)
      << "Removing follower " << sid << " from " << _docColl->name();

  MUTEX_LOCKER(locker, _agencyMutex);
  WRITE_LOCKER(writeLocker, _dataLock); // the data lock has to be locked until this function completes
                                        // because if the agency communication does not work
                                        // local data is modified again.

  // First check if there is anything to do:
  bool found = false;
  for (auto const& s : *_followers) {
    if (s == sid) {
      found = true;
      break;
    }
  }
  if (!found) {
    return {TRI_ERROR_NO_ERROR};  // nothing to do
  }

  auto v = std::make_shared<std::vector<ServerID>>();
  if (_followers->size() > 0) {
    v->reserve(_followers->size() - 1);
    for (auto const& i : *_followers) {
      if (i != sid) {
        v->push_back(i);
      }
    }
  }
  auto _oldFollowers = _followers;
  _followers = v;  // will cast to std::vector<ServerID> const
#ifdef DEBUG_SYNC_REPLICATION
  if (!AgencyCommManager::MANAGER) {
    return {TRI_ERROR_NO_ERROR};
  }
#endif

  std::string planId = std::to_string(_docColl->planId());
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = _docColl->vocbase().name() + "/" + planId + "/";
  std::string curPath = "Current/Collections/" + path + _docColl->name();
  // We also need the corresponding Plan entry, path is
  //   Plan/Collections/<dbName>/<collectionID>/shards/<shardID>
  std::string planPath = "Plan/Collections/" + path + "shards/" +
                         _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  do {
    AgencyReadTransaction trx(std::vector<std::string>(
        {AgencyCommManager::path(planPath), AgencyCommManager::path(curPath)}));
    AgencyCommResult res = ac.sendTransactionWithFailover(trx);
    if (res.successful()) {
      // Let's look at the results, note that both can be None!
      velocypack::Slice planEntry = res.slice()[0].get(
          std::vector<std::string>(
            {AgencyCommManager::path(), "Plan", "Collections",
             _docColl->vocbase().name(), planId, "shards", _docColl->name()}));
      velocypack::Slice currentEntry = res.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Current", "Collections",
           _docColl->vocbase().name(), std::to_string(_docColl->planId()),
           _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC("01896", ERR, Logger::CLUSTER)
            << "FollowerInfo::remove, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC("57c84", ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        if (!planEntry.isArray() || planEntry.length() == 0 ||
            !planEntry[0].isString() ||
            !planEntry[0].isEqualString(ServerState::instance()->getId())) {
          LOG_TOPIC("42231", INFO, Logger::CLUSTER)
              << "FollowerInfo::remove, did not find myself in Plan: " << path
              << " (can happen when the leader changed recently).";
          if (!planEntry.isNone()) {
            LOG_TOPIC("ffede", INFO, Logger::CLUSTER) << "Found: " << planEntry.toJson();
          }
          return {TRI_ERROR_CLUSTER_NOT_LEADER};
        } else {
          auto newValue = newShardEntry(currentEntry, sid, false);
          std::string key = "Current/Collections/" + _docColl->vocbase().name() +
                            "/" + std::to_string(_docColl->planId()) + "/" +
                            _docColl->name();
          AgencyWriteTransaction trx;
          trx.preconditions.push_back(
              AgencyPrecondition(key, AgencyPrecondition::Type::VALUE, currentEntry));
          trx.preconditions.push_back(
              AgencyPrecondition(planPath, AgencyPrecondition::Type::VALUE, planEntry));
          trx.operations.push_back(
              AgencyOperation(key, AgencyValueOperationType::SET, newValue.slice()));
          trx.operations.push_back(
              AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
          AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
          if (res2.successful()) {
            // we are finished
            LOG_TOPIC("be0cb", DEBUG, Logger::CLUSTER) << "Removing follower " << sid << " from "
                                                     << _docColl->name() << "succeeded";
            return {TRI_ERROR_NO_ERROR};
          } else {
            LOG_TOPIC("67778", WARN, Logger::CLUSTER)
                << "FollowerInfo::remove, could not cas key " << path
                << ". status code: " << res2._statusCode
                << ", incriminating body: " << res2.bodyRef();
          }
        }
      }
    } else {
      LOG_TOPIC("b7333", WARN, Logger::CLUSTER)
          << "FollowerInfo::remove, could not read " << planPath
          << " and " << curPath << " in agency.";
    }
    std::this_thread::sleep_for(std::chrono::microseconds(500000));
  } while (TRI_microtime() < startTime + 7200 &&
           application_features::ApplicationServer::isRetryOK());
  // This is important, give it 2h if needed. We really do not want to get
  // into the position to fail to drop a follower, just because we cannot
  // talk to the agency temporarily. The worst would be to drop the follower
  // locally but not report the fact to the agency. The second worst is to
  // not be able to drop the follower, despite the fact that a replication
  // was not successful. All else is less dramatic. Therefore we try for
  // a long time.

  // rollback:
  _followers = _oldFollowers;
  
  int errorCode = (application_features::ApplicationServer::isRetryOK()) ? TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED : TRI_ERROR_SHUTTING_DOWN;
  std::string errorMessage = "unable to remove follower from agency, timeout in agency CAS operation for key " + path + ": " + TRI_errno_string(errorCode);
  LOG_TOPIC("a0dcc", ERR, Logger::CLUSTER) << errorMessage;

  return {errorCode, std::move(errorMessage)};
}

//////////////////////////////////////////////////////////////////////////////
/// @brief clear follower list, no changes in agency necessary
//////////////////////////////////////////////////////////////////////////////

void FollowerInfo::clear() {
  WRITE_LOCKER(writeLocker, _dataLock);
  _followers = std::make_shared<std::vector<ServerID>>();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief check whether the given server is a follower
//////////////////////////////////////////////////////////////////////////////

bool FollowerInfo::contains(ServerID const& sid) const {
  READ_LOCKER(readLocker, _dataLock);
  auto const& f = *_followers;
  return std::find(f.begin(), f.end(), sid) != f.end();
}
