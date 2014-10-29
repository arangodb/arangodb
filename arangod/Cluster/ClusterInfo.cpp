////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterInfo.cpp
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterInfo.h"

#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"
#include "Basics/json-utilities.h"
#include "Basics/JsonHelper.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/StringUtils.h"
#include "VocBase/server.h"

using namespace std;
using namespace triagens::arango;
using triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a local helper to report errors and messages
////////////////////////////////////////////////////////////////////////////////

static inline int setErrormsg (int ourerrno, string& errorMsg) {
  errorMsg = TRI_errno_string(ourerrno);
  return ourerrno;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the JSON returns an error
////////////////////////////////////////////////////////////////////////////////

static inline bool hasError (TRI_json_t const* json) {
  TRI_json_t const* error = TRI_LookupArrayJson(json, "error");

  return (TRI_IsBooleanJson(error) && error->_value._boolean);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static string extractErrorMessage (string const& shardId,
                                   TRI_json_t const* json) {
  string msg = " shardID:" + shardId + ": ";

  // add error message text
  TRI_json_t const* errorMessage = TRI_LookupArrayJson(json, "errorMessage");
  if (TRI_IsStringJson(errorMessage)) {
    msg += string(errorMessage->_value._string.data,
                  errorMessage->_value._string.length - 1);
  }

  // add error number
  TRI_json_t const* errorNum = TRI_LookupArrayJson(json, "errorNum");
  if (TRI_IsNumberJson(errorNum)) {
    msg += " (errNum=" + triagens::basics::StringUtils::itoa(static_cast<uint32_t>(errorNum->_value._number)) + ")";
  }

  return msg;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              CollectionInfo class
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo ()
  : _json(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo (TRI_json_t* json)
  : _json(json) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo (CollectionInfo const& other) :
  _json(other._json) {

  if (other._json != nullptr) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo& CollectionInfo::operator= (CollectionInfo const& other) {
  if (other._json != nullptr && this != &other) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  }
  else {
    _json = nullptr;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::~CollectionInfo () {
  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                       CollectionInfoCurrent class
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent (ShardID const& shardID, TRI_json_t* json) {
  _jsons.insert(make_pair(shardID, json));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent (CollectionInfoCurrent const& other) :
  _jsons(other._jsons) {
  copyAllJsons();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent& CollectionInfoCurrent::operator= (CollectionInfoCurrent const& other) {
  if (this == &other) {
    return *this;
  }
  freeAllJsons();
  _jsons = other._jsons;
  copyAllJsons();
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::~CollectionInfoCurrent () {
  freeAllJsons();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free all pointers to TRI_json_t in the map _jsons
////////////////////////////////////////////////////////////////////////////////

void CollectionInfoCurrent::freeAllJsons () {
  map<ShardID, TRI_json_t*>::iterator it;
  for (it = _jsons.begin(); it != _jsons.end(); ++it) {
    if (it->second != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it->second);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy TRI_json_t behind the pointers in the map _jsons
////////////////////////////////////////////////////////////////////////////////

void CollectionInfoCurrent::copyAllJsons () {
  map<ShardID, TRI_json_t*>::iterator it;
  for (it = _jsons.begin(); it != _jsons.end(); ++it) {
    if (nullptr != it->second) {
      it->second = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an instance of the cluster info class
////////////////////////////////////////////////////////////////////////////////

ClusterInfo* ClusterInfo::instance () {
  static ClusterInfo* Instance = new ClusterInfo();
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the cluster info singleton object
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::initialise () {
  instance();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::cleanup () {
  auto i = instance();
  TRI_ASSERT(i != nullptr);

  delete i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::ClusterInfo ()
  : _agency(),
    _uniqid(),
    _plannedDatabases(),
    _currentDatabases(),
    _collectionsValid(false),
    _serversValid(false),
    _DBServersValid(false) {
  _uniqid._currentValue = _uniqid._upperValue = 0ULL;

  // Actual loading into caches is postponed until necessary
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo () {
  clearPlannedDatabases();
  clearCurrentDatabases();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

uint64_t ClusterInfo::uniqid (uint64_t count) {
  WRITE_LOCKER(_lock);

  if (_uniqid._currentValue >= _uniqid._upperValue) {
    uint64_t fetch = count;

    if (fetch < MinIdsPerBatch) {
      fetch = MinIdsPerBatch;
    }

    AgencyCommResult result = _agency.uniqid("Sync/LatestID", fetch, 0.0);

    if (! result.successful() || result._index == 0) {
      return 0;
    }

    _uniqid._currentValue = result._index + count;
    _uniqid._upperValue   = _uniqid._currentValue + fetch - 1;

    return result._index;
  }

  uint64_t result = _uniqid._currentValue;
  _uniqid._currentValue += count;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::flush () {
  WRITE_LOCKER(_lock);

  _collectionsValid = false;
  _collectionsCurrentValid = false;
  _serversValid = false;
  _DBServersValid = false;

  _collections.clear();
  _collectionsCurrent.clear();
  _servers.clear();
  _shardIds.clear();

  clearPlannedDatabases();
  clearCurrentDatabases();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist (DatabaseID const& databaseID,
                                     bool reload) {
  int tries = 0;

  if (reload) {
    loadPlannedDatabases();
    loadCurrentDatabases();
    loadCurrentDBServers();
    ++tries;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      const size_t expectedSize = _DBServers.size();

      // look up database by name

      std::map<DatabaseID, TRI_json_t*>::const_iterator it = _plannedDatabases.find(databaseID);

      if (it != _plannedDatabases.end()) {
        // found the database in Plan
        std::map<DatabaseID, std::map<ServerID, TRI_json_t*> >::const_iterator it2 = _currentDatabases.find(databaseID);

        if (it2 != _currentDatabases.end()) {
          // found the database in Current

          return ((*it2).second.size() >= expectedSize);
        }
      }
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

vector<DatabaseID> ClusterInfo::listDatabases (bool reload) {
  vector<DatabaseID> result;

  if (reload) {
    loadPlannedDatabases();
    loadCurrentDatabases();
    loadCurrentDBServers();
  }

  READ_LOCKER(_lock);
  const size_t expectedSize = _DBServers.size();

  std::map<DatabaseID, TRI_json_t*>::const_iterator it = _plannedDatabases.begin();

  while (it != _plannedDatabases.end()) {
    std::map<DatabaseID, std::map<ServerID, TRI_json_t*> >::const_iterator it2 = _currentDatabases.find((*it).first);

    if (it2 != _currentDatabases.end()) {
      if ((*it2).second.size() >= expectedSize) {
        result.push_back((*it).first);
      }
    }

    ++it;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of planned databases
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::clearPlannedDatabases () {
  std::map<DatabaseID, TRI_json_t*>::iterator it = _plannedDatabases.begin();

  while (it != _plannedDatabases.end()) {
    TRI_json_t* json = (*it).second;

    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    ++it;
  }

  _plannedDatabases.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the list of current databases
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::clearCurrentDatabases () {
  std::map<DatabaseID, std::map<ServerID, TRI_json_t*> >::iterator it = _currentDatabases.begin();

  while (it != _currentDatabases.end()) {
    std::map<ServerID, TRI_json_t*>::iterator it2 = (*it).second.begin();

    while (it2 != (*it).second.end()) {
      TRI_json_t* json = (*it2).second;

      if (json != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }

      ++it2;
    }
    ++it;
  }

  _currentDatabases.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about planned databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadPlannedDatabases () {
  static const std::string prefix = "Plan/Databases";

  AgencyCommResult result;

  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    clearPlannedDatabases();

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    while (it != result._values.end()) {
      string const& name = (*it).first;
      TRI_json_t* options = (*it).second._json;

      // steal the json
      (*it).second._json = nullptr;
      _plannedDatabases.insert(std::make_pair(name, options));

      ++it;
    }

    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadCurrentDatabases () {
  static const std::string prefix = "Current/Databases";

  AgencyCommResult result;

  {
    AgencyCommLocker locker("Plan", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    clearCurrentDatabases();

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    while (it != result._values.end()) {
      const std::string key = (*it).first;

      // each entry consists of a database id and a collection id, separated by '/'
      std::vector<std::string> parts = triagens::basics::StringUtils::split(key, '/');

      if (parts.empty()) {
        ++it;
        continue;
      }
      const std::string database = parts[0];

      std::map<std::string, std::map<ServerID, TRI_json_t*> >::iterator it2 = _currentDatabases.find(database);

      if (it2 == _currentDatabases.end()) {
        // insert an empty list for this database
        std::map<ServerID, TRI_json_t*> empty;
        it2 = _currentDatabases.insert(std::make_pair(database, empty)).first;
      }

      if (parts.size() == 2) {
        // got a server name
        TRI_json_t* json = (*it).second._json;
        // steal the JSON
        (*it).second._json = nullptr;
        (*it2).second.insert(std::make_pair(parts[1], json));
      }

      ++it;
    }

    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadPlannedCollections (bool acquireLock) {
  static const std::string prefix = "Plan/Collections";

  AgencyCommResult result;

  {
    if (acquireLock) {
      AgencyCommLocker locker("Plan", "READ");

      if (locker.successful()) {
        result = _agency.getValues(prefix, true);
      }
    }
    else {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    _collections.clear();
    _shards.clear();

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    for (; it != result._values.end(); ++it) {
      const std::string key = (*it).first;

      // each entry consists of a database id and a collection id, separated by '/'
      std::vector<std::string> parts = triagens::basics::StringUtils::split(key, '/');

      if (parts.size() != 2) {
        // invalid entry
        LOG_WARNING("found invalid collection key in agency: '%s'", key.c_str());
        continue;
      }

      const std::string database   = parts[0];
      const std::string collection = parts[1];

      // check whether we have created an entry for the database already
      AllCollections::iterator it2 = _collections.find(database);

      if (it2 == _collections.end()) {
        // not yet, so create an entry for the database
        DatabaseCollections empty;
        _collections.emplace(std::make_pair(database, empty));
        it2 = _collections.find(database);
      }

      TRI_json_t* json = (*it).second._json;
      // steal the json
      (*it).second._json = nullptr;

      shared_ptr<CollectionInfo> collectionData (new CollectionInfo(json));
      vector<string>* shardKeys = new vector<string>;
      *shardKeys = collectionData->shardKeys();
      _shardKeys.insert(
                    make_pair(collection, shared_ptr<vector<string> > (shardKeys)));
      map<ShardID, ServerID> shardIDs = collectionData->shardIds();
      vector<string>* shards = new vector<string>;
      map<ShardID, ServerID>::iterator it3;
      for (it3 = shardIDs.begin(); it3 != shardIDs.end(); ++it3) {
        shards->push_back(it3->first);
      }
      _shards.emplace(
              std::make_pair(collection, shared_ptr<vector<string> >(shards)));

      // insert the collection into the existing map, insert it under its
      // ID as well as under its name, so that a lookup can be done with
      // either of the two.

      (*it2).second.emplace(std::make_pair(collection, collectionData));
      (*it2).second.emplace(std::make_pair(collectionData->name(),
                                           collectionData));

    }
    _collectionsValid = true;
    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());
  _collectionsValid = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
////////////////////////////////////////////////////////////////////////////////

shared_ptr<CollectionInfo> ClusterInfo::getCollection
                                          (DatabaseID const& databaseID,
                                           CollectionID const& collectionID) {
  int tries = 0;

  if (! _collectionsValid) {
    loadPlannedCollections(true);
    ++tries;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      // look up database by id
      AllCollections::const_iterator it = _collections.find(databaseID);

      if (it != _collections.end()) {
        // look up collection by id (or by name)
        DatabaseCollections::const_iterator it2 = (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }

    // must load collections outside the lock
    loadPlannedCollections(true);
  }

  return shared_ptr<CollectionInfo>(new CollectionInfo());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_col_info_t ClusterInfo::getCollectionProperties (CollectionInfo const& collection) {
  TRI_col_info_t info;

  info._type        = collection.type();
  info._cid         = collection.id();
  info._revision    = 0; // TODO
  info._maximalSize = collection.journalSize();

  const std::string name = collection.name();
  memcpy(info._name, name.c_str(), name.size());
  info._deleted     = collection.deleted();
  info._doCompact   = collection.doCompact();
  info._isSystem    = collection.isSystem();
  info._isVolatile  = collection.isVolatile();
  info._waitForSync = collection.waitForSync();
  info._keyOptions = collection.keyOptions();

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_col_info_t ClusterInfo::getCollectionProperties (DatabaseID const& databaseID,
                                                     CollectionID const& collectionID) {
  shared_ptr<CollectionInfo> ci = getCollection(databaseID, collectionID);

  return getCollectionProperties(*ci);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

const std::vector<shared_ptr<CollectionInfo> > ClusterInfo::getCollections
                         (DatabaseID const& databaseID) {
  std::vector<shared_ptr<CollectionInfo> > result;

  // always reload
  loadPlannedCollections(true);

  READ_LOCKER(_lock);
  // look up database by id
  AllCollections::const_iterator it = _collections.find(databaseID);

  if (it == _collections.end()) {
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

void ClusterInfo::loadCurrentCollections (bool acquireLock) {
  static const std::string prefix = "Current/Collections";

  AgencyCommResult result;

  {
    if (acquireLock) {
      AgencyCommLocker locker("Current", "READ");

      if (locker.successful()) {
        result = _agency.getValues(prefix, true);
      }
    }
    else {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    _collectionsCurrent.clear();
    _shardIds.clear();

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    for (; it != result._values.end(); ++it) {
      const std::string key = (*it).first;

      // each entry consists of a database id, a collection id, and a shardID,
      // separated by '/'
      std::vector<std::string> parts = triagens::basics::StringUtils::split(key, '/');

      if (parts.size() != 3) {
        // invalid entry
        LOG_WARNING("found invalid collection key in current in agency: '%s'", key.c_str());
        continue;
      }

      const std::string database   = parts[0];
      const std::string collection = parts[1];
      const std::string shardID    = parts[2];

      // check whether we have created an entry for the database already
      AllCollectionsCurrent::iterator it2 = _collectionsCurrent.find(database);

      if (it2 == _collectionsCurrent.end()) {
        // not yet, so create an entry for the database
        DatabaseCollectionsCurrent empty;
        _collectionsCurrent.insert(std::make_pair(database, empty));
        it2 = _collectionsCurrent.find(database);
      }

      TRI_json_t* json = (*it).second._json;
      // steal the json
      (*it).second._json = 0;

      // check whether we already have a CollectionInfoCurrent:
      DatabaseCollectionsCurrent::iterator it3;
      it3 = it2->second.find(collection);
      if (it3 == it2->second.end()) {
        shared_ptr<CollectionInfoCurrent> collectionDataCurrent
                    (new CollectionInfoCurrent(shardID, json));
        it2->second.insert(make_pair(collection, collectionDataCurrent));
        it3 = it2->second.find(collection);
      }
      else {
        it3->second->add(shardID, json);
      }

      // Note that we have only inserted the CollectionInfoCurrent under
      // the collection ID and not under the name! It is not possible
      // to query the current collection info by name. This is because
      // the correct place to hold the current name is in the plan.
      // Thus: Look there and get the collection ID from there. Then
      // ask about the current collection info.

      // Now take note of this shard and its responsible server:
      std::string DBserver = triagens::basics::JsonHelper::getStringValue
                    (json, "DBServer", "");
      if (DBserver != "") {
        _shardIds.insert(make_pair(shardID, DBserver));
      }
    }
    _collectionsCurrentValid = true;
    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());
  _collectionsCurrentValid = false;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

shared_ptr<CollectionInfoCurrent> ClusterInfo::getCollectionCurrent
           (DatabaseID const& databaseID,
            CollectionID const& collectionID) {
  int tries = 0;

  if (! _collectionsCurrentValid) {
    loadCurrentCollections(true);
    ++tries;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      // look up database by id
      AllCollectionsCurrent::const_iterator it = _collectionsCurrent.find(databaseID);

      if (it != _collectionsCurrent.end()) {
        // look up collection by id
        DatabaseCollectionsCurrent::const_iterator it2 = (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }

    // must load collections outside the lock
    loadCurrentCollections(true);
  }

  return shared_ptr<CollectionInfoCurrent>(new CollectionInfoCurrent());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createDatabaseCoordinator (string const& name,
                                            TRI_json_t const* json,
                                            string& errorMsg,
                                            double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    res = ac.casValue("Plan/Databases/"+name, json, false, 0.0, realTimeout);
    if (! res.successful()) {
      if (res._statusCode == triagens::rest::HttpResponse::PRECONDITION_FAILED) {
        return setErrormsg(TRI_ERROR_ARANGO_DUPLICATE_NAME, errorMsg);
      }

      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN,
                         errorMsg);
    }
  }

  // Now wait for it to appear and be complete:
  res.clear();
  res = ac.getValues("Current/Version", false);
  if (! res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  uint64_t index = res._index;

  vector<ServerID> DBServers = getCurrentDBServers();
  int count = 0;  // this counts, when we have to reload the DBServers

  string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      if (res._values.size() == DBServers.size()) {
        map<string, AgencyCommResultEntry>::iterator it;
        string tmpMsg = "";
        bool tmpHaveError = false;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          TRI_json_t const* json = (*it).second._json;
          TRI_json_t const* error = TRI_LookupArrayJson(json, "error");
          if (TRI_IsBooleanJson(error) && error->_value._boolean) {
            tmpHaveError = true;
            tmpMsg += " DBServer:"+it->first+":";
            TRI_json_t const* errorMessage
                  = TRI_LookupArrayJson(json, "errorMessage");
            if (TRI_IsStringJson(errorMessage)) {
              tmpMsg += string(errorMessage->_value._string.data,
                               errorMessage->_value._string.length-1);
            }
            TRI_json_t const* errorNum = TRI_LookupArrayJson(json, "errorNum");
            if (TRI_IsNumberJson(errorNum)) {
              tmpMsg += " (errorNum=";
              tmpMsg += basics::StringUtils::itoa(static_cast<uint32_t>(
                          errorNum->_value._number));
              tmpMsg += ")";
            }
          }
        }
        if (tmpHaveError) {
          errorMsg = "Error in creation of database:" + tmpMsg;
          return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE;
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }

    res.clear();
    res = ac.watchValue("Current/Version", index, getReloadServerListTimeout() / interval, false);
    index = res._index;
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

int ClusterInfo::dropDatabaseCoordinator (string const& name, string& errorMsg,
                                          double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    if (! ac.exists("Plan/Databases/" + name)) {
      return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    }

    res = ac.removeValues("Plan/Databases/"+name, false);
    if (!res.successful()) {
      if (res.httpCode() == (int) rest::HttpResponse::NOT_FOUND) {
        return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
      }

      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,
                          errorMsg);
    }

    res.clear();
    res = ac.removeValues("Plan/Collections/" + name, true);

    if (! res.successful() && res.httpCode() != (int) rest::HttpResponse::NOT_FOUND) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,
                         errorMsg);
    }
  }

  _collectionsValid = false;

  // Now wait for it to appear and be complete:
  res.clear();
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                        errorMsg);
  }
  uint64_t index = res._index;

  string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
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
    res = ac.watchValue("Current/Version", index, interval, false);
    index = res._index;
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createCollectionCoordinator (string const& databaseName,
                                              string const& collectionID,
                                              uint64_t numberOfShards,
                                              TRI_json_t const* json,
                                              string& errorMsg, double timeout) {
  AgencyComm ac;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");


    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    {
      // check if a collection with the same name is already planned
      loadPlannedCollections(false);

      READ_LOCKER(_lock);
      AllCollections::const_iterator it = _collections.find(databaseName);
      if (it != _collections.end()) {
        const std::string name = JsonHelper::getStringValue(json, "name", "");

        DatabaseCollections::const_iterator it2 = (*it).second.find(name);

        if (it2 != (*it).second.end()) {
          // collection already exists!
          return TRI_ERROR_ARANGO_DUPLICATE_NAME;
        }
      }
    }

    if (! ac.exists("Plan/Databases/" + databaseName)) {
      return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    }

    if (ac.exists("Plan/Collections/" + databaseName + "/"+collectionID)) {
      return setErrormsg(TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS, errorMsg);
    }

    AgencyCommResult result
      = ac.setValue("Plan/Collections/" + databaseName + "/"+collectionID,
                        json, 0.0);
    if (!result.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                          errorMsg);
    }
  }

  // Now wait for it to appear and be complete:
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  uint64_t index = res._index;

  string where = "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      if (res._values.size() == (size_t) numberOfShards) {
        map<string, AgencyCommResultEntry>::iterator it;
        string tmpMsg = "";
        bool tmpHaveError = false;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          TRI_json_t const* json = (*it).second._json;
          TRI_json_t const* error = TRI_LookupArrayJson(json, "error");
          if (TRI_IsBooleanJson(error) && error->_value._boolean) {
            tmpHaveError = true;
            tmpMsg += " shardID:"+it->first+":";
            TRI_json_t const* errorMessage
                  = TRI_LookupArrayJson(json, "errorMessage");
            if (TRI_IsStringJson(errorMessage)) {
              tmpMsg += string(errorMessage->_value._string.data,
                               errorMessage->_value._string.length-1);
            }
            TRI_json_t const* errorNum = TRI_LookupArrayJson(json, "errorNum");
            if (TRI_IsNumberJson(errorNum)) {
              tmpMsg += " (errNum=";
              tmpMsg += basics::StringUtils::itoa(static_cast<uint32_t>(
                          errorNum->_value._number));
              tmpMsg += ")";
            }
          }
        }
        if (tmpHaveError) {
          errorMsg = "Error in creation of collection:" + tmpMsg;
          return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }

    res.clear();
    res = ac.watchValue("Current/Version", index, interval, false);
    index = res._index;
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropCollectionCoordinator (string const& databaseName,
                                            string const& collectionID,
                                            string& errorMsg,
                                            double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    if (! ac.exists("Plan/Databases/" + databaseName)) {
      return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    }

    res = ac.removeValues("Plan/Collections/"+databaseName+"/"+collectionID,
                          false);
    if (! res.successful()) {
      if (res._statusCode == rest::HttpResponse::NOT_FOUND) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
                         errorMsg);
    }
  }

  flush();

  // Now wait for it to appear and be complete:
  res.clear();
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  uint64_t index = res._index;

  // monitor the entry for the collection
  const string where = "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      // if there are no more active shards for the collection...
      if (res._values.size() == 0) {
        // ...remove the entire directory for the collection
        AgencyCommLocker locker("Current", "WRITE");
        if (locker.successful()) {
          res.clear();
          res = ac.removeValues("Current/Collections/"+databaseName+"/"+
                                collectionID, true);
          if (res.successful()) {
            return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
          }
          return setErrormsg(
            TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT, errorMsg);
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }

    res.clear();
    res = ac.watchValue("Current/Version", index, interval, false);
    index = res._index;
  }
  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionPropertiesCoordinator (string const& databaseName,
                                                     string const& collectionID,
                                                     TRI_col_info_t const* info) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyCommLocker locker("Plan", "WRITE");

  if (! locker.successful()) {
    return TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN;
  }

  if (! ac.exists("Plan/Databases/" + databaseName)) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID, false);

  if (! res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  res.parse("", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = res._values.begin();

  if (it == res._values.end()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_json_t* json = (*it).second._json;
  if (json == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_json_t* copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json);
  if (copy == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "doCompact");
  TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "journalSize");
  TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "waitForSync");

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "doCompact", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_doCompact));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "journalSize", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, info->_maximalSize));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "waitForSync", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, info->_waitForSync));

  res.clear();
  res = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID, copy, 0.0);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, copy);

  if (res.successful()) {
    loadPlannedCollections(false);
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionStatusCoordinator (string const& databaseName,
                                                 string const& collectionID,
                                                 TRI_vocbase_col_status_e status) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyCommLocker locker("Plan", "WRITE");

  if (! locker.successful()) {
    return TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN;
  }

  if (! ac.exists("Plan/Databases/" + databaseName)) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID, false);

  if (! res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  res.parse("", false);
  std::map<std::string, AgencyCommResultEntry>::const_iterator it = res._values.begin();

  if (it == res._values.end()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_json_t* json = (*it).second._json;
  if (json == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_vocbase_col_status_e old = (TRI_vocbase_col_status_e) triagens::basics::JsonHelper::getNumericValue<int>(json, "status", (int) TRI_VOC_COL_STATUS_CORRUPTED);

  if (old == status) {
    // no status change
    return TRI_ERROR_NO_ERROR;
  }

  TRI_json_t* copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json);
  if (copy == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "status");
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, copy, "status", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, status));

  res.clear();
  res = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID, copy, 0.0);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, copy);

  if (res.successful()) {
    loadPlannedCollections(false);
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::ensureIndexCoordinator (string const& databaseName,
                                         string const& collectionID,
                                         TRI_json_t const* json,
                                         bool create,
                                         bool (*compare)(TRI_json_t const*, TRI_json_t const*),
                                         TRI_json_t*& resultJson,
                                         string& errorMsg,
                                         double timeout) {
  AgencyComm ac;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  resultJson = 0;
  TRI_json_t* newIndex = 0;
  int numberOfShards = 0;

  // check index id
  uint64_t iid = 0;

  TRI_json_t const* idxJson = TRI_LookupArrayJson(json, "id");
  if (TRI_IsStringJson(idxJson)) {
    // use predefined index id
    iid = triagens::basics::StringUtils::uint64(idxJson->_value._string.data);
  }

  if (iid == 0) {
    // no id set, create a new one!
    iid = uniqid();
  }

  string const idString = triagens::basics::StringUtils::itoa(iid);

  {
    TRI_json_t* collectionJson = 0;
    AgencyCommLocker locker("Plan", "WRITE");

    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    {
      loadPlannedCollections(false);

      READ_LOCKER(_lock);

      shared_ptr<CollectionInfo> c = getCollection(databaseName, collectionID);

      if (c->empty()) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }

      TRI_json_t const* indexes = c->getIndexes();
      numberOfShards = c->numberOfShards();

      if (TRI_IsListJson(indexes)) {
        bool hasSameIndexType = false;
        TRI_json_t const* type = TRI_LookupArrayJson(json, "type");

        if (! TRI_IsStringJson(type)) {
          return setErrormsg(TRI_ERROR_INTERNAL, errorMsg);
        }

        for (size_t i = 0; i < indexes->_value._objects._length; ++i) {
          TRI_json_t const* other = TRI_LookupListJson(indexes, i);

          if (! TRI_CheckSameValueJson(TRI_LookupArrayJson(json, "type"),
                                       TRI_LookupArrayJson(other, "type"))) {
            // compare index types first. they must match
            continue;
          }

          hasSameIndexType = true;

          bool isSame = compare(json, other);

          if (isSame) {
            // found an existing index...
            resultJson = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other);
            TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, resultJson, "isNewlyCreated", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, false));
            return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
          }
        }

        if (TRI_EqualString(type->_value._string.data, "cap")) {
          // special handling for cap constraints
          if (hasSameIndexType) {
            // there can only be one cap constraint
            return setErrormsg(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED, errorMsg);
          }

          if (numberOfShards > 1) {
            // there must be at most one shard if there should be a cap constraint
            return setErrormsg(TRI_ERROR_CLUSTER_UNSUPPORTED, errorMsg);
          }
        }
      }

      // no existing index found.
      if (! create) {
        TRI_ASSERT(resultJson == 0);
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }


      // now create a new index
      collectionJson = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, c->getJson());
    }

    if (collectionJson == 0) {
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    TRI_json_t* idx = TRI_LookupArrayJson(collectionJson, "indexes");

    if (idx == 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    newIndex = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json);

    if (newIndex == 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    // add index id
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                         newIndex,
                         "id",
                         TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, idString.c_str()));

    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, idx, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, newIndex));

    AgencyCommResult result = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID,
                                          collectionJson,
                                          0.0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);

    if (! result.successful()) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newIndex);
      // TODO
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN, errorMsg);
    }
  }

  // wipe cache
  flush();

  TRI_ASSERT(numberOfShards > 0);

  // now wait for the index to appear
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (! res.successful()) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newIndex);
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION, errorMsg);
  }
  uint64_t index = res._index;

  string where = "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == (size_t) numberOfShards) {
        map<string, AgencyCommResultEntry>::iterator it;

        size_t found = 0;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          TRI_json_t const* json = (*it).second._json;
          TRI_json_t const* indexes = TRI_LookupArrayJson(json, "indexes");
          if (! TRI_IsListJson(indexes)) {
            // no list, so our index is not present. we can abort searching
            break;
          }

          for (size_t i = 0; i < indexes->_value._objects._length; ++i) {
            TRI_json_t const* v = TRI_LookupListJson(indexes, i);

            // check for errors
            if (hasError(v)) {
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newIndex);
              string errorMsg = extractErrorMessage((*it).first, v);

              errorMsg = "Error during index creation: " + errorMsg;

              v = TRI_LookupArrayJson(v, "errorNum");
              if (TRI_IsNumberJson(v)) {
                // found a specific error number
                return (int) v->_value._number;
              }

              // return generic error number
              return TRI_ERROR_ARANGO_INDEX_CREATION_FAILED;
            }

            TRI_json_t const* k = TRI_LookupArrayJson(v, "id");

            if (! TRI_IsStringJson(k) || idString != string(k->_value._string.data)) {
              // this is not our index
              continue;
            }

            // found our index
            found++;
            break;
          }
        }

        if (found == (size_t) numberOfShards) {
          resultJson = newIndex;
          TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, resultJson, "isNewlyCreated", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));

          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
      }
    }
    res.clear();
    res = ac.watchValue("Current/Version", index, interval, false);
    index = res._index;
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, newIndex);

  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropIndexCoordinator (string const& databaseName,
                                       string const& collectionID,
                                       TRI_idx_iid_t iid,
                                       string& errorMsg,
                                       double timeout) {
  AgencyComm ac;

  const double realTimeout = getTimeout(timeout);
  const double endTime = TRI_microtime() + realTimeout;
  const double interval = getPollInterval();

  int numberOfShards = 0;
  string const idString = triagens::basics::StringUtils::itoa(iid);

  {
    AgencyCommLocker locker("Plan", "WRITE");

    if (! locker.successful()) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, errorMsg);
    }

    TRI_json_t* collectionJson = 0;
    TRI_json_t const* indexes = 0;

    {
      loadPlannedCollections(false);

      READ_LOCKER(_lock);

      shared_ptr<CollectionInfo> c = getCollection(databaseName, collectionID);

      if (c->empty()) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
      }

      indexes = c->getIndexes();

      if (! TRI_IsListJson(indexes)) {
        // no indexes present, so we can't delete our index
        return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
      }

      collectionJson = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, c->getJson());
      numberOfShards = c->numberOfShards();
    }


    if (collectionJson == 0) {
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    TRI_ASSERT(TRI_IsListJson(indexes));

    // delete previous indexes entry
    TRI_DeleteArrayJson(TRI_UNKNOWN_MEM_ZONE, collectionJson, "indexes");

    TRI_json_t* copy = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

    if (copy == 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
    }

    bool found = false;

    // copy remaining indexes back into collection
    for (size_t i = 0; i < indexes->_value._objects._length; ++i) {
      TRI_json_t const* v = TRI_LookupListJson(indexes, i);
      TRI_json_t const* id = TRI_LookupArrayJson(v, "id");
      TRI_json_t const* type = TRI_LookupArrayJson(v, "type");

      if (! TRI_IsStringJson(id) || ! TRI_IsStringJson(type)) {
        continue;
      }

      if (idString == string(id->_value._string.data)) {
        // found our index, ignore it when copying
        found = true;

        string const typeString(type->_value._string.data);
        if (typeString == "primary" || typeString == "edge") {
          // must not delete these index types
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, copy);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
          return setErrormsg(TRI_ERROR_FORBIDDEN, errorMsg);
        }

        continue;
      }

      TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, copy, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, v));
    }

    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, collectionJson, "indexes", copy);

    if (! found) {
      // did not find the sought index
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);
      return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
    }

    AgencyCommResult result = ac.setValue("Plan/Collections/" + databaseName + "/" + collectionID,
                                          collectionJson,
                                          0.0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, collectionJson);

    if (! result.successful()) {
      // TODO
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN, errorMsg);
    }
  }

  // wipe cache
  flush();

  TRI_ASSERT(numberOfShards > 0);

  // now wait for the index to disappear
  AgencyCommResult res = ac.getValues("Current/Version", false);
  if (! res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION, errorMsg);
  }
  uint64_t index = res._index;

  string where = "Current/Collections/" + databaseName + "/" + collectionID;
  while (TRI_microtime() <= endTime) {
    res.clear();
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where + "/", false)) {
      if (res._values.size() == (size_t) numberOfShards) {
        map<string, AgencyCommResultEntry>::iterator it;

        bool found = false;
        for (it = res._values.begin(); it != res._values.end(); ++it) {
          TRI_json_t const* json = (*it).second._json;
          TRI_json_t const* indexes = TRI_LookupArrayJson(json, "indexes");

          if (TRI_IsListJson(indexes)) {
            for (size_t i = 0; i < indexes->_value._objects._length; ++i) {
              TRI_json_t const* v = TRI_LookupListJson(indexes, i);
              if (TRI_IsArrayJson(v)) {
                TRI_json_t const* k = TRI_LookupArrayJson(v, "id");
                if (TRI_IsStringJson(k) && idString == string(k->_value._string.data)) {
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

        if (! found) {
          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
      }
    }

    res.clear();
    res = ac.watchValue("Current/Version", index, interval, false);
    index = res._index;
  }

  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadServers () {
  static const std::string prefix = "Current/ServersRegistered";

  AgencyCommResult result;

  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    _servers.clear();

    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

    while (it != result._values.end()) {
      TRI_json_t const* sub
        = triagens::basics::JsonHelper::getArrayElement((*it).second._json,
                                                        "endpoint");
      if (nullptr != sub) {
        std::string server = triagens::basics::JsonHelper::getStringValue(sub, "");

        _servers.emplace(std::make_pair((*it).first, server));
      }
      ++it;
    }

    _serversValid = true;

    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());

  _serversValid = false;

  return;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint (ServerID const& serverID) {
  int tries = 0;

  if (! _serversValid) {
    loadServers();
    tries++;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      std::map<ServerID, string>::const_iterator it = _servers.find(serverID);

      if (it != _servers.end()) {
        return (*it).second;
      }
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadCurrentDBServers () {
  static const std::string prefix = "Current/DBServers";

  AgencyCommResult result;

  {
    AgencyCommLocker locker("Current", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix, true);
    }
  }

  if (result.successful()) {
    result.parse(prefix + "/", false);

    WRITE_LOCKER(_lock);
    _DBServers.clear();

    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.begin();

    for (; it != result._values.end(); ++it) {
      _DBServers.emplace(std::make_pair((*it).first, triagens::basics::JsonHelper::getStringValue((*it).second._json, "")));
    }

    _DBServersValid = true;
    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());

  _DBServersValid = false;

  return;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers () {
  std::vector<ServerID> result;

  int tries = 0;
  while (++tries <= 2) {
    {
      // return a consistent state of servers
      READ_LOCKER(_lock);

      if (_DBServersValid) {
        result.reserve(_DBServers.size());

        for (auto& it : _DBServers) {
          result.emplace_back(it.first);
        }

        return result;
      }
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

std::string ClusterInfo::getTargetServerEndpoint (ServerID const& serverID) {
  static const std::string prefix = "Target/MapIDToEndpoint/";

  AgencyCommResult result;

  // fetch value at Target/MapIDToEndpoint
  {
    AgencyCommLocker locker("Target", "READ");

    if (locker.successful()) {
      result = _agency.getValues(prefix + serverID, false);
    }
  }

  if (result.successful()) {
    result.parse(prefix, false);

    // check if we can find ourselves in the list returned by the agency
    std::map<std::string, AgencyCommResultEntry>::const_iterator it = result._values.find(serverID);

    if (it != result._values.end()) {
      return triagens::basics::JsonHelper::getStringValue((*it).second._json, "");
    }
  }

  // not found
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the server who is responsible for a shard
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

ServerID ClusterInfo::getResponsibleServer (ShardID const& shardID) {
  int tries = 0;

  if (! _collectionsCurrentValid) {
    loadCurrentCollections(true);
    tries++;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      std::map<ShardID, ServerID>::const_iterator it = _shardIds.find(shardID);

      if (it != _shardIds.end()) {
        return (*it).second;
      }
    }

    // must load collections outside the lock
    loadCurrentCollections(true);
  }

  return ServerID("");
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

int ClusterInfo::getResponsibleShard (CollectionID const& collectionID,
                                      TRI_json_t const* json,
                                      bool docComplete,
                                      ShardID& shardID,
                                      bool& usesDefaultShardingAttributes) {
  // Note that currently we take the number of shards and the shardKeys
  // from Plan, since they are immutable. Later we will have to switch
  // this to Current, when we allow to add and remove shards.
  if (! _collectionsValid) {
    loadPlannedCollections();
  }

  int tries = 0;
  shared_ptr<vector<string> > shardKeysPtr;
  char const** shardKeys = nullptr;
  shared_ptr<vector<ShardID> > shards;
  bool found = false;

  while (++tries <= 2) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(_lock);
      map<CollectionID, shared_ptr<vector<string>>>::iterator it
          = _shards.find(collectionID);

      if (it != _shards.end()) {
        shards = it->second;
        map<CollectionID, shared_ptr<vector<string>>>::iterator it2
            = _shardKeys.find(collectionID);
        if (it2 != _shardKeys.end()) {
          shardKeysPtr = it2->second;
          shardKeys = new char const* [shardKeysPtr->size()];
          if (shardKeys != nullptr) {
            size_t i;
            for (i = 0; i < shardKeysPtr->size(); ++i) {
              shardKeys[i] = shardKeysPtr->at(i).c_str();
            }
            usesDefaultShardingAttributes = shardKeysPtr->size() == 1 &&
                                            shardKeysPtr->at(0) == "_key";
            found = true;
            break;  // all OK
          }
        }
      }
    }
    loadPlannedCollections();
  }
  if (! found) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int error;
  uint64_t hash = TRI_HashJsonByAttributes(json, shardKeys,
                                           (int) shardKeysPtr->size(),
                                           docComplete, &error);
  static char const* magicPhrase
      = "Foxx you have stolen the goose, give she back again!";
  static size_t const len = 52;
  // To improve our hash function:
  hash = TRI_FnvHashBlock(hash, magicPhrase, len);

  delete[] shardKeys;

  shardID = shards->at(hash % shards->size());
  return error;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
