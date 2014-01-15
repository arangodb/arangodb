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

using namespace triagens::arango;
using triagens::basics::JsonHelper;

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
  : _id(),
    _name(),
    _type(TRI_COL_TYPE_UNKNOWN),
    _status(TRI_VOC_COL_STATUS_CORRUPTED),
    _maximalSize(0),
    _deleted(false),
    _doCompact(false),
    _isSystem(false),
    _isVolatile(false),
    _waitForSync(false),
    _keyOptions(0),
    _shardKeys(),
    _shardIds() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo (std::string const& data) {
  TRI_json_t* json = JsonHelper::fromString(data);

  if (json != 0) {
    if (JsonHelper::isArray(json)) {
      if (! createFromJson(json)) {
        invalidate();
      }
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo (TRI_json_t* json) {
  if (json != 0) {
    if (JsonHelper::isArray(json)) {
      if (! createFromJson(json)) {
        invalidate();
      }
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::CollectionInfo (CollectionInfo const& other) :
  _id(other._id),
  _name(other._name),
  _type(other._type),
  _status(other._status),
  _maximalSize(other._maximalSize),
  _deleted(other._deleted),
  _doCompact(other._doCompact),
  _isSystem(other._isSystem),
  _isVolatile(other._isVolatile),
  _waitForSync(other._waitForSync),
  _keyOptions(0),
  _shardKeys(other._shardKeys),
  _shardIds(other._shardIds) {

  if (other._keyOptions != 0) {
    _keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._keyOptions);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfo& CollectionInfo::operator= (CollectionInfo const& other) {
  _id = other._id;
  _name = other._name;
  _type = other._type;
  _status = other._status;
  _maximalSize = other._maximalSize;
  _deleted = other._deleted;
  _doCompact = other._doCompact;
  _isSystem = other._isSystem;
  _isVolatile = other._isVolatile;
  _waitForSync = other._waitForSync;
  _shardKeys = other._shardKeys;
  _shardIds = other._shardIds;
  _keyOptions = 0;
  
  if (other._keyOptions != 0) {
    _keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, other._keyOptions);
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::~CollectionInfo () {
  if (_keyOptions != 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _keyOptions);
  }
} 

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates a collection info object
////////////////////////////////////////////////////////////////////////////////

void CollectionInfo::invalidate () {
  _id = 0;
  _name = "";
  _type = TRI_COL_TYPE_UNKNOWN;
  _status = TRI_VOC_COL_STATUS_CORRUPTED;
  _maximalSize = 0;
  _deleted = false;
  _doCompact = false;
  _isSystem = false;
  _isVolatile = false;
  _waitForSync = false;
  _shardKeys.clear();
  _shardIds.clear();
  
  if (_keyOptions != 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _keyOptions);
  }
  _keyOptions = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate object properties from the JSON given
////////////////////////////////////////////////////////////////////////////////

bool CollectionInfo::createFromJson (TRI_json_t const* json) {
  // id
  const std::string id = JsonHelper::getStringValue(json, "id", "");
  if (id.empty()) {
    return false;
  }
  _id = triagens::basics::StringUtils::uint64(id.c_str(), id.size());


  // name
  _name = JsonHelper::getStringValue(json, "name", "");
  if (_name.empty()) {
    return false;
  }

  // type
  _type = (TRI_col_type_e) JsonHelper::getNumericValue<int>(json, 
                                                            "type", 
                                                            (int) TRI_COL_TYPE_UNKNOWN);
  if (_type == TRI_COL_TYPE_UNKNOWN) {
    return false;
  }

  // status
  _status = (TRI_vocbase_col_status_e) JsonHelper::getNumericValue<int>(json, 
                                                                        "status", 
                                                                        (int) TRI_VOC_COL_STATUS_NEW_BORN);
  if (_status == TRI_VOC_COL_STATUS_CORRUPTED || 
      _status == TRI_VOC_COL_STATUS_NEW_BORN) {
    return false;
  }

  _maximalSize = JsonHelper::getNumericValue<TRI_voc_size_t>(json, "maximalSize", 0);
  _doCompact   = JsonHelper::getBooleanValue(json, "doCompact", false);
  _isSystem    = JsonHelper::getBooleanValue(json, "isSystem", false);
  _isVolatile  = JsonHelper::getBooleanValue(json, "isVolatile", false);
  _waitForSync = JsonHelper::getBooleanValue(json, "waitForSync", false);

  TRI_json_t const* keyOptions = JsonHelper::getArrayElement(json, "keyOptions");
  if (JsonHelper::isArray(keyOptions)) {
    _keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
  }
  else {
    _keyOptions = 0;
  }
  
  // TODO: indexes

  // shardKeys
  TRI_json_t const* value = TRI_LookupArrayJson(json, "shardKeys");
  if (! JsonHelper::isList(value)) {
    return false;
  }
  _shardKeys = JsonHelper::stringList(value);
 
  // shards
  value = TRI_LookupArrayJson(json, "shards");
  if (! JsonHelper::isArray(value)) {
    return false;
  }
  _shardIds = JsonHelper::stringObject(value);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON string from the object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* CollectionInfo::toJson (TRI_memory_zone_t* zone) {
  TRI_json_t* json = TRI_CreateArrayJson(zone);

  if (json == 0) {
    return json;
  }

  char* data = TRI_StringUInt64(_id);

  if (data == 0) {
    TRI_FreeJson(zone, json);
    return 0;
  }

  TRI_Insert3ArrayJson(zone, json, "id", TRI_CreateStringJson(zone, data));
  TRI_Insert3ArrayJson(zone, json, "name", TRI_CreateStringCopyJson(zone, _name.c_str()));
  TRI_Insert3ArrayJson(zone, json, "type", TRI_CreateNumberJson(zone, (int) _type));
  TRI_Insert3ArrayJson(zone, json, "status", TRI_CreateNumberJson(zone, (int) _status));
  
  TRI_Insert3ArrayJson(zone, json, "maximalSize", TRI_CreateNumberJson(zone, (int) _maximalSize));
  TRI_Insert3ArrayJson(zone, json, "doCompact", TRI_CreateBooleanJson(zone, _doCompact));
  TRI_Insert3ArrayJson(zone, json, "isSystem", TRI_CreateBooleanJson(zone, _isSystem));
  TRI_Insert3ArrayJson(zone, json, "isVolatile", TRI_CreateBooleanJson(zone, _isVolatile));
  TRI_Insert3ArrayJson(zone, json, "waitForSync", TRI_CreateBooleanJson(zone, _waitForSync));

  if (_keyOptions != 0) {
    TRI_Insert3ArrayJson(zone, json, "keyOptions", TRI_CopyJson(zone, _keyOptions));
  }

  // TODO: indexes

  TRI_json_t* values = JsonHelper::stringList(zone, _shardKeys);

  if (values == 0) {
    TRI_FreeJson(zone, json);
    return 0;
  }

  TRI_Insert3ArrayJson(zone, json, "shardKeys", values);
  
  values = JsonHelper::stringObject(zone, _shardIds);

  if (values == 0) {
    TRI_FreeJson(zone, json);
    return 0;
  }

  TRI_Insert3ArrayJson(zone, json, "shards", values);

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 ClusterInfo class
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

    _uniqid._currentValue = result._index;
    _uniqid._upperValue   = _uniqid._currentValue + fetch - 1;

    return _uniqid._currentValue++;
  }

  return ++_uniqid._currentValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::flush () {
  WRITE_LOCKER(_lock);

  _collectionsValid = false;
  _serversValid = false;
  _DBServersValid = false;

  _collections.clear();
  _servers.clear();
  _shardIds.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist (DatabaseID const& databaseID) {
  int tries = 0;

  if (! _collectionsValid) {
    loadCurrentCollections();
    ++tries;
  }

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      // look up database by id
      AllCollections::const_iterator it = _collections.find(databaseID);

      if (it != _collections.end()) {
        return true;
      }
    }
    
    // must load collections outside the lock
    loadCurrentCollections();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

vector<DatabaseID> ClusterInfo::listDatabases () {
  vector<DatabaseID> res;

  if (! _collectionsValid) {
    loadCurrentCollections();
  }

  AllCollections::const_iterator it;
  for (it = _collections.begin(); it != _collections.end(); ++it) {
    res.push_back(it->first);
  }
  return res;
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
    _plannedDatabases.clear(); 

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    while (it != result._values.end()) {
      const std::string& name = (*it).first;
      TRI_json_t* options = (*it).second._json;

      // steal the json
      (*it).second._json = 0;
      _plannedDatabases.insert(std::make_pair<DatabaseID, TRI_json_t*>(name, options));
    }

    return;
  }

  LOG_TRACE("Error while loading %s", prefix.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadCurrentCollections () {
  static const std::string prefix = "Current/Collections";

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
    _collections.clear(); 
    _shardIds.clear();

    std::map<std::string, AgencyCommResultEntry>::iterator it = result._values.begin();

    for (; it != result._values.end(); ++it) {
      const std::string& key = (*it).first;

      // each entry consists of a database id and a collection id, separated by '/'
      std::vector<std::string> parts = triagens::basics::StringUtils::split(key, '/'); 
       
      if (parts.size() != 2) {
        // invalid entry
        LOG_WARNING("found invalid collection key in agency: '%s'", key.c_str());
        continue;
      }
        
      const std::string& database   = parts[0]; 
      const std::string& collection = parts[1]; 

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

      std::map<std::string, std::string> shards = collectionData.shardIds();
      std::map<std::string, std::string>::const_iterator it3 = shards.begin();
        
      while (it3 != shards.end()) {
        const std::string shardId = (*it3).first;
        const std::string serverId = (*it3).second;

        _shardIds.insert(std::make_pair<ShardID, ServerID>(shardId, serverId));
        ++it3;
      }

      _collectionsValid = true;
      return;
    }
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
    loadCurrentCollections();
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
    loadCurrentCollections();
  }

  return CollectionInfo();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get properties of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_col_info_t ClusterInfo::getCollectionProperties (CollectionInfo const& collection) {
  TRI_col_info_t info;

  info._type        = collection._type;
  info._cid         = collection._id;
  info._revision    = 0; // TODO 
  info._maximalSize = collection._maximalSize;
  memcpy(info._name, collection._name.c_str(), collection._name.size());
  info._deleted     = collection._deleted;
  info._doCompact   = collection._doCompact;
  info._isSystem    = collection._isSystem;
  info._isVolatile  = collection._isVolatile;
  info._waitForSync = collection._waitForSync;

  if (collection._keyOptions != 0) {
    info._keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, collection._keyOptions);
  }
  else {
    info._keyOptions = 0;
  }

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
  loadCurrentCollections();

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

  std::vector<ServerID> res;
  std::map<ServerID, ServerID>::iterator i;
  for (i = _DBServers.begin(); i != _DBServers.end(); ++i) {
    res.push_back(i->first);
  }
  return res;
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
    loadCurrentCollections();
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
    loadCurrentCollections();
  }

  return ServerID("");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
