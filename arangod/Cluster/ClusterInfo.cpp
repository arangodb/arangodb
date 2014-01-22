////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterInfo.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterInfo.h"

#include "BasicsC/conversions.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "Basics/JsonHelper.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/StringUtils.h"

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
  : _json(0) {
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

  if (other._json != 0) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo& CollectionInfo::operator= (CollectionInfo const& other) {
  if (other._json != 0 && this != &other) {
    _json = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._json);
  }
  else {
    _json = 0;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::~CollectionInfo () {
  if (_json != 0) {
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
  _jsons.insert(make_pair<ShardID, TRI_json_t*>(shardID, json));
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
    if (it->second != 0) {
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
    if (0 != it->second) {
      it->second = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

ClusterInfo* ClusterInfo::_theinstance = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an instance of the cluster info class
////////////////////////////////////////////////////////////////////////////////

ClusterInfo* ClusterInfo::instance () {
  // This does not have to be thread-safe, because we guarantee that
  // this is called very early in the startup phase when there is still
  // a single thread.
  if (0 == _theinstance) {
    _theinstance = new ClusterInfo();  // this now happens exactly once
  }
  return _theinstance;
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

    if (json != 0) {
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

      if (json != 0) {
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
      const std::string& name = (*it).first;
      TRI_json_t* options = (*it).second._json;

      // steal the json
      (*it).second._json = 0;
      _plannedDatabases.insert(std::make_pair<DatabaseID, TRI_json_t*>(name, options));

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
        it2 = _currentDatabases.insert(std::make_pair<DatabaseID, std::map<ServerID, TRI_json_t*> >(database, empty)).first; 
      }
       
      if (parts.size() == 2) {
        // got a server name
        TRI_json_t* json = (*it).second._json;
        // steal the JSON
        (*it).second._json = 0;
        (*it2).second.insert(std::make_pair<ServerID, TRI_json_t*>(parts[1], json));
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
        _collections.insert(std::make_pair<DatabaseID, DatabaseCollections>(database, empty));
        it2 = _collections.find(database);
      }

      TRI_json_t* json = (*it).second._json;
      // steal the json
      (*it).second._json = 0;

      const CollectionInfo collectionData(json);
        
      // insert the collection into the existing map
        
      (*it2).second.insert(std::make_pair<CollectionID, CollectionInfo>(collection, collectionData));
      (*it2).second.insert(std::make_pair<CollectionID, CollectionInfo>(collectionData.name(), collectionData));

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

CollectionInfo ClusterInfo::getCollection (DatabaseID const& databaseID,
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
        // look up collection by id
        DatabaseCollections::const_iterator it2 = (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }
    
    // must load collections outside the lock
    loadPlannedCollections(true);
  }

  return CollectionInfo();
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
  CollectionInfo ci = getCollection(databaseID, collectionID);
  
  return getCollectionProperties(ci);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

const std::vector<CollectionInfo> ClusterInfo::getCollections (DatabaseID const& databaseID) {
  std::vector<CollectionInfo> result;

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
        _collectionsCurrent.insert(std::make_pair<DatabaseID, DatabaseCollectionsCurrent>(database, empty));
        it2 = _collectionsCurrent.find(database);
      }

      TRI_json_t* json = (*it).second._json;
      // steal the json
      (*it).second._json = 0;

      // check whether we already have a CollectionInfoCurrent:
      DatabaseCollectionsCurrent::iterator it3;
      it3 = it2->second.find(collection);
      if (it3 == it2->second.end()) {
        const CollectionInfoCurrent collectionDataCurrent(shardID, json);
        it2->second.insert(make_pair<CollectionID, CollectionInfoCurrent>
                              (collection, collectionDataCurrent));
        it3 = it2->second.find(collection);
      }
      else {
        it3->second.add(shardID, json);
      }
        
      // Note that we have only inserted the CollectionInfoCurrent under
      // the collection ID and not under the name! It is not possible
      // to query the current collection info by name. This is because
      // the correct place to hold the current name is in the plan.
      // Thus: Look there and get the collection ID from there. Then
      // ask about the current collection info.

      // Now take note of this shard and its responsible server:
      std::string DBserver = triagens::basics::JsonHelper::getStringValue
                    (json, "DBserver", "");
      if (DBserver != "") {
        _shardIds.insert(make_pair<ShardID, ServerID>(shardID, DBserver));
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

CollectionInfoCurrent ClusterInfo::getCollectionCurrent 
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

  return CollectionInfoCurrent();
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
    if (!res.successful()) {
      if (res._statusCode == triagens::rest::HttpResponse::PRECONDITION_FAILED) {
        return setErrormsg(TRI_ERROR_CLUSTER_DATABASE_NAME_EXISTS, errorMsg);
      }

      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN,
                         errorMsg);
    }
  }

  // Now wait for it to appear and be complete:
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  uint64_t index = res._index;

  vector<ServerID> DBServers = getCurrentDBServers();
  int count = 0;  // this counts, when we have to reload the DBServers

  string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
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
    
    res = ac.removeValues("Plan/Collections/" + name, true);

    if (! res.successful() && res.httpCode() != (int) rest::HttpResponse::NOT_FOUND) {
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,
                         errorMsg);
    }

  }

  // Now wait for it to appear and be complete:
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                        errorMsg);
  }
  uint64_t index = res._index;

  string where = "Current/Databases/" + name;
  while (TRI_microtime() <= endTime) {
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      if (res._values.size() == 0) {
        AgencyCommLocker locker("Current", "WRITE");
        if (locker.successful()) {
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
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      if (res._values.size() == numberOfShards) {
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
    if (!res.successful()) {
      if (res._statusCode == rest::HttpResponse::NOT_FOUND) {
        return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg); 
      }
      return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,
                         errorMsg);
    }
  }

  // Now wait for it to appear and be complete:
  res = ac.getValues("Current/Version", false);
  if (!res.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION,
                       errorMsg);
  }
  uint64_t index = res._index;

  const string where = "Current/Collections/" + databaseName + "/" + collectionID; 
  while (TRI_microtime() <= endTime) {
    res = ac.getValues(where, true);
    if (res.successful() && res.parse(where+"/", false)) {
      if (res._values.size() == 0) {
        AgencyCommLocker locker("Current", "WRITE");
        if (locker.successful()) {
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
      const std::string server = triagens::basics::JsonHelper::getStringValue((*it).second._json, "");

      _servers.insert(std::make_pair<ServerID, std::string>((*it).first, server));
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
      _DBServers.insert(std::make_pair<ServerID, ServerID>((*it).first, triagens::basics::JsonHelper::getStringValue((*it).second._json, "")));
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
  if (! _DBServersValid) {
    loadCurrentDBServers();
  }

  std::vector<ServerID> result;

  READ_LOCKER(_lock);
  std::map<ServerID, ServerID>::iterator it = _DBServers.begin();

  while (it != _DBServers.end()) {
    result.push_back((*it).first);
    it++;
  }

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

  if (! _collectionsValid) {
    loadPlannedCollections(true);
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
