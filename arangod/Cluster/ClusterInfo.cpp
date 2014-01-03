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
    _shardKeys(),
    _shards() {
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
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfo::~CollectionInfo () {
} 

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates a collection info object
////////////////////////////////////////////////////////////////////////////////

void CollectionInfo::invalidate () {
  _id   = 0;
  _name = "";
  _type = TRI_COL_TYPE_UNKNOWN;
  _shardKeys.clear();
  _shards.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populate object properties from the JSON given
////////////////////////////////////////////////////////////////////////////////

bool CollectionInfo::createFromJson (TRI_json_t const* json) {
  // id
  std::string id = JsonHelper::getStringValue(json, "id", "");
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
  _type = (TRI_col_type_e) JsonHelper::getNumericValue<int>(json, "type", (int) TRI_COL_TYPE_UNKNOWN);
  if (_type == TRI_COL_TYPE_UNKNOWN) {
    return false;
  }

  // TODO: status
  // TODO: indexes

  // shardKeys
  TRI_json_t const* value = TRI_LookupArrayJson(json, "shardKeys");
  if (! JsonHelper::isList(value)) {
    return false;
  }
  _shardKeys = JsonHelper::stringList(value);
  
  // shards
  value = TRI_LookupArrayJson(json, "shards");
  if (! JsonHelper::isList(value)) {
    return false;
  }
  _shards = JsonHelper::stringList(value);

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
  TRI_Insert3ArrayJson(zone, json, "type", TRI_CreateNumberJson(zone, (double) _type));

  TRI_json_t* values = JsonHelper::stringList(zone, _shardKeys);

  if (values == 0) {
    TRI_FreeJson(zone, json);
    return 0;
  }

  TRI_Insert3ArrayJson(zone, json, "shardKeys", values);
  
  values = JsonHelper::stringList(zone, _shards);

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
  : _agency() {

  loadServers();
  loadShards();
  loadCollections();
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
/// @brief (re-)load the information about collections from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadCollections () {
  while (true) {
    AgencyCommResult result;

    {
      AgencyCommLocker locker("Current", "READ");
      result = _agency.getValues("Current/Collections", true);
    }

    if (result.successful()) {
      std::map<std::string, std::string> collections;

      if (result.flattenJson(collections, "Current/Collections/", false)) {
        LOG_TRACE("Current/Collections loaded successfully");
      
        WRITE_LOCKER(_lock);
        _collections.clear(); 

        std::map<std::string, std::string>::const_iterator it;
        for (it = collections.begin(); it != collections.end(); ++it) {
          const std::string key = (*it).first;

          // each entry consists of a database id and a collection id, separated by '/'
          std::vector<std::string> parts = triagens::basics::StringUtils::split(key, '/'); 
         
          if (parts.size() != 2) {
            // invalid entry
            LOG_WARNING("found invalid collection key in agency: '%s'", key.c_str());
            continue;
          }
          
          const std::string& database   = parts[0]; 
          const std::string& collection = parts[1]; 

          if (collection == "Lock") {
            continue;
          }

          // check whether we have created an entry for the database already
          AllCollections::iterator it2 = _collections.find(database);

          if (it2 == _collections.end()) {
            // not yet, so create an entry for the database
            DatabaseCollections empty;
            CollectionInfo collectionData((*it).second);
            empty.insert(std::make_pair<CollectionID, CollectionInfo>(collection, collectionData));

            _collections.insert(std::make_pair<DatabaseID, DatabaseCollections>(database, empty));
          }
          else {
            // insert the collection into the existing map
            CollectionInfo collectionData((*it).second);
            (*it2).second.insert(std::make_pair<CollectionID, CollectionInfo>(collection, collectionData));
          }
        }

        return;
      }
    }

    LOG_TRACE("Error while loading Current/Collections");

    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
////////////////////////////////////////////////////////////////////////////////

CollectionInfo ClusterInfo::getCollectionInfo (DatabaseID const& databaseID,
                                               CollectionID const& collectionID) {
  int tries = 0;

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
    
    // must call loadCollections outside the lock
    loadCollections();
  }

  return CollectionInfo();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadServers () {
  while (true) {
    AgencyCommResult result;

    {
      AgencyCommLocker locker("Current", "READ");
      result = _agency.getValues("Current/ServersRegistered", true);
    }

    if (result.successful()) {
      std::map<std::string, std::string> servers;

      if (result.flattenJson(servers, "Current/ServersRegistered/", false)) {
        LOG_TRACE("Current/ServersRegistered loaded successfully");
      
        WRITE_LOCKER(_lock); 
        _servers.clear();

        std::map<std::string, std::string>::const_iterator it;
        for (it = servers.begin(); it != servers.end(); ++it) {
          _shards.insert(std::make_pair<ServerID, std::string>((*it).first, (*it).second));
        }

        return;
      }
    }

    LOG_TRACE("Error while loading Current/ServersRegistered");

    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint (ServerID const& serverID) {
  int tries = 0;

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
/// @brief (re-)load the information about shards from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::loadShards () {
  while (true) {
    AgencyCommResult result;

    {
      AgencyCommLocker locker("Current", "READ");
      result = _agency.getValues("Current/ShardLocation", true);
    }

    if (result.successful()) {
      std::map<std::string, std::string> shards;

      if (result.flattenJson(shards, "Current/ShardLocation/", false)) {
        LOG_TRACE("Current/ShardLocation loaded successfully");

        // now update our internals with the results
        WRITE_LOCKER(_lock);
        _shards.clear();

        std::map<ShardID, ServerID>::const_iterator it;
        for (it = shards.begin(); it != shards.end(); ++it) {
          _shards.insert(std::make_pair<ShardID, ServerID>((*it).first, (*it).second));
        }
        
        return;
      }
    }
    
    LOG_TRACE("Error while loading Current/ServersRegistered");

    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the server who is responsible for a shard
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

ServerID ClusterInfo::getResponsibleServer (ShardID const& shardID) {
  int tries = 0;

  while (++tries <= 2) {
    {
      READ_LOCKER(_lock);
      std::map<ShardID, ServerID>::const_iterator it = _shards.find(shardID);

      if (it != _shards.end()) {
        return (*it).second;
      }
    }

    // must call loadShards outside the lock
    loadShards();
  }

  return ServerID("");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
