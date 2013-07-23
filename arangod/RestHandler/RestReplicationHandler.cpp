////////////////////////////////////////////////////////////////////////////////
/// @brief replication request handler
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestReplicationHandler.h"

#include "build.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/replication-logger.h"
#include "VocBase/server-id.h"

#ifdef TRI_ENABLE_REPLICATION

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

  
const uint64_t RestReplicationHandler::minChunkSize = 512 * 1024;

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

RestReplicationHandler::RestReplicationHandler (HttpRequest* request, 
                                                TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestReplicationHandler::~RestReplicationHandler () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestReplicationHandler::queue () const {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e RestReplicationHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();
  
  vector<string> const& suffix = _request->suffix();

  const size_t len = suffix.size();

  if (len == 1) {
    const string& command = suffix[0];

    if (command == "log-start") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandLogStart();
    }
    else if (command == "log-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandLogStop();
    }
    else if (command == "log-state") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLogState();
    }
    else if (command == "log-follow") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLogFollow(); 
    }
    else if (command == "inventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandInventory();
    }
    else if (command == "dump") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandDump(); 
    }
    else if (command == "apply-config") {
      if (type == HttpRequest::HTTP_REQUEST_GET) {
        handleCommandApplyGetConfig();
      }
      else {
        if (type != HttpRequest::HTTP_REQUEST_PUT) {
          goto BAD_CALL;
        }
        handleCommandApplySetConfig();
      }
    }
    else if (command == "apply-start") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandApplyStart();
    }
    else if (command == "apply-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandApplyStop();
    }
    else if (command == "apply-state") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandApplyState();
    }
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }
      
    return Handler::HANDLER_DONE;
  }

