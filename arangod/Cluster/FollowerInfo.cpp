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

void FollowerInfo::add(ServerID const& sid) {

  MUTEX_LOCKER(locker, _agencyMutex);

  std::shared_ptr<std::vector<ServerID>> v;

  {
    WRITE_LOCKER(writeLocker, _dataLock);
    // First check if there is anything to do:
    for (auto const& s : *_followers) {
      if (s == sid) {
        return;  // Do nothing, if follower already there
      }
    }
    // Fully copy the vector:
    v = std::make_shared<std::vector<ServerID>>(*_followers);
    v->push_back(sid);  // add a single entry
    _followers = v;     // will cast to std::vector<ServerID> const
  #ifdef DEBUG_SYNC_REPLICATION
    if (!AgencyCommManager::MANAGER) {
      return;
    }
  #endif
  }
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->vocbase().name();
  path += "/";
  path += std::to_string(_docColl->planId());
  path += "/";
  path += _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path);

    if (res.successful()) {
      velocypack::Slice currentEntry = res.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Current", "Collections",
           _docColl->vocbase().name(), std::to_string(_docColl->planId()),
           _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "FollowerInfo::add, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        auto newValue = newShardEntry(currentEntry, sid, true);
        std::string key = "Current/Collections/" + _docColl->vocbase().name() +
                          "/" + std::to_string(_docColl->planId()) + "/" +
                          _docColl->name();
        AgencyWriteTransaction trx;
        trx.preconditions.push_back(
            AgencyPrecondition(key, AgencyPrecondition::Type::VALUE, currentEntry));
        trx.operations.push_back(
            AgencyOperation(key, AgencyValueOperationType::SET, newValue.slice()));
        trx.operations.push_back(
            AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
        AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
        if (res2.successful()) {
          success = true;
          break;  //
        } else {
          LOG_TOPIC(WARN, Logger::CLUSTER)
              << "FollowerInfo::add, could not cas key " << path;
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER)
          << "FollowerInfo::add, could not read " << path << " in agency.";
    }
    std::this_thread::sleep_for(std::chrono::microseconds(500000));
  } while (TRI_microtime() < startTime + 30);
  if (!success) {
    LOG_TOPIC(ERR, Logger::CLUSTER)
        << "FollowerInfo::add, timeout in agency operation for key " << path;
    return;
  }
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Adding follower " << sid << " to "
                                    << _docColl->name() << "succeeded.";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower from a shard, this is only done by the
/// server if a synchronous replication request fails. This reports to
/// the agency under `/Current` but in asynchronous "fire-and-forget"
/// way. The method fails silently, if the follower information has
/// since been dropped (see `dropFollowerInfo` below).
////////////////////////////////////////////////////////////////////////////////

bool FollowerInfo::remove(ServerID const& sid) {
  if (application_features::ApplicationServer::isStopping()) {
    // If we are already shutting down, we cannot be trusted any more with
    // such an important decision like dropping a follower.
    return false;
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Removing follower " << sid << " from " << _docColl->name();

  MUTEX_LOCKER(locker, _agencyMutex);

  {
    WRITE_LOCKER(writeLocker, _dataLock);
    // First check if there is anything to do:
    bool found = false;
    for (auto const& s : *_followers) {
      if (s == sid) {
        found = true;
        break;
      }
    }
    if (!found) {
      return true;  // nothing to do
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
      return true;
    }
#endif
  }

  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->vocbase().name();
  path += "/";
  path += std::to_string(_docColl->planId());
  path += "/";
  path += _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path);
    if (res.successful()) {
      velocypack::Slice currentEntry = res.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Current", "Collections",
           _docColl->vocbase().name(), std::to_string(_docColl->planId()),
           _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "FollowerInfo::remove, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        auto newValue = newShardEntry(currentEntry, sid, false);
        std::string key = "Current/Collections/" + _docColl->vocbase().name() +
                          "/" + std::to_string(_docColl->planId()) + "/" +
                          _docColl->name();
        AgencyWriteTransaction trx;
        trx.preconditions.push_back(
            AgencyPrecondition(key, AgencyPrecondition::Type::VALUE, currentEntry));
        trx.operations.push_back(
            AgencyOperation(key, AgencyValueOperationType::SET, newValue.slice()));
        trx.operations.push_back(
            AgencyOperation("Current/Version", AgencySimpleOperationType::INCREMENT_OP));
        AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
        if (res2.successful()) {
          success = true;
          break;  //
        } else {
          LOG_TOPIC(WARN, Logger::CLUSTER)
              << "FollowerInfo::remove, could not cas key " << path
              << ". status code: " << res2._statusCode
              << ", incriminating body: " << res2.bodyRef();
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER)
          << "FollowerInfo::remove, could not read " << path << " in agency.";
    }
    std::this_thread::sleep_for(std::chrono::microseconds(500000));
  } while (TRI_microtime() < startTime + 30 &&
           application_features::ApplicationServer::isRetryOK());
  if (!success) {
    LOG_TOPIC(ERR, Logger::CLUSTER)
        << "FollowerInfo::remove, timeout in agency operation for key " << path;
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Removing follower " << sid << " from "
                                    << _docColl->name() << "succeeded: " << success;

  return success;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief clear follower list, no changes in agency necessary
//////////////////////////////////////////////////////////////////////////////

void FollowerInfo::clear() {
  WRITE_LOCKER(writeLocker, _dataLock);
  auto v = std::make_shared<std::vector<ServerID>>();
  _followers = v;  // will cast to std::vector<ServerID> const
}
