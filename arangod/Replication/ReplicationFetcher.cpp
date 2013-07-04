////////////////////////////////////////////////////////////////////////////////
/// @brief replication data fetcher
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationFetcher.h"

#include "BasicsC/json.h"
#include "Basics/JsonHelper.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/collection.h"
#include "VocBase/replication.h"
#include "VocBase/server-id.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace triagens::httpclient;
  
#define ENSURE_JSON(what, check, msg)                \
  if (what == 0 || ! JsonHelper::check(what)) {      \
    errorMsg = msg;                                  \
                                                     \
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;   \
  }                                                         

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ReplicationFetcher::ReplicationFetcher (TRI_vocbase_t* vocbase,
                                        const string& masterEndpoint,
                                        double timeout) :
  _vocbase(vocbase),
  _masterInfo(),
  _applyState(),
  _endpoint(Endpoint::clientFactory(masterEndpoint)),
  _connection(0),
  _client(0) {
 
  TRI_InitMasterInfoReplication(&_masterInfo, masterEndpoint.c_str());
  TRI_InitApplyStateReplication(&_applyState);

  if (_endpoint != 0) { 
    _connection = GeneralClientConnection::factory(_endpoint, timeout, timeout, 3);

    if (_connection != 0) {
      _client = new SimpleHttpClient(_connection, timeout, false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ReplicationFetcher::~ReplicationFetcher () {
  // shutdown everything properly
  if (_client != 0) {
    delete _client;
  }

  if (_connection != 0) {
    delete _connection;
  }

  if (_endpoint != 0) {
    delete _endpoint;
  }

  TRI_DestroyMasterInfoReplication(&_masterInfo);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief run method
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::run () {
  int res;
  string errorMsg;

  res = getMasterState(errorMsg);
  if (res != TRI_ERROR_NO_ERROR) {
    std::cout << "RES: " << res << ", MSG: " << errorMsg << "\n";
    return res;
  }
  
  res = getLocalState(errorMsg);
  if (res != TRI_ERROR_NO_ERROR) {
    std::cout << "RES: " << res << ", MSG: " << errorMsg << "\n";
    return res;
  }

  bool fullSynchronisation = false;

  if (_applyState._lastTick == 0) {
    // we had never sychronised anything
    fullSynchronisation = true;
  }
  else if (_applyState._lastTick > 0 && 
           _applyState._lastTick < _masterInfo._state._firstTick) {
    // we had synchronised something before, but that point was
    // before the start of the master logs. this would mean a gap
    // in the data, so we'll do a complete re-sync
    fullSynchronisation = true;
  }

  if (fullSynchronisation) {
    LOGGER_INFO("performing full synchronisation with master");

    // nothing applied so far. do a full sync of collections
    res = getMasterInventory(errorMsg);
    if (res != TRI_ERROR_NO_ERROR) {
      std::cout << "RES: " << res << ", MSG: " << errorMsg << "\n";
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyCollectionDump (TRI_voc_cid_t cid,
                                             SimpleHttpResult* response,
                                             string& errorMsg) {
  
  std::stringstream& data = response->getBody();

  while (true) {
    string line;
    
    std::getline(data, line, '\n');

    if (line.size() < 2) {
      return TRI_ERROR_NO_ERROR;
    }

    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, line.c_str());

    if (! JsonHelper::isArray(json)) {
      if (json != 0) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      }

      errorMsg = "received invalid JSON data for collection " + StringUtils::itoa(cid);
      
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
 
    TRI_json_t* type = JsonHelper::getArrayElement(json, "type");

    if (! JsonHelper::isString(type)) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      
      errorMsg = "received invalid JSON data for collection " + StringUtils::itoa(cid);

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
    
//    std::cout << "type: " << type->_value._string.data << "\n";

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get local replication apply state
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getLocalState (string& errorMsg) {
  int res;

  res = TRI_LoadApplyStateReplication(_vocbase, &_applyState);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // no state file found, so this is the initialisation
    _applyState._serverId = _masterInfo._serverId;

    res = TRI_SaveApplyStateReplication(_vocbase, &_applyState, true);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "could not save replication state information";
    }
  }
  else {
    if (_masterInfo._serverId != _applyState._serverId) {
      res = TRI_ERROR_REPLICATION_MASTER_CHANGE;
      errorMsg = "encountered wrong master id in replication state file. " 
                 "found: " + StringUtils::itoa(_masterInfo._serverId) + ", " 
                 "expected: " + StringUtils::itoa(_applyState._serverId);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getMasterState (string& errorMsg) {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  map<string, string> headers;

  // send request
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                "/_api/replication/state",
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint);

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;

  if (! response->isComplete()) {
    res = TRI_ERROR_REPLICATION_NO_RESPONSE;
   
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
               ": " + _client->getErrorMessage();
  }
  else {
    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                 ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + 
                 ": " + response->getHttpReturnMessage();
    }
    else {
      TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().str().c_str());

      if (json != 0) {
        res = handleStateResponse(json, errorMsg);

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
    }
    // std::cout << response->getBody().str() << std::endl;
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master inventory
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getMasterInventory (string& errorMsg) {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  map<string, string> headers;

  // send request
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                "/_api/replication/inventory",
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint);

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;
  
  if (! response->isComplete()) {
    res = TRI_ERROR_REPLICATION_NO_RESPONSE;
   
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
               ": " + _client->getErrorMessage();
  }
  else {
    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                 ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + 
                 ": " + response->getHttpReturnMessage();
    }
    else {
      TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().str().c_str());

      if (json != 0) {
        res = handleInventoryResponse(json, errorMsg);

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
    }
    // std::cout << response->getBody().str() << std::endl;
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::handleCollectionDump (TRI_voc_cid_t cid,
                                              TRI_voc_tick_t maxTick,
                                              string& errorMsg) {
  static const uint64_t chunkSize = 1024 * 1024; 

  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }


  const string baseUrl = "/_api/replication/dump"  
                         "?collection=" + StringUtils::itoa(cid) + 
                         "&chunkSize=" + StringUtils::itoa(chunkSize);

  map<string, string> headers;

  TRI_voc_tick_t fromTick = 0;

  while (true) {
    string url = baseUrl + 
                 "&from=" + StringUtils::itoa(fromTick) + 
                 "&to=" + StringUtils::itoa(maxTick);

    // send request
    SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                  url,
                                                  0, 
                                                  0,  
                                                  headers); 

    if (response == 0) {
      errorMsg = "could not connect to master at " + string(_masterInfo._endpoint);

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    if (! response->isComplete()) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                 ": " + _client->getErrorMessage();

      delete response;

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    if (response->wasHttpError()) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                 ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + 
                 ": " + response->getHttpReturnMessage();
      
      delete response;

      return TRI_ERROR_REPLICATION_MASTER_ERROR;
    }

    bool hasMore = false;
    bool found;
    string header = response->getHeaderField("x-arango-hasmore", found);

    if (found) {
      hasMore = StringUtils::boolean(header);
    }
   
    if (hasMore) { 
      header = response->getHeaderField("x-arango-lastfound", found);

      if (found) {
        TRI_voc_tick_t tick = StringUtils::uint64(header);

        if (tick > fromTick) {
          fromTick = tick;
        }
        else {
          // TODO: raise error
          hasMore = false;
        }
      }
      else {
        // TODO: raise error
        hasMore = false;
      }
    }

    int res = applyCollectionDump(cid, response, errorMsg);

    delete response;

    if (res != TRI_ERROR_NO_ERROR ||
        ! hasMore || 
        fromTick == 0) {
      // done
      return res;
    }
  }
  
  assert(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the information about a collection
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::handleCollectionInitial (TRI_json_t const* json,
                                                 string& errorMsg,
                                                 setup_phase_e phase) {
  
  TRI_json_t const* masterName = JsonHelper::getArrayElement(json, "name");
  ENSURE_JSON(masterName, isString, "collection name is missing in response");
  
  if (TRI_IsSystemCollectionName(masterName->_value._string.data)) {
    // we will not care about system collections
    return TRI_ERROR_NO_ERROR;
  }
  
  if (JsonHelper::getBooleanValue(json, "deleted", false)) {  
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  TRI_json_t const* masterId = JsonHelper::getArrayElement(json, "cid");
  ENSURE_JSON(masterId, isString, "collection id is missing in response");
  
  TRI_json_t const* masterType = JsonHelper::getArrayElement(json, "type");
  ENSURE_JSON(masterType, isNumber, "collection type is missing in response");
  
  TRI_voc_cid_t id = StringUtils::uint64(masterId->_value._string.data);

  TRI_vocbase_col_t* col;
  
  // first look up the collection by the cid
  col = TRI_LookupCollectionByIdVocBase(_vocbase, id);

  if (phase == PHASE_DROP) { 
    if (col == 0) {
      // not found, try name next
      col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName->_value._string.data);
    }

    if (col != 0) {
      LOGGER_INFO("dropping collection '" << col->_name << "', id " << id);

      int res = TRI_DropCollectionVocBase(_vocbase, col);
 
      if (res != TRI_ERROR_NO_ERROR) {
        LOGGER_ERROR("unable to drop collection " << id << ": " << TRI_errno_string(res));

        return res;
      }
    }

    return TRI_ERROR_NO_ERROR;
  }
  else if (phase == PHASE_CREATE) {
    TRI_json_t* keyOptions = 0;

    if (JsonHelper::isArray(JsonHelper::getArrayElement(json, "keyOptions"))) {
      keyOptions = TRI_CopyJson(TRI_CORE_MEM_ZONE, JsonHelper::getArrayElement(json, "keyOptions"));
    }

    TRI_col_info_t params;
    TRI_InitCollectionInfo(_vocbase, 
                           &params, 
                           masterName->_value._string.data, 
                           (TRI_col_type_e) masterType->_value._number,
                           (TRI_voc_size_t) JsonHelper::getNumberValue(json, "maximalSize", (double) TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
                           keyOptions);

    params._doCompact =   JsonHelper::getBooleanValue(json, "doCompact", true); 
    params._waitForSync = JsonHelper::getBooleanValue(json, "waitForSync", _vocbase->_defaultWaitForSync);
    params._isVolatile =  JsonHelper::getBooleanValue(json, "isVolatile", false); 

    LOGGER_INFO("creating collection '" << masterName->_value._string.data << "', id " << id);

    col = TRI_CreateCollectionVocBase(_vocbase, &params, id);

    if (col == 0) {
      int res = TRI_errno();

      LOGGER_ERROR("unable to create collection " << id << ": " << TRI_errno_string(res));

      return res;
    }
  
    return TRI_ERROR_NO_ERROR;
  }
  else if (phase == PHASE_DATA) {
    int res;
  
    LOGGER_INFO("syncing data for collection '" << masterName->_value._string.data << "', id " << id);

    res = handleCollectionDump(id, _masterInfo._state._lastTick, errorMsg);

    return res;
  }

  assert(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::handleStateResponse (TRI_json_t const* json, 
                                             string& errorMsg) {

  // process "state" section
  TRI_json_t const* state = JsonHelper::getArrayElement(json, "state");
  ENSURE_JSON(state, isArray, "state section is missing in response");

  TRI_json_t const* tick = JsonHelper::getArrayElement(state, "firstTick");
  ENSURE_JSON(tick, isString, "firstTick is missing in response");
  const TRI_voc_tick_t firstTick = StringUtils::uint64(tick->_value._string.data);

  tick = JsonHelper::getArrayElement(state, "lastTick");
  ENSURE_JSON(tick, isString, "lastTick is missing in response");
  const TRI_voc_tick_t lastTick = StringUtils::uint64(tick->_value._string.data);

  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getArrayElement(json, "server");
  ENSURE_JSON(server, isArray, "server section is missing in response");

  TRI_json_t const* version = JsonHelper::getArrayElement(server, "version");
  ENSURE_JSON(version, isString, "server version is missing in response");
  
  TRI_json_t const* id = JsonHelper::getArrayElement(server, "serverId");
  ENSURE_JSON(id, isString, "server id is missing in response");


  // validate all values we got
  const TRI_server_id_t masterId = StringUtils::uint64(id->_value._string.data);

  if (masterId == 0) {
    // invalid master id
    errorMsg = "server id in response is invalid";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (masterId == TRI_GetServerId()) {
    // master and replica are the same instance. this is not supported.
    errorMsg = "master's id is the same as the local server's id";

    return TRI_ERROR_REPLICATION_LOOP;
  }

  int major = 0;
  int minor = 0;

  if (sscanf(version->_value._string.data, "%d.%d", &major, &minor) != 2) {
    errorMsg = "invalid master version info: " + string(version->_value._string.data);

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major != 1 ||
      (major == 1 && minor != 4)) {
    errorMsg = "incompatible master version: " + string(version->_value._string.data);

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  _masterInfo._majorVersion     = major;
  _masterInfo._minorVersion     = minor;
  _masterInfo._serverId         = masterId;
  _masterInfo._state._firstTick = firstTick;
  _masterInfo._state._lastTick  = lastTick;
  _masterInfo._state._active    = running;

  TRI_LogMasterInfoReplication(&_masterInfo, "connected to");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::handleInventoryResponse (TRI_json_t const* json, 
                                                 string& errorMsg) {
  TRI_json_t const* collections = JsonHelper::getArrayElement(json, "collections");
  ENSURE_JSON(collections, isList, "collections section is missing from response");
  
  const size_t n = collections->_value._objects._length;

  // iterate over all collections from the master...
  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);
    ENSURE_JSON(collection, isArray, "collection declaration is invalid in response");
  
    TRI_json_t const* parameters = JsonHelper::getArrayElement(collection, "parameters");
    ENSURE_JSON(parameters, isArray, "parameters section is missing from response");

    /* TODO: handle indexes
    TRI_json_t const* indexes = JsonHelper::getArrayElement(collection, "indexes");
    ENSURE_JSON(indexes, isList, "indexes section is missing from response");
    LOGGER_INFO(JsonHelper::toString(indexes));
    */

    // drop the collection if it exists locally
    int res = handleCollectionInitial(parameters, errorMsg, PHASE_DROP);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // iterate over all collections from the master again
  if (n > 0) {
    // we'll sleep for a while to allow the collections to be dropped (asynchronously)
    // TODO: find a safer mechanism for waiting until we can beginning creating collections
    sleep(5);

    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);
    
      TRI_json_t const* parameters = JsonHelper::getArrayElement(collection, "parameters");

      // now re-create the collection locally
      int res = handleCollectionInitial(parameters, errorMsg, PHASE_CREATE);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }

  // all collections created. now sync collection data
  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);
    
    TRI_json_t const* parameters = JsonHelper::getArrayElement(collection, "parameters");
      
    // now sync collection data
    int res = handleCollectionInitial(parameters, errorMsg, PHASE_DATA);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
