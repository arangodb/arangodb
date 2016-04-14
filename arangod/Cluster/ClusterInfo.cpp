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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ClusterInfo.h"

#include <velocypack/Iterator.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/JsonHelper.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/json-utilities.h"
#include "Basics/json.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "VocBase/document-collection.h"

#ifdef _WIN32
// turn off warnings about too long type name for debug symbols blabla in MSVC
// only...
#pragma warning(disable : 4503)
#endif

using namespace arangodb;

using arangodb::basics::JsonHelper;

static std::unique_ptr<ClusterInfo> _instance;

////////////////////////////////////////////////////////////////////////////////
/// @brief a local helper to report errors and messages
////////////////////////////////////////////////////////////////////////////////

static inline int setErrormsg(int ourerrno, std::string& errorMsg) {
  errorMsg = TRI_errno_string(ourerrno);
  return ourerrno;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the JSON returns an error
////////////////////////////////////////////////////////////////////////////////

static inline bool hasError(VPackSlice const& slice) {
  return arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "error",
                                                             false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static std::string extractErrorMessage(std::string const& shardId,
                                       VPackSlice const& slice) {
  std::string msg = " shardID:" + shardId + ": ";

  // add error message text
  msg += arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                            "errorMessage", "");

  // add error number
  if (slice.hasKey("errorNum")) {
    VPackSlice const errorNum = slice.get("errorNum");
    if (errorNum.isNumber()) {
      msg += " (errNum=" + arangodb::basics::StringUtils::itoa(
                               errorNum.getNumericValue<uint32_t>()) +
             ")";
    }
  }

  return msg;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo() : _json(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo(TRI_json_t* json) : _json(json) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo(CollectionInfo const& other)
    : _json(other._json) {
  if (other._json != nullptr) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move constructs a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo(CollectionInfo&& other) : _json(other._json) {
  other._json = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy assigns a collection info object from another one
////////////////////////////////////////////////////////////////////////////////

CollectionInfo& CollectionInfo::operator=(CollectionInfo const& other) {
  if (other._json != nullptr && this != &other) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  } else {
    _json = nullptr;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move assigns a collection info object from another one
////////////////////////////////////////////////////////////////////////////////

CollectionInfo& CollectionInfo::operator=(CollectionInfo&& other) {
  if (this == &other) {
    return *this;
  }

  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
  _json = other._json;
  other._json = nullptr;

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::~CollectionInfo() {
  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(ShardID const& shardID,
                                             TRI_json_t* json) {
  _jsons.insert(make_pair(shardID, json));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(CollectionInfoCurrent const& other)
    : _jsons(other._jsons) {
  copyAllJsons();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a collection info current object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(CollectionInfoCurrent&& other) {
  _jsons.swap(other._jsons);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy assigns a collection info current object from another one
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent& CollectionInfoCurrent::operator=(
    CollectionInfoCurrent const& other) {
  if (this == &other) {
    return *this;
  }
  freeAllJsons();
  _jsons = other._jsons;
  copyAllJsons();
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent& CollectionInfoCurrent::operator=(
    CollectionInfoCurrent&& other) {
  if (this == &other) {
    return *this;
  }
  freeAllJsons();
  _jsons.clear();
  _jsons.swap(other._jsons);
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::~CollectionInfoCurrent() { freeAllJsons(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief free all pointers to TRI_json_t in the map _jsons
////////////////////////////////////////////////////////////////////////////////

void CollectionInfoCurrent::freeAllJsons() {
  for (auto it = _jsons.begin(); it != _jsons.end(); ++it) {
    if (it->second != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it->second);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy TRI_json_t behind the pointers in the map _jsons
////////////////////////////////////////////////////////////////////////////////

void CollectionInfoCurrent::copyAllJsons() {
  for (auto it = _jsons.begin(); it != _jsons.end(); ++it) {
    if (nullptr != it->second) {
      it->second = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the clusterinfo instance
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::createInstance(AgencyCallbackRegistry* agencyCallbackRegistry) {
  _instance.reset(new ClusterInfo(agencyCallbackRegistry));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an instance of the cluster info class
////////////////////////////////////////////////////////////////////////////////

ClusterInfo* ClusterInfo::instance() { return _instance.get(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::ClusterInfo(AgencyCallbackRegistry* agencyCallbackRegistry)
  : _agency(), _agencyCallbackRegistry(agencyCallbackRegistry), _uniqid() {
  _uniqid._currentValue = _uniqid._upperValue = 0ULL;

  // Actual loading into caches is postponed until necessary
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo() {
  clearPlannedDatabases(_plannedDatabases);
  clearCurrentDatabases(_currentDatabases);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the uniqid value. if it exceeds the upper bound, fetch a
/// new upper bound value from the agency
////////////////////////////////////////////////////////////////////////////////

uint64_t ClusterInfo::uniqid(uint64_t count) {
  MUTEX_LOCKER(mutexLocker, _idLock);

  if (_uniqid._currentValue + count - 1 >= _uniqid._upperValue) {
    uint64_t fetch = count;

    if (fetch < MinIdsPerBatch) {
      fetch = MinIdsPerBatch;
    }

    AgencyCommResult result = _agency.uniqid("Sync/LatestID", fetch, 0.0);

    if (!result.successful() || result._index == 0) {
      return 0;
    }

    _uniqid._currentValue = result._index + count;
    _uniqid._upperValue = _uniqid._currentValue + fetch - 1;

    return result._index;
  }

  uint64_t result = _uniqid._currentValue;
  _uniqid._currentValue += count;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::flush() {
  loadServers();
  loadCurrentDBServers();
  loadCurrentCoordinators();
  loadPlannedDatabases();
  loadCurrentDatabases();
  loadPlannedCollections();
  loadCurrentCollections();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist(DatabaseID const& databaseID, bool reload) {
  int tries = 0;

  if (reload || !_plannedDatabasesProt.isValid ||
      !_currentDatabasesProt.isValid || !_DBServersProt.isValid) {
    loadPlannedDatabases();
    loadCurrentDatabases();
    loadCurrentDBServers();
    ++tries;  // no need to reload if the database is not found
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.

  while (true) {
    {
      size_t expectedSize;
      {
        READ_LOCKER(readLocker, _DBServersProt.lock);
        expectedSize = _DBServers.size();
      }

      // look up database by name:

      READ_LOCKER(readLocker, _plannedDatabasesProt.lock);
      // _plannedDatabases is a map-type<DatabaseID, TRI_json_t*>
      auto it = _plannedDatabases.find(databaseID);

      if (it != _plannedDatabases.end()) {
        // found the database in Plan
        READ_LOCKER(readLocker, _currentDatabasesProt.lock);
        // _currentDatabases is
        //     a map-type<DatabaseID, a map-type<ServerID, TRI_json_t*>>
        auto it2 = _currentDatabases.find(databaseID);

        if (it2 != _currentDatabases.end()) {
          // found the database in Current

          return ((*it2).second.size() >= expectedSize);
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    loadPlannedDatabases();
    loadCurrentDatabases();
    loadCurrentDBServers();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

std::vector<DatabaseID> ClusterInfo::listDatabases(bool reload) {
  std::vector<DatabaseID> result;

  if (reload || !_plannedDatabasesProt.isValid ||
      !_currentDatabasesProt.isValid || !_DBServersProt.isValid) {
    loadPlannedDatabases();
    loadCurrentDatabases();
    loadCurrentDBServers();
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.

  size_t expectedSize;
  {
    READ_LOCKER(readLocker, _DBServersProt.lock);
    expectedSize = _DBServers.size();
  }

  {
    READ_LOCKER(readLockerPlanned, _plannedDatabasesProt.lock);
    READ_LOCKER(readLockerCurrent, _currentDatabasesProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, TRI_json_t*>
    auto it = _plannedDatabases.begin();

    while (it != _plannedDatabases.end()) {
      // _currentDatabases is:
      //   a map-type<DatabaseID, a map-type<ServerID, TRI_json_t*>>
      auto it2 = _currentDatabases.find((*it).first);

      if (it2 != _currentDatabases.end()) {
        if ((*it2).second.size() >= expectedSize) {
          result.push_back((*it).first);
        }
      }

      ++it;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually clears a list of planned databases
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::clearPlannedDatabases(
    std::unordered_map<DatabaseID, TRI_json_t*>& databases) {
  auto it = databases.begin();
  while (it != databases.end()) {
    TRI_json_t* json = (*it).second;

    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    ++it;
  }
  databases.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about planned databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////
//
static std::string const prefixPlannedDatabases = "Plan/Databases";

void ClusterInfo::loadPlannedDatabases() {
  uint64_t storedVersion = _plannedDatabasesProt.version;
  MUTEX_LOCKER(mutexLocker, _plannedDatabasesProt.mutex);
  if (_plannedDatabasesProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixPlannedDatabases, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixPlannedDatabases + "/", false);

    decltype(_plannedDatabases) newDatabases;

    // result._values is a std::map<std::string, AgencyCommResultEntry>
    auto it = result._values.begin();

    while (it != result._values.end()) {
      std::string const& name = (*it).first;
      // TODO: _plannedDatabases need to be moved to velocypack
      // Than this can be merged to swap
      TRI_json_t* options =
          arangodb::basics::VelocyPackHelper::velocyPackToJson(
              (*it).second._vpack->slice());

      // steal the VelocyPack
      (*it).second._vpack.reset();
      newDatabases.insert(std::make_pair(name, options));

      ++it;
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _plannedDatabasesProt.lock);
      _plannedDatabases.swap(newDatabases);
      _plannedDatabasesProt.version++;  // such that others notice our change
      _plannedDatabasesProt.isValid = true;  // will never be reset to false
    }
    clearPlannedDatabases(newDatabases);  // delete the old stuff
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixPlannedDatabases
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a list of current databases
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::clearCurrentDatabases(
    std::unordered_map<DatabaseID, std::unordered_map<ServerID, TRI_json_t*>>&
        databases) {
  auto it = databases.begin();
  while (it != databases.end()) {
    auto it2 = (*it).second.begin();

    while (it2 != (*it).second.end()) {
      TRI_json_t* json = (*it2).second;

      if (json != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }

      ++it2;
    }
    ++it;
  }

  databases.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentDatabases = "Current/Databases";

void ClusterInfo::loadCurrentDatabases() {
  uint64_t storedVersion = _currentDatabasesProt.version;
  MUTEX_LOCKER(mutexLocker, _currentDatabasesProt.mutex);
  if (_currentDatabasesProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixCurrentDatabases, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixCurrentDatabases + "/", false);

    decltype(_currentDatabases) newDatabases;

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.begin();

    while (it != result._values.end()) {
      std::string const key = (*it).first;

      // each entry consists of a database id and a collection id, separated by
      // '/'
      std::vector<std::string> parts =
          arangodb::basics::StringUtils::split(key, '/');

      if (parts.empty()) {
        ++it;
        continue;
      }
      std::string const database = parts[0];

      // _currentDatabases is
      //   a map-type<DatabaseID, a map-type<ServerID, TRI_json_t*>>
      auto it2 = newDatabases.find(database);

      if (it2 == newDatabases.end()) {
        // insert an empty list for this database
        decltype(it2->second) empty;
        it2 = newDatabases.insert(std::make_pair(database, empty)).first;
      }

      if (parts.size() == 2) {
        // got a server name
        //
        // TODO: _plannedDatabases need to be moved to velocypack
        // Than this can be merged to swap
        TRI_json_t* json = arangodb::basics::VelocyPackHelper::velocyPackToJson(
            (*it).second._vpack->slice());

        // steal the VelocyPack
        (*it).second._vpack.reset();
        (*it2).second.insert(std::make_pair(parts[1], json));
      }

      ++it;
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _currentDatabasesProt.lock);
      _currentDatabases.swap(newDatabases);
      _currentDatabasesProt.version++;  // such that others notice our change
      _currentDatabasesProt.isValid = true;  // will never be reset to false
    }
    clearCurrentDatabases(newDatabases);  // delete the old stuff
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixCurrentDatabases
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixPlannedCollections = "Plan/Collections";

void ClusterInfo::loadPlannedCollections() {
  uint64_t storedVersion = _plannedCollectionsProt.version;
  MUTEX_LOCKER(mutexLocker, _plannedCollectionsProt.mutex);
  if (_plannedCollectionsProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixPlannedCollections, true);
    } else {
      LOG(ERR) << "Error while locking " << prefixPlannedCollections;
      return;
    }
  }

  if (result.successful()) {
    result.parse(prefixPlannedCollections + "/", false);

    decltype(_plannedCollections) newCollections;
    decltype(_shards) newShards;
    decltype(_shardKeys) newShardKeys;

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.begin();

    for (; it != result._values.end(); ++it) {
      std::string const key = (*it).first;

      // each entry consists of a database id and a collection id, separated by
      // '/'
      std::vector<std::string> parts =
          arangodb::basics::StringUtils::split(key, '/');

      if (parts.size() != 2) {
        // invalid entry
        LOG(WARN) << "found invalid collection key in agency: '" << key << "'";
        continue;
      }

      std::string const database = parts[0];
      std::string const collection = parts[1];

      // check whether we have created an entry for the database already
      AllCollections::iterator it2 = newCollections.find(database);

      if (it2 == newCollections.end()) {
        // not yet, so create an entry for the database
        DatabaseCollections empty;
        newCollections.emplace(std::make_pair(database, empty));
        it2 = newCollections.find(database);
      }

      // TODO: The Collection info has to store VPack instead of JSON
      TRI_json_t* json = arangodb::basics::VelocyPackHelper::velocyPackToJson(
          (*it).second._vpack->slice());
      // steal the velocypack
      (*it).second._vpack.reset();

      auto collectionData = std::make_shared<CollectionInfo>(json);
      auto shardKeys = std::make_shared<std::vector<std::string>>(
          collectionData->shardKeys());
      newShardKeys.insert(make_pair(collection, shardKeys));
      auto shardIDs = collectionData->shardIds();
      auto shards = std::make_shared<std::vector<std::string>>();
      for (auto const& p : *shardIDs) {
        shards->push_back(p.first);
      }
      // Sort by the number in the shard ID ("s0000001" for example):
      std::sort(shards->begin(), shards->end(),
                [](std::string const& a, std::string const& b) -> bool {
                  return std::strtol(a.c_str() + 1, nullptr, 10) <
                         std::strtol(b.c_str() + 1, nullptr, 10);
                });
      newShards.emplace(std::make_pair(collection, shards));

      // insert the collection into the existing map, insert it under its
      // ID as well as under its name, so that a lookup can be done with
      // either of the two.

      (*it2).second.emplace(std::make_pair(collection, collectionData));
      (*it2).second.emplace(
          std::make_pair(collectionData->name(), collectionData));
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _plannedCollectionsProt.lock);
      _plannedCollections.swap(newCollections);
      _shards.swap(newShards);
      _shardKeys.swap(newShardKeys);
      _plannedCollectionsProt.version++;  // such that others notice our change
      _plannedCollectionsProt.isValid = true;  // will never be reset to false
    }
    return;
  }

  LOG(ERR) << "Error while loading " << prefixPlannedCollections
           << " httpCode: " << result.httpCode()
           << " errorCode: " << result.errorCode()
           << " errorMessage: " << result.errorMessage()
           << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CollectionInfo> ClusterInfo::getCollection(
    DatabaseID const& databaseID, CollectionID const& collectionID) {
  int tries = 0;

  if (!_plannedCollectionsProt.isValid) {
    loadPlannedCollections();
    ++tries;
  }

  while (true) {  // left by break
    {
      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
      // look up database by id
      AllCollections::const_iterator it = _plannedCollections.find(databaseID);

      if (it != _plannedCollections.end()) {
        // look up collection by id (or by name)
        DatabaseCollections::const_iterator it2 =
            (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }
    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadPlannedCollections();
  }

  return std::make_shared<CollectionInfo>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

arangodb::VocbaseCollectionInfo ClusterInfo::getCollectionProperties(
    CollectionInfo const& collection) {
  arangodb::VocbaseCollectionInfo info(collection);
  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

VocbaseCollectionInfo ClusterInfo::getCollectionProperties(
    DatabaseID const& databaseID, CollectionID const& collectionID) {
  auto ci = getCollection(databaseID, collectionID);
  return getCollectionProperties(*ci);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<CollectionInfo>> const ClusterInfo::getCollections(
    DatabaseID const& databaseID) {
  std::vector<std::shared_ptr<CollectionInfo>> result;

  // always reload
  loadPlannedCollections();

  READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
  // look up database by id
  AllCollections::const_iterator it = _plannedCollections.find(databaseID);

  if (it == _plannedCollections.end()) {
    return result;
  }

  // iterate over all collections
  DatabaseCollections::const_iterator it2 = (*it).second.begin();
  while (it2 != (*it).second.end()) {
    char c = (*it2).first[0];

    if (c < '0' || c > '9') {
      // skip collections indexed by id
      result.push_back((*it2).second);
    }

    ++it2;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current collections from the agency
/// Usually one does not have to call this directly. Note that this is
/// necessarily complicated, since here we have to consider information
/// about all shards of a collection.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentCollections = "Current/Collections";
void ClusterInfo::loadCurrentCollections() {
  uint64_t storedVersion = _currentCollectionsProt.version;
  MUTEX_LOCKER(mutexLocker, _currentCollectionsProt.mutex);
  if (_currentCollectionsProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixCurrentCollections, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixCurrentCollections + "/", false);

    decltype(_currentCollections) newCollections;
    decltype(_shardIds) newShardIds;

    std::map<std::string, AgencyCommResultEntry>::iterator it =
        result._values.begin();

    for (; it != result._values.end(); ++it) {
      std::string const key = (*it).first;

      // each entry consists of a database id, a collection id, and a shardID,
      // separated by '/'
      std::vector<std::string> parts =
          arangodb::basics::StringUtils::split(key, '/');

      if (parts.size() != 3) {
        // invalid entry
        LOG(WARN) << "found invalid collection key in current in agency: '"
                  << key << "'";
        continue;
      }

      std::string const database = parts[0];
      std::string const collection = parts[1];
      std::string const shardID = parts[2];

      // check whether we have created an entry for the database already
      AllCollectionsCurrent::iterator it2 = newCollections.find(database);

      if (it2 == newCollections.end()) {
        // not yet, so create an entry for the database
        DatabaseCollectionsCurrent empty;
        newCollections.insert(std::make_pair(database, empty));
        it2 = newCollections.find(database);
      }

      // TODO: The Collection info has to store VPack instead of JSON
      TRI_json_t* json = arangodb::basics::VelocyPackHelper::velocyPackToJson(
          (*it).second._vpack->slice());
      // steal the velocypack
      (*it).second._vpack.reset();

      // check whether we already have a CollectionInfoCurrent:
      DatabaseCollectionsCurrent::iterator it3 = it2->second.find(collection);
      if (it3 == it2->second.end()) {
        auto collectionDataCurrent =
            std::make_shared<CollectionInfoCurrent>(shardID, json);
        it2->second.insert(make_pair(collection, collectionDataCurrent));
        it3 = it2->second.find(collection);
      } else {
        it3->second->add(shardID, json);
      }

      // Note that we have only inserted the CollectionInfoCurrent under
      // the collection ID and not under the name! It is not possible
      // to query the current collection info by name. This is because
      // the correct place to hold the current name is in the plan.
      // Thus: Look there and get the collection ID from there. Then
      // ask about the current collection info.

      // Now take note of this shard and its responsible server:
      auto servers = std::make_shared<std::vector<ServerID>>(
          it3->second->servers(shardID));
      newShardIds.insert(make_pair(shardID, servers));
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _currentCollectionsProt.lock);
      _currentCollections.swap(newCollections);
      _shardIds.swap(newShardIds);
      _currentCollectionsProt.version++;  // such that others notice our change
      _currentCollectionsProt.isValid = true;
    }
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixCurrentCollections
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CollectionInfoCurrent> ClusterInfo::getCollectionCurrent(
    DatabaseID const& databaseID, CollectionID const& collectionID) {
  int tries = 0;

  if (!_currentCollectionsProt.isValid) {
    loadCurrentCollections();
    ++tries;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentCollectionsProt.lock);
      // look up database by id
      AllCollectionsCurrent::const_iterator it =
          _currentCollections.find(databaseID);

      if (it != _currentCollections.end()) {
        // look up collection by id
        DatabaseCollectionsCurrent::const_iterator it2 =
            (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadCurrentCollections();
  }

  return std::make_shared<CollectionInfoCurrent>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createDatabaseCoordinator(std::string const& name,
                                           VPackSlice const& slice,
                                           std::string& errorMsg,
                                           double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    res = ac.casValue("Plan/Databases/" + name, slice, false, 0.0, realTimeout);
    if (!res.successful()) {
      if (res._statusCode ==
          (int)arangodb::GeneralResponse::ResponseCode::PRECONDITION_FAILED) {
        return setErrormsg(TRI_ERROR_ARANGO_DUPLICATE_NAME, errorMsg);
      }

      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN,
                         errorMsg);
    }
  }

  // Now update our own cache of planned databases:
  loadPlannedDatabases();

  // Now wait for it to appear and be complete:
  res.clear();
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  std::vector<ServerID> DBServers = getCurrentDBServers();
  int count = 0;  // this counts, when we have to reload the DBServers

  std::string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == DBServers.size()) {
        std::map<std::string, AgencyCommResultEntry>::iterator it;
        std::string tmpMsg = "";
        bool tmpHaveError = false;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          VPackSlice slice = (*it).second._vpack->slice();
          if (arangodb::basics::VelocyPackHelper::getBooleanValue(
                  slice, "error", false)) {
            tmpHaveError = true;
            tmpMsg += " DBServer:" + it->first + ":";
            tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(
                slice, "errorMessage", "");
            if (slice.hasKey("errorNum")) {
              VPackSlice errorNum = slice.get("errorNum");
              if (errorNum.isNumber()) {
                tmpMsg += " (errorNum=";
                tmpMsg += basics::StringUtils::itoa(
                    errorNum.getNumericValue<uint32_t>());
                tmpMsg += ")";
              }
            }
          }
        }
        if (tmpHaveError) {
          errorMsg = "Error in creation of database:" + tmpMsg;
          return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE;
        }
        loadCurrentDatabases();  // update our cache
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }
     
    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", getReloadServerListTimeout() / interval);

    if (++count >= static_cast<int>(getReloadServerListTimeout() / interval)) {
      // We update the list of DBServers every minute in case one of them
      // was taken away since we last looked. This also helps (slightly)
      // if a new DBServer was added. However, in this case we report
      // success a bit too early, which is not too bad.
      loadCurrentDBServers();
      DBServers = getCurrentDBServers();
      count = 0;
    }
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropDatabaseCoordinator(std::string const& name,
                                         std::string& errorMsg,
                                         double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    if (!ac.exists("Plan/Databases/" + name)) {
      return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    }

    res = ac.removeValues("Plan/Databases/" + name, false);
    if (!res.successful()) {
      if (res.httpCode() == (int)GeneralResponse::ResponseCode::NOT_FOUND) {
        return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
      }

      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,
                         errorMsg);
    }

    res.clear();
    res = ac.removeValues("Plan/Collections/" + name, true);

    if (!res.successful() &&
        res.httpCode() != (int)GeneralResponse::ResponseCode::NOT_FOUND) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,
                         errorMsg);
    }
  }

  // Load our own caches:
  loadPlannedDatabases();
  loadPlannedCollections();

  // Now wait for it to appear and be complete:
  res.clear();
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }

  std::string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == 0) {
        AgencyCommLocker locker("Current", "WRITE");
        if (locker.successful()) {
          res.clear();
          res = ac.removeValues(where, true);
          if (res.successful()) {
            return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
          }
          return setErrormsg(
              TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT, errorMsg);
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }
    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", interval);
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createCollectionCoordinator(std::string const& databaseName,
                                             std::string const& collectionID,
                                             uint64_t numberOfShards,
                                             VPackSlice const& json,
                                             std::string& errorMsg,
                                             double timeout) {
  using arangodb::velocypack::Slice;

  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();
  {
    // check if a collection with the same name is already planned
    loadPlannedCollections();

    READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
    AllCollections::const_iterator it = _plannedCollections.find(databaseName);
    if (it != _plannedCollections.end()) {
      std::string const name =
          arangodb::basics::VelocyPackHelper::getStringValue(json, "name", "");

      DatabaseCollections::const_iterator it2 = (*it).second.find(name);

      if (it2 != (*it).second.end()) {
        // collection already exists!
        return TRI_ERROR_ARANGO_DUPLICATE_NAME;
      }
    }
  }

  if (!ac.exists("Plan/Databases/" + databaseName)) {
    return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }

  if (ac.exists("Plan/Collections/" + databaseName + "/" + collectionID)) {
    return setErrormsg(TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS, errorMsg);
  }

  AgencyCommResult result =
      ac.casValue("Plan/Collections/" + databaseName + "/" + collectionID, json,
                  false, 0.0, 0.0);
  if (!result.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                       errorMsg);
  }

  ac.increaseVersionRepeated("Plan/Version");

  // Update our cache:
  loadPlannedCollections();

  // Now wait for it to appear and be complete:
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }

  std::string const where =
      "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == (size_t)numberOfShards) {
        std::string tmpMsg = "";
        bool tmpHaveError = false;
        for (auto const& p : res._values) {
          VPackSlice const slice = p.second._vpack->slice();
          if (arangodb::basics::VelocyPackHelper::getBooleanValue(
                  slice, "error", false)) {
            tmpHaveError = true;
            tmpMsg += " shardID:" + p.first + ":";
            tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(
                slice, "errorMessage", "");
            if (slice.hasKey("errorNum")) {
              VPackSlice const errorNum = slice.get("errorNum");
              if (errorNum.isNumber()) {
                tmpMsg += " (errNum=";
                tmpMsg += basics::StringUtils::itoa(
                    errorNum.getNumericValue<uint32_t>());
                tmpMsg += ")";
              }
            }
          }
        }
        loadCurrentCollections();
        if (tmpHaveError) {
          errorMsg = "Error in creation of collection:" + tmpMsg;
          return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }

    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", interval);
  }

  // LOG(ERR) << "GOT TIMEOUT. NUMBEROFSHARDS: " << numberOfShards;
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropCollectionCoordinator(std::string const& databaseName,
                                           std::string const& collectionID,
                                           std::string& errorMsg,
                                           double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    if (!ac.exists("Plan/Databases/" + databaseName)) {
      return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    }

    res = ac.removeValues(
        "Plan/Collections/" + databaseName + "/" + collectionID, false);
    if (!res.successful()) {
      if (res._statusCode == (int) GeneralResponse::ResponseCode::NOT_FOUND) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
                         errorMsg);
    }
  }

  // Update our own cache:
  loadPlannedCollections();

  // monitor the entry for the collection
  std::string const where =
      "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      // if there are no more active shards for the collection...
      if (res._values.size() == 0) {
        // ...remove the entire directory for the collection
        AgencyCommLocker locker("Current", "WRITE");
        if (locker.successful()) {
          res.clear();
          res = ac.removeValues(
              "Current/Collections/" + databaseName + "/" + collectionID, true);
          if (res.successful()) {
            return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
          }
          return setErrormsg(
              TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT,
              errorMsg);
        }
        loadCurrentCollections();
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }

    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", interval);
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionPropertiesCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    VocbaseCollectionInfo const* info) {
  AgencyComm ac;
  AgencyCommResult res;

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN;
    }

    if (!ac.exists("Plan/Databases/" + databaseName)) {
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }

    res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID,
                       false);

    if (!res.successful()) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    res.parse("", false);
    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        res._values.begin();

    if (it == res._values.end()) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    if (it->second._vpack == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    VPackSlice const slice = it->second._vpack->slice();
    if (slice.isNone()) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    VPackBuilder copy;
    try {
      VPackObjectBuilder b(&copy);
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key = entry.key.copyString();
        // Copy all values except the following
        // They are overwritten later
        if (key != "doCompact" && key != "journalSize" &&
            key != "waitForSync" && key != "indexBuckets") {
          copy.add(key, entry.value);
        }
      }
      copy.add("doCompact", VPackValue(info->doCompact()));
      copy.add("journalSize", VPackValue(info->maximalSize()));
      copy.add("waitForSync", VPackValue(info->waitForSync()));
      copy.add("indexBuckets", VPackValue(info->indexBuckets()));
    } catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    res.clear();
    res = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID,
                      copy.slice(), 0.0);
  }

  if (res.successful()) {
    loadPlannedCollections();
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionStatusCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    TRI_vocbase_col_status_e status) {
  AgencyComm ac;
  AgencyCommResult res;

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN;
    }

    if (!ac.exists("Plan/Databases/" + databaseName)) {
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }

    res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID,
                       false);

    if (!res.successful()) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    res.parse("", false);
    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        res._values.begin();

    if (it == res._values.end()) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    VPackSlice const slice = it->second._vpack->slice();
    if (slice.isNone()) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    TRI_vocbase_col_status_e old = static_cast<TRI_vocbase_col_status_e>(
        arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            slice, "status", static_cast<int>(TRI_VOC_COL_STATUS_CORRUPTED)));

    if (old == status) {
      // no status change
      return TRI_ERROR_NO_ERROR;
    }

    VPackBuilder builder;
    try {
      VPackObjectBuilder b(&builder);
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key = entry.key.copyString();
        if (key != "status") {
          builder.add(key, entry.value);
        }
      }
      builder.add("status", VPackValue(status));
    } catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    res.clear();
    res = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID,
                      builder.slice(), 0.0);
  }

  if (res.successful()) {
    loadPlannedCollections();
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::ensureIndexCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    VPackSlice const& slice, bool create,
    bool (*compare)(VPackSlice const&, VPackSlice const&),
    VPackBuilder& resultBuilder, std::string& errorMsg, double timeout) {
  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  TRI_ASSERT(resultBuilder.isEmpty());
  int numberOfShards = 0;

  // check index id
  uint64_t iid = 0;

  VPackSlice const idxSlice = slice.get("id");
  if (idxSlice.isString()) {
    // use predefined index id
    iid = arangodb::basics::StringUtils::uint64(idxSlice.copyString());
  }

  if (iid == 0) {
    // no id set, create a new one!
    iid = uniqid();
  }

  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  std::string const key =
      "Plan/Collections/" + databaseName + "/" + collectionID;
  AgencyCommResult previous = ac.getValues(key, false);
  previous.parse("", false);
  auto it = previous._values.begin();
  bool usePrevious = true;
  if (it == previous._values.end()) {
    LOG(INFO) << "Entry for collection in Plan does not exist!";
    usePrevious = false;
  }

  loadPlannedCollections();
  VPackBuilder newBuilder;
  // It is possible that between the fetching of the planned collections
  // and the write lock we acquire below something has changed. Therefore
  // we first get the previous value and then do a compare and swap operation.
  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    std::shared_ptr<VPackBuilder> collectionBuilder;
    {
      std::shared_ptr<CollectionInfo> c =
          getCollection(databaseName, collectionID);

      // Note that nobody is removing this collection in the plan, since
      // we hold the write lock in the agency, therefore it does not matter
      // that getCollection fetches the read lock and releases it before
      // we get it again.
      //
      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);

      if (c->empty()) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }

      std::shared_ptr<VPackBuilder> tmp =
          arangodb::basics::JsonHelper::toVelocyPack(c->getIndexes());
      numberOfShards = c->numberOfShards();
      VPackSlice const indexes = tmp->slice();

      if (indexes.isArray()) {
        VPackSlice const type = slice.get("type");

        if (!type.isString()) {
          return setErrormsg(TRI_ERROR_INTERNAL, errorMsg);
        }

        for (auto const& other : VPackArrayIterator(indexes)) {
          if (arangodb::basics::VelocyPackHelper::compare(
                  type, other.get("type"), false) != 0) {
            // compare index types first. they must match
            continue;
          }
          TRI_ASSERT(other.isObject());

          bool isSame = compare(slice, other);

          if (isSame) {
            // found an existing index...
            {
              // Copy over all elements in slice.
              VPackObjectBuilder b(&resultBuilder);
              for (auto const& entry : VPackObjectIterator(other)) {
                resultBuilder.add(entry.key.copyString(), entry.value);
              }
              resultBuilder.add("isNewlyCreated", VPackValue(false));
            }
            return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
          }
        }
      }

      // no existing index found.
      if (!create) {
        TRI_ASSERT(resultBuilder.isEmpty());
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }

      // now create a new index
      collectionBuilder =
          arangodb::basics::JsonHelper::toVelocyPack(c->getJson());
    }
    VPackSlice const collectionSlice = collectionBuilder->slice();

    if (!collectionSlice.isObject()) {
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }
    try {
      VPackObjectBuilder b(&newBuilder);
      // Create a new collection VPack with the new Index
      for (auto const& entry : VPackObjectIterator(collectionSlice)) {
        TRI_ASSERT(entry.key.isString());
        std::string key = entry.key.copyString();
        if (key == "indexes") {
          TRI_ASSERT(entry.value.isArray());
          newBuilder.add(key, VPackValue(VPackValueType::Array));
          // Copy over all indexes known so far
          for (auto const& idx : VPackArrayIterator(entry.value)) {
            newBuilder.add(idx);
          }
          {
            VPackObjectBuilder ob(&newBuilder);
            // Add the new index ignoring "id"
            for (auto const& e : VPackObjectIterator(slice)) {
              TRI_ASSERT(e.key.isString());
              std::string tmpkey = e.key.copyString();
              if (tmpkey != "id") {
                newBuilder.add(tmpkey, e.value);
              }
            }
            newBuilder.add("id", VPackValue(idString));
          }
          newBuilder.close();  // the array
        } else {
          // Plain copy everything else
          newBuilder.add(key, entry.value);
        }
      }
    } catch (...) {
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    AgencyCommResult result;
    if (usePrevious) {
      result = ac.casValue(key, it->second._vpack->slice(), newBuilder.slice(),
                           0.0, 0.0);
    } else {  // only when there is no previous value
      result = ac.setValue(key, newBuilder.slice(), 0.0);
    }

    if (!result.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                         errorMsg);
    }
  }

  // reload our own cache:
  loadPlannedCollections();

  TRI_ASSERT(numberOfShards > 0);

  // now wait for the index to appear
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == (size_t)numberOfShards) {
        std::map<std::string, AgencyCommResultEntry>::iterator it;

        size_t found = 0;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          VPackSlice const slice = it->second._vpack->slice();
          if (slice.hasKey("indexes")) {
            VPackSlice const indexes = slice.get("indexes");
            if (!indexes.isArray()) {
              // no list, so our index is not present. we can abort searching
              break;
            }

            for (auto const& v : VPackArrayIterator(indexes)) {
              // check for errors
              if (hasError(v)) {
                std::string errorMsg = extractErrorMessage((*it).first, v);

                errorMsg = "Error during index creation: " + errorMsg;

                // Returns the specific error number if set, or the general
                // error
                // otherwise
                return arangodb::basics::VelocyPackHelper::getNumericValue<int>(
                    v, "errorNum", TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
              }

              VPackSlice const k = v.get("id");

              if (!k.isString() || idString != k.copyString()) {
                // this is not our index
                continue;
              }

              // found our index
              found++;
              break;
            }
          }
        }

        if (found == (size_t)numberOfShards) {
          VPackSlice indexFinder = newBuilder.slice();
          TRI_ASSERT(indexFinder.isObject());
          indexFinder = indexFinder.get("indexes");
          TRI_ASSERT(indexFinder.isArray());
          VPackValueLength l = indexFinder.length();
          indexFinder = indexFinder.at(l - 1);  // Get the last index
          TRI_ASSERT(indexFinder.isObject());
          {
            // Copy over all elements in slice.
            VPackObjectBuilder b(&resultBuilder);
            for (auto const& entry : VPackObjectIterator(indexFinder)) {
              resultBuilder.add(entry.key.copyString(), entry.value);
            }
            resultBuilder.add("isNewlyCreated", VPackValue(true));
          }
          loadCurrentCollections();

          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
      }
    }
    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", interval);
  }

  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropIndexCoordinator(std::string const& databaseName,
                                      std::string const& collectionID,
                                      TRI_idx_iid_t iid, std::string& errorMsg,
                                      double timeout) {
  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  int numberOfShards = 0;
  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  std::string const key =
      "Plan/Collections/" + databaseName + "/" + collectionID;
  AgencyCommResult previous = ac.getValues(key, false);
  previous.parse("", false);
  auto it = previous._values.begin();
  TRI_ASSERT(it != previous._values.end());

  loadPlannedCollections();
  // It is possible that between the fetching of the planned collections
  // and the write lock we acquire below something has changed. Therefore
  // we first get the previous value and then do a compare and swap operation.
  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (!locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    TRI_json_t* collectionJson = nullptr;
    TRI_json_t const* indexes = nullptr;

    {
      std::shared_ptr<CollectionInfo> c =
          getCollection(databaseName, collectionID);

      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);

      if (c->empty()) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }

      indexes = c->getIndexes();

      if (!TRI_IsArrayJson(indexes)) {
        // no indexes present, so we can't delete our index
        return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
      }

      collectionJson = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, c->getJson());
      numberOfShards = c->numberOfShards();
    }

    if (collectionJson == nullptr) {
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    TRI_ASSERT(TRI_IsArrayJson(indexes));

    // delete previous indexes entry
    TRI_DeleteObjectJson(TRI_UNKNOWN_MEM_ZONE, collectionJson, "indexes");

    TRI_json_t* copy = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

    if (copy == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    bool found = false;

    // copy remaining indexes back into collection
    size_t const n = TRI_LengthArrayJson(indexes);
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* v = TRI_LookupArrayJson(indexes, i);
      TRI_json_t const* id = TRI_LookupObjectJson(v, "id");
      TRI_json_t const* type = TRI_LookupObjectJson(v, "type");

      if (!TRI_IsStringJson(id) || !TRI_IsStringJson(type)) {
        continue;
      }

      if (idString == std::string(id->_value._string.data)) {
        // found our index, ignore it when copying
        found = true;

        std::string const typeString(type->_value._string.data);
        if (typeString == "primary" || typeString == "edge") {
          // must not delete these index types
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, copy);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
          return setErrormsg(TRI_ERROR_FORBIDDEN, errorMsg);
        }

        continue;
      }

      TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, copy,
                             TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, v));
    }

    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, collectionJson, "indexes",
                          copy);

    if (!found) {
      // did not find the sought index
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
    }

    auto tmp = arangodb::basics::JsonHelper::toVelocyPack(collectionJson);
    AgencyCommResult result =
        ac.casValue(key, it->second._vpack->slice(), tmp->slice(), 0.0, 0.0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);

    if (!result.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                         errorMsg);
    }
  }

  // load our own cache:
  loadPlannedCollections();

  TRI_ASSERT(numberOfShards > 0);

  // now wait for the index to disappear
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == (size_t)numberOfShards) {
        std::map<std::string, AgencyCommResultEntry>::iterator it;

        bool found = false;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          VPackSlice const slice = it->second._vpack->slice();
          VPackSlice const indexes = slice.get("indexes");

          if (indexes.isArray()) {
            for (auto const& v : VPackArrayIterator(indexes)) {
              if (v.isObject()) {
                VPackSlice const k = v.get("id");
                if (k.isString() && idString == k.copyString()) {
                  // still found the index in some shard
                  found = true;
                  break;
                }
              }

              if (found) {
                break;
              }
            }
          }
        }

        if (!found) {
          loadCurrentCollections();
          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
      }
    }

    res.clear();
    _agencyCallbackRegistry->awaitNextChange("Current/Version", interval);
  }

  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixServers = "Current/ServersRegistered";

void ClusterInfo::loadServers() {
  uint64_t storedVersion = _serversProt.version;
  MUTEX_LOCKER(mutexLocker, _serversProt.mutex);
  if (_serversProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixServers, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixServers + "/", false);

    decltype(_servers) newServers;

    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.begin();

    while (it != result._values.end()) {
      VPackSlice const slice = it->second._vpack->slice();
      if (slice.isObject() && slice.hasKey("endpoint")) {
        std::string server = arangodb::basics::VelocyPackHelper::getStringValue(
            slice, "endpoint", "");
        newServers.emplace(std::make_pair((*it).first, server));
      }
      ++it;
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _serversProt.lock);
      _servers.swap(newServers);
      _serversProt.version++;       // such that others notice our change
      _serversProt.isValid = true;  // will never be reset to false
    }
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixServers
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint(ServerID const& serverID) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);
      // _servers is a map-type <ServerId, std::string>
      auto it = _servers.find(serverID);

      if (it != _servers.end()) {
        return (*it).second;
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the ID of a server from its endpoint.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerName(std::string const& endpoint) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);
      for (auto const& it : _servers) {
        if (it.second == endpoint) {
          return it.first;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all coordinators from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentCoordinators = "Current/Coordinators";

void ClusterInfo::loadCurrentCoordinators() {
  uint64_t storedVersion = _coordinatorsProt.version;
  MUTEX_LOCKER(mutexLocker, _coordinatorsProt.mutex);
  if (_coordinatorsProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixCurrentCoordinators, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixCurrentCoordinators + "/", false);

    decltype(_coordinators) newCoordinators;

    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.begin();

    for (; it != result._values.end(); ++it) {
      VPackSlice const slice = it->second._vpack->slice();
      newCoordinators.emplace(std::make_pair(
          (*it).first,
          arangodb::basics::VelocyPackHelper::getStringValue(slice, "")));
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
      _coordinators.swap(newCoordinators);
      _coordinatorsProt.version++;       // such that others notice our change
      _coordinatorsProt.isValid = true;  // will never be reset to false
    }
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixCurrentCoordinators
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentDBServers = "Current/DBServers";

void ClusterInfo::loadCurrentDBServers() {
  uint64_t storedVersion = _DBServersProt.version;
  MUTEX_LOCKER(mutexLocker, _DBServersProt.mutex);
  if (_DBServersProt.version > storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result;
  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixCurrentDBServers, true);
    }
  }

  if (result.successful()) {
    result.parse(prefixCurrentDBServers + "/", false);

    decltype(_DBServers) newDBServers;

    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.begin();

    for (; it != result._values.end(); ++it) {
      VPackSlice const slice = it->second._vpack->slice();
      newDBServers.emplace(std::make_pair(
          (*it).first,
          arangodb::basics::VelocyPackHelper::getStringValue(slice, "")));
    }

    // Now set the new value:
    {
      WRITE_LOCKER(writeLocker, _DBServersProt.lock);
      _DBServers.swap(newDBServers);
      _DBServersProt.version++;       // such that others notice our change
      _DBServersProt.isValid = true;  // will never be reset to false
    }
    return;
  }

  LOG(DEBUG) << "Error while loading " << prefixCurrentDBServers
             << " httpCode: " << result.httpCode()
             << " errorCode: " << result.errorCode()
             << " errorMessage: " << result.errorMessage()
             << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers() {
  std::vector<ServerID> result;
  int tries = 0;

  if (!_DBServersProt.isValid) {
    loadCurrentDBServers();
    tries++;
  }
  while (true) {
    {
      // return a consistent state of servers
      READ_LOCKER(readLocker, _DBServersProt.lock);

      result.reserve(_DBServers.size());

      for (auto& it : _DBServers) {
        result.emplace_back(it.first);
      }

      return result;
    }

    if (++tries >= 2) {
      break;
    }

    // loadCurrentDBServers needs the write lock
    loadCurrentDBServers();
  }

  // note that the result will be empty if we get here
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup the server's endpoint by scanning Target/MapIDToEnpdoint for
/// our id
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixTargetServerEndpoint = "Target/MapIDToEndpoint/";

std::string ClusterInfo::getTargetServerEndpoint(ServerID const& serverID) {
  AgencyCommResult result;

  // fetch value at Target/MapIDToEndpoint
  {
    AgencyCommLocker locker("Target", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefixTargetServerEndpoint + serverID, false);
    }
  }

  if (result.successful()) {
    result.parse(prefixTargetServerEndpoint, false);

    // check if we can find ourselves in the list returned by the agency
    std::map<std::string, AgencyCommResultEntry>::const_iterator it =
        result._values.find(serverID);

    if (it != result._values.end()) {
      VPackSlice const slice = it->second._vpack->slice();
      return arangodb::basics::VelocyPackHelper::getStringValue(slice, "");
    }
  }

  // not found
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the servers who are responsible for a shard (one leader
/// and multiple followers)
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ServerID>> ClusterInfo::getResponsibleServer(
    ShardID const& shardID) {
  int tries = 0;

  if (!_currentCollectionsProt.isValid) {
    loadCurrentCollections();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentCollectionsProt.lock);
      // _shardIds is a map-type <ShardId,
      // std::shared_ptr<std::vector<ServerId>>>
      auto it = _shardIds.find(shardID);

      if (it != _shardIds.end()) {
        return (*it).second;
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadCurrentCollections();
  }

  return std::make_shared<std::vector<ServerID>>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard list of a collection, sorted numerically
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ShardID>> ClusterInfo::getShardList(
    CollectionID const& collectionID) {
  if (!_plannedCollectionsProt.isValid) {
    loadPlannedCollections();
  }

  int tries = 0;
  while (true) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
      // _shards is a map-type <CollectionId, shared_ptr<vector<string>>>
      auto it = _shards.find(collectionID);

      if (it != _shards.end()) {
        return it->second;
      }
    }
    if (++tries >= 2) {
      return std::make_shared<std::vector<ShardID>>();
    }
    loadPlannedCollections();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard that is responsible for a document, which is given
/// as a VelocyPackSlice.
///
/// There are two modes, one assumes that the document is given as a
/// whole (`docComplete`==`true`), in this case, the non-existence of
/// values for some of the sharding attributes is silently ignored
/// and treated as if these values were `null`. In the second mode
/// (`docComplete`==false) leads to an error which is reported by
/// returning TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, which is the only
/// error code that can be returned.
///
/// In either case, if the collection is found, the variable
/// shardID is set to the ID of the responsible shard and the flag
/// `usesDefaultShardingAttributes` is used set to `true` if and only if
/// `_key` is the one and only sharding attribute.
////////////////////////////////////////////////////////////////////////////////


int ClusterInfo::getResponsibleShard(CollectionID const& collectionID,
                                     VPackSlice slice, bool docComplete,
                                     ShardID& shardID,
                                     bool& usesDefaultShardingAttributes) {
  // Note that currently we take the number of shards and the shardKeys
  // from Plan, since they are immutable. Later we will have to switch
  // this to Current, when we allow to add and remove shards.
  if (!_plannedCollectionsProt.isValid) {
    loadPlannedCollections();
  }

  int tries = 0;
  std::shared_ptr<std::vector<std::string>> shardKeysPtr;
  std::shared_ptr<std::vector<ShardID>> shards;
  bool found = false;

  while (true) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
      // _shards is a map-type <CollectionId, shared_ptr<vector<string>>>
      auto it = _shards.find(collectionID);

      if (it != _shards.end()) {
        shards = it->second;
        // _shardKeys is a map-type <CollectionID, shared_ptr<vector<string>>>
        auto it2 = _shardKeys.find(collectionID);
        if (it2 != _shardKeys.end()) {
          shardKeysPtr = it2->second;
          usesDefaultShardingAttributes =
              shardKeysPtr->size() == 1 &&
              shardKeysPtr->at(0) == TRI_VOC_ATTRIBUTE_KEY;
          found = true;
          break;  // all OK
        }
      }
    }
    if (++tries >= 2) {
      break;
    }
    loadPlannedCollections();
  }

  if (!found) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int error = TRI_ERROR_NO_ERROR;
  uint64_t hash = arangodb::basics::VelocyPackHelper::hashByAttributes(
      slice, *shardKeysPtr, docComplete, error);
  static char const* magicPhrase =
      "Foxx you have stolen the goose, give she back again!";
  static size_t const len = 52;
  // To improve our hash function:
  hash = TRI_FnvHashBlock(hash, magicPhrase, len);

  shardID = shards->at(hash % shards->size());
  return error;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard that is responsible for a document, which is given
/// as a TRI_json_t const*.
///
/// There are two modes, one assumes that the document is given as a
/// whole (`docComplete`==`true`), in this case, the non-existence of
/// values for some of the sharding attributes is silently ignored
/// and treated as if these values were `null`. In the second mode
/// (`docComplete`==false) leads to an error which is reported by
/// returning TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, which is the only
/// error code that can be returned.
///
/// In either case, if the collection is found, the variable
/// shardID is set to the ID of the responsible shard and the flag
/// `usesDefaultShardingAttributes` is used set to `true` if and only if
/// `_key` is the one and only sharding attribute.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::getResponsibleShard(CollectionID const& collectionID,
                                     TRI_json_t const* json, bool docComplete,
                                     ShardID& shardID,
                                     bool& usesDefaultShardingAttributes) {
  // Note that currently we take the number of shards and the shardKeys
  // from Plan, since they are immutable. Later we will have to switch
  // this to Current, when we allow to add and remove shards.
  if (!_plannedCollectionsProt.isValid) {
    loadPlannedCollections();
  }

  int tries = 0;
  std::shared_ptr<std::vector<std::string>> shardKeysPtr;
  std::unique_ptr<char const* []> shardKeys;
  std::shared_ptr<std::vector<ShardID>> shards;
  bool found = false;

  while (true) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(readLocker, _plannedCollectionsProt.lock);
      // _shards is a map-type <CollectionId, shared_ptr<vector<string>>>
      auto it = _shards.find(collectionID);

      if (it != _shards.end()) {
        shards = it->second;
        // _shardKeys is a map-type <CollectionID, shared_ptr<vector<string>>>
        auto it2 = _shardKeys.find(collectionID);
        if (it2 != _shardKeys.end()) {
          shardKeysPtr = it2->second;
          shardKeys.reset(new char const*[shardKeysPtr->size()]);
          size_t i;
          for (i = 0; i < shardKeysPtr->size(); ++i) {
            shardKeys[i] = shardKeysPtr->at(i).c_str();
          }
          usesDefaultShardingAttributes =
              shardKeysPtr->size() == 1 &&
              shardKeysPtr->at(0) == TRI_VOC_ATTRIBUTE_KEY;
          found = true;
          break;  // all OK
        }
      }
    }
    if (++tries >= 2) {
      break;
    }
    loadPlannedCollections();
  }

  if (!found) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int error;
  uint64_t hash = TRI_HashJsonByAttributes(
      json, shardKeys.get(), (int)shardKeysPtr->size(), docComplete, error);
  static char const* magicPhrase =
      "Foxx you have stolen the goose, give she back again!";
  static size_t const len = 52;
  // To improve our hash function:
  hash = TRI_FnvHashBlock(hash, magicPhrase, len);

  shardID = shards->at(hash % shards->size());
  return error;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of coordinator server names
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentCoordinators() {
  std::vector<ServerID> result;
  int tries = 0;

  if (!_coordinatorsProt.isValid) {
    loadCurrentCoordinators();
    tries++;
  }
  while (true) {
    {
      // return a consistent state of servers
      READ_LOCKER(readLocker, _coordinatorsProt.lock);

      result.reserve(_coordinators.size());

      for (auto& it : _coordinators) {
        result.emplace_back(it.first);
      }

      return result;
    }

    if (++tries >= 2) {
      break;
    }

    // loadCurrentCoordinators needs the write lock
    loadCurrentCoordinators();
  }

  // note that the result will be empty if we get here
  return result;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current
//////////////////////////////////////////////////////////////////////////////
void ClusterInfo::invalidateCurrent() {
  WRITE_LOCKER(writeLocker, _currentCollectionsProt.lock);
  _currentCollectionsProt.isValid = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get information about current followers of a shard.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ServerID> const> FollowerInfo::get() {
  MUTEX_LOCKER(locker, _mutex);
  return _followers;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change JSON under
/// Current/Collection/<DB-name>/<Collection-ID>/<shard-ID>
/// to add or remove a serverID, if add flag is true, the entry is added
/// (if it is not yet there), otherwise the entry is removed (if it was
/// there).
////////////////////////////////////////////////////////////////////////////////

VPackBuilder newShardEntry(VPackSlice oldValue, ServerID const& sid, bool add) {
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
  MUTEX_LOCKER(locker, _mutex);
  
  // Fully copy the vector:
  auto v = std::make_shared<std::vector<ServerID>>(*_followers);
  v->push_back(sid);  // add a single entry
  _followers = v;     // will cast to std::vector<ServerID> const
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->_vocbase->_name;
  path += "/";
  path += std::to_string(_docColl->_info.planId());
  path += "/";
  path += _docColl->_info.name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path, false);
    if (res.successful()) {
      if (res.parse("", false)) {
        auto it = res._values.begin();
        if (it != res._values.end() && it->first == path) {
          VPackSlice oldValue = it->second._vpack->slice();
          auto newValue = newShardEntry(oldValue, sid, true);
          AgencyCommResult res2 =
              ac.casValue(path, oldValue, newValue.slice(), 0, 0);
          if (res2.successful()) {
            success = true;
            break;  //
          } else {
            LOG(WARN) << "FollowerInfo::add, could not cas key " << path;
          }
        } else {
          LOG(ERR) << "FollowerInfo::add, did not find key " << path
                   << " in agency.";
        }
      } else {
        LOG(ERR) << "FollowerInfo::add, could not parse " << path
                 << " in agency.";
      }
    } else {
      LOG(ERR) << "FollowerInfo::add, could not read " << path << " in agency.";
    }
    usleep(500000);
  } while (TRI_microtime() < startTime + 30);
  if (!success) {
    LOG(ERR) << "FollowerInfo::add, timeout in agency operation for key "
             << path;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower from a shard, this is only done by the
/// server if a synchronous replication request fails. This reports to
/// the agency under `/Current` but in asynchronous "fire-and-forget"
/// way. The method fails silently, if the follower information has
/// since been dropped (see `dropFollowerInfo` below).
////////////////////////////////////////////////////////////////////////////////

void FollowerInfo::remove(ServerID const& sid) {
  MUTEX_LOCKER(locker, _mutex);
  
  auto v = std::make_shared<std::vector<ServerID>>();
  v->reserve(_followers->size() - 1);
  for (auto const& i : *_followers) {
    if (i != sid) {
      v->push_back(i);
    }
  }
  _followers = v;  // will cast to std::vector<ServerID> const
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->_vocbase->_name;
  path += "/";
  path += std::to_string(_docColl->_info.planId());
  path += "/";
  path += _docColl->_info.name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path, false);
    if (res.successful()) {
      if (res.parse("", false)) {
        auto it = res._values.begin();
        if (it != res._values.end() && it->first == path) {
          VPackSlice oldValue = it->second._vpack->slice();
          auto newValue = newShardEntry(oldValue, sid, false);
          AgencyCommResult res2 =
              ac.casValue(path, oldValue, newValue.slice(), 0, 0);
          if (res2.successful()) {
            success = true;
            break;  //
          } else {
            LOG(WARN) << "FollowerInfo::remove, could not cas key " << path;
          }
        } else {
          LOG(ERR) << "FollowerInfo::remove, did not find key " << path
                   << " in agency.";
        }
      } else {
        LOG(ERR) << "FollowerInfo::remove, could not parse " << path
                 << " in agency.";
      }
    } else {
      LOG(ERR) << "FollowerInfo::remove, could not read " << path
               << " in agency.";
    }
    usleep(500000);
  } while (TRI_microtime() < startTime + 30);
  if (!success) {
    LOG(ERR) << "FollowerInfo::remove, timeout in agency operation for key "
             << path;
  }
}