BAD_CALL:
  if (len != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  }
  else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  
  return Handler::HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief filter a collection based on collection attributes
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::filterCollection (TRI_vocbase_col_t* collection, 
                                               void* data) {
  const char* name = collection->_name;

  if (name == NULL) {
    // invalid collection
    return false;
  }

  if (collection->_type != (TRI_col_type_t) TRI_COL_TYPE_DOCUMENT && 
      collection->_type != (TRI_col_type_t) TRI_COL_TYPE_EDGE) {
    // invalid type
    return false;
  }

  if (*name == '_' && TRI_ExcludeCollectionReplication(name)) {
    // system collection
    return false;
  }

  // all other cases should be included
  return true;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the applier action into an action list
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::insertApplier () {
  bool found;
  char const* value;

  value = _request->value("serverId", found);

  if (found) {
    TRI_server_id_t serverId = (TRI_server_id_t) StringUtils::uint64(value);

    if (serverId > 0) {
      TRI_UpdateClientReplicationLogger(_vocbase->_replicationLogger, serverId, _request->fullUrl().c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the minimum chunk size
////////////////////////////////////////////////////////////////////////////////
  
uint64_t RestReplicationHandler::determineChunkSize () const {
  // determine chunk size
  uint64_t chunkSize = minChunkSize;

  bool found;
  const char* value = _request->value("chunkSize", found);

  if (found) {
    chunkSize = (uint64_t) StringUtils::uint64(value);
  }
  if (chunkSize < minChunkSize) {
    chunkSize = minChunkSize;
  }

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remotely start the replication logger
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLogStart () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StartReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;
    
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remotely stop the replication logger
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLogStop () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StopReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;
    
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, false));

    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the state of the replication logger
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLogState () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_replication_log_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;

    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
    
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "state", TRI_JsonStateReplicationLogger(&state)); 
    
    // add server info
    TRI_json_t* server = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, server, "version", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRIAGENS_VERSION));

    TRI_server_id_t serverId = TRI_GetServerId();  
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, server, "serverId", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, TRI_StringUInt64(serverId)));

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "server", server);
    
    TRI_json_t* clients = TRI_JsonClientsReplicationLogger(_vocbase->_replicationLogger);
    if (clients != 0) {
      TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "clients", clients);
    }

    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a follow command for the replication log
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLogFollow () {
  // determine start tick
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = (TRI_voc_tick_t) UINT64_MAX;
  bool found;
  char const* value;
  
  value = _request->value("from", found);
  if (found) {
    tickStart = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  // determine end tick for dump
  value = _request->value("to", found);
  if (found) {
    tickEnd = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }
  
  const uint64_t chunkSize = determineChunkSize(); 
  
  // initialise the dump container
  TRI_replication_dump_t dump; 
  TRI_InitDumpReplication(&dump);
  dump._buffer = TRI_CreateSizedStringBuffer(TRI_CORE_MEM_ZONE, (size_t) minChunkSize);

  if (dump._buffer == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  int res = TRI_DumpLogReplication(_vocbase, &dump, tickStart, tickEnd, chunkSize);
  
  TRI_replication_log_state_t state;

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);
  }
 
  if (res == TRI_ERROR_NO_ERROR) { 
    const bool checkMore = (dump._lastFoundTick > 0 && dump._lastFoundTick != state._lastLogTick);

    // generate the result
    _response = createResponse(HttpResponse::OK);

    _response->setContentType("application/x-arango-dump; charset=utf-8");

    // set headers
    _response->setHeader(TRI_REPLICATION_HEADER_CHECKMORE, 
                         strlen(TRI_REPLICATION_HEADER_CHECKMORE), 
                         checkMore ? "true" : "false");

    _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED, 
                         strlen(TRI_REPLICATION_HEADER_LASTINCLUDED), 
                         StringUtils::itoa(dump._lastFoundTick));
    
    _response->setHeader(TRI_REPLICATION_HEADER_LASTTICK, 
                         strlen(TRI_REPLICATION_HEADER_LASTTICK), 
                         StringUtils::itoa(state._lastLogTick));
    
    _response->setHeader(TRI_REPLICATION_HEADER_ACTIVE, 
                         strlen(TRI_REPLICATION_HEADER_ACTIVE), 
                         state._active ? "true" : "false");

    // transfer ownership of the buffer contents
    _response->body().appendText(TRI_BeginStringBuffer(dump._buffer), TRI_LengthStringBuffer(dump._buffer));
    // avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
    
    insertApplier();
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, res);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, dump._buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the inventory (current replication and collection state)
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_voc_tick_t tick = TRI_CurrentTickVocBase();
  
  // collections
  TRI_json_t* collections = TRI_InventoryCollectionsVocBase(_vocbase, tick, &filterCollection, NULL);

  TRI_replication_log_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_CORE_MEM_ZONE, collections);

    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;
  
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);

    // add collections data
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "collections", collections);
    
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "state", TRI_JsonStateReplicationLogger(&state)); 
  
    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
    
    insertApplier();
  }
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief handle a dump command for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump () {
  char const* collection = _request->value("collection");
    
  if (collection == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }
  
  // determine start tick for dump
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = (TRI_voc_tick_t) UINT64_MAX;
  bool found;
  char const* value;
  
  value = _request->value("from", found);
  if (found) {
    tickStart = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  // determine end tick for dump
  value = _request->value("to", found);
  if (found) {
    tickEnd = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }
  
  const uint64_t chunkSize = determineChunkSize(); 

  TRI_vocbase_col_t* c = TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  const TRI_voc_cid_t cid = c->_cid;

  LOGGER_DEBUG("request collection dump for collection '" << collection << "', "
               "tickStart: " << tickStart << ", tickEnd: " << tickEnd);

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }
  

  // initialise the dump container
  TRI_replication_dump_t dump; 
  TRI_InitDumpReplication(&dump);
  dump._buffer = TRI_CreateSizedStringBuffer(TRI_CORE_MEM_ZONE, (size_t) minChunkSize);

  if (dump._buffer == 0) {
    TRI_ReleaseCollectionVocBase(_vocbase, col);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);

    return;
  }

  int res = TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd, chunkSize);
  
  TRI_ReleaseCollectionVocBase(_vocbase, col);

  if (res == TRI_ERROR_NO_ERROR) {
    // generate the result
    _response = createResponse(HttpResponse::OK);

    _response->setContentType("application/x-arango-dump; charset=utf-8");

    // set headers
    _response->setHeader(TRI_REPLICATION_HEADER_CHECKMORE, 
                         strlen(TRI_REPLICATION_HEADER_CHECKMORE), 
                         ((dump._hasMore || dump._bufferFull) ? "true" : "false"));

    _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED, 
                         strlen(TRI_REPLICATION_HEADER_LASTINCLUDED), 
                         StringUtils::itoa(dump._lastFoundTick));
    
    // transfer ownership of the buffer contents
    _response->body().appendText(TRI_BeginStringBuffer(dump._buffer), TRI_LengthStringBuffer(dump._buffer));
    // avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
    
    insertApplier();
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, res);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, dump._buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the configuration of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplyGetConfig () {
  assert(_vocbase->_replicationApplier != 0);
    
  TRI_replication_apply_configuration_t config;
  TRI_InitApplyConfigurationReplicationApplier(&config);
    
  TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  TRI_CopyApplyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  
  TRI_json_t* result = TRI_JsonApplyConfigurationReplicationApplier(&config);
  TRI_DestroyApplyConfigurationReplicationApplier(&config);
    
  if (result == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
  else {
    generateResult(result);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplySetConfig () {
  assert(_vocbase->_replicationApplier != 0);
  
  TRI_replication_apply_configuration_t config;
  TRI_InitApplyConfigurationReplicationApplier(&config);
  
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return;
  }

  const string endpoint = JsonHelper::getStringValue(json, "endpoint", "");

  config._endpoint          = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._requestTimeout    = JsonHelper::getDoubleValue(json, "requestTimeout", config._requestTimeout);
  config._connectTimeout    = JsonHelper::getDoubleValue(json, "connectTimeout", config._connectTimeout);
  config._ignoreErrors      = JsonHelper::getUInt64Value(json, "ignoreErrors", config._ignoreErrors);
  config._maxConnectRetries = JsonHelper::getIntValue(json, "maxConnectRetries", config._maxConnectRetries);
  config._autoStart         = JsonHelper::getBooleanValue(json, "autoStart", config._autoStart);
  config._adaptivePolling   = JsonHelper::getBooleanValue(json, "adaptivePolling", config._adaptivePolling);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);

  int res = TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);
  
  TRI_DestroyApplyConfigurationReplicationApplier(&config);
    
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    handleCommandApplyState();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplyStart () {
  assert(_vocbase->_replicationApplier != 0);
  
  bool found;
  const char* value = _request->value("fullSync", found);

  bool fullSync;
  if (found) {
    fullSync = StringUtils::boolean(value);
  }
  else {
    fullSync = false;
  }

  int res = TRI_StartReplicationApplier(_vocbase->_replicationApplier, fullSync);
    
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    handleCommandApplyState();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplyStop () {
  assert(_vocbase->_replicationApplier != 0);

  int res = TRI_StopReplicationApplier(_vocbase->_replicationApplier, true);
  
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    handleCommandApplyState();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the state of the replication apply
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplyState () {
  assert(_vocbase->_replicationApplier != 0);

  TRI_replication_apply_state_t state;
  
  int res = TRI_StateReplicationApplier(_vocbase->_replicationApplier, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;
    
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
  
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "state", TRI_JsonStateReplicationApplier(&state));
    
    // add server info
    TRI_json_t* server = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, server, "version", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRIAGENS_VERSION));

    TRI_server_id_t serverId = TRI_GetServerId();  
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, server, "serverId", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, TRI_StringUInt64(serverId)));

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "server", server);
   
    TRI_replication_apply_configuration_t config;
    TRI_InitApplyConfigurationReplicationApplier(&config);

    TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
    TRI_CopyApplyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
    TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);

    if (config._endpoint != NULL) {
      TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "endpoint", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, config._endpoint));
    }
    TRI_DestroyApplyConfigurationReplicationApplier(&config);

    generateResult(&result);

    TRI_DestroyApplyStateReplicationApplier(&state);
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
