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
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/replication.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

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

  // only POST is allowed
  if (type != HttpRequest::HTTP_REQUEST_POST) { 
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return Handler::HANDLER_DONE;
  }
  
  const size_t len = _request->suffix().size();
  if (len != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting POST /_api/replication/<command>");
    return Handler::HANDLER_DONE;
  }
  
  vector<string> const& suffix = _request->suffix();
  const string& command = suffix[0];

  if (command == "start") {
    handleCommandStart();
  }
  else if (command == "stop") {
    handleCommandStop();
  }
  else if (command == "state") {
    handleCommandState();
  }
  else if (command == "inventory") {
    handleCommandInventory();
  }
  else if (command == "dump") {
    handleCommandDump(); 
  }
  else {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting POST /_api/replication/<command>");
    return Handler::HANDLER_DONE;
  }

  // success
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
/// @brief exclude a collection from replication?
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::excludeCollection (const char* name) {
  if (TRI_EqualString(name, TRI_COL_NAME_DATABASES)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_ENDPOINTS)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_PREFIXES)) {
    return true;
  }

  if (TRI_EqualString(name, TRI_COL_NAME_REPLICATION)) {
    return true;
  }

  if (TRI_EqualString(name, TRI_COL_NAME_TRANSACTION)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_USERS)) {
    return true;
  }

  return false;
}

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

  if (*name == '_' && excludeCollection(name)) {
    // system collection
    return false;
  }

  TRI_voc_tick_t* tick = (TRI_voc_tick_t*) data;

  if (collection->_cid > *tick) {
    // collection is too new?
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
/// @brief add replication state to a JSON array
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::addState (TRI_json_t* dst, 
                                       TRI_replication_state_t const* state) {

  TRI_json_t* stateJson = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 3);

  // add replication state
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, stateJson, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, state->_active));

  char* firstString = TRI_StringUInt64(state->_firstTick);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, stateJson, "firstTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, firstString));

  char* lastString = TRI_StringUInt64(state->_lastTick);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, stateJson, "lastTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastString));
  
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, dst, "state", stateJson);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remotely start the replication
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandStart () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StartReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res);
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
/// @brief remotely stop the replication
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandStop () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StopReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res);
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
/// @brief return the state of the replication
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandState () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_replication_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res);
  }
  else {
    TRI_json_t result;
    
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
    
    addState(&result, &state);

    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the inventory (current replication and collection state)
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_voc_tick_t tick = TRI_CurrentTickVocBase();
  
  // collections
  TRI_json_t* collections = TRI_ParametersCollectionsVocBase(_vocbase, &filterCollection, &tick);

  TRI_replication_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_CORE_MEM_ZONE, collections);

    generateError(HttpResponse::BAD, res);
  }
  else {
    TRI_json_t result;
  
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);

    // add server info
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "version", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRIAGENS_VERSION));
    
    // add collections state
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "collections", collections);

    addState(&result, &state);
  
    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief handle a dump command for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump () {
  static const uint64_t minChunkSize = 16 * 1024;

  char const* collection = _request->value("collection");
    
  if (collection == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }
  
  // determine start tick for dump
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = 0;
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
  
  // determine chunk size
  uint64_t chunkSize = minChunkSize;

  value = _request->value("chunkSize", found);
  if (found) {
    chunkSize = (uint64_t) StringUtils::uint64(value);
  }
  if (chunkSize < minChunkSize) {
    chunkSize = minChunkSize;
  }

  const TRI_voc_cid_t cid = (TRI_voc_cid_t) StringUtils::uint64(collection);

  LOGGER_DEBUG("request collection dump for collection " << cid << 
               ", tickStart: " << tickStart << ", tickEnd: " << tickEnd);

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    generateError(HttpResponse::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "collection not found");
    return;
  }
  
  // TODO: block compaction
  // TRI_ReadLockReadWriteLock(((TRI_primary_collection_t*) collection)->_compactionLock);


  TRI_string_buffer_t buffer; 
  // initialise the buffer
  TRI_InitSizedStringBuffer(&buffer, TRI_CORE_MEM_ZONE, (size_t) minChunkSize);

  int res = TRI_DumpCollectionReplication(&buffer, col, tickStart, tickEnd, chunkSize);
  
  //TRI_ReadUnlockReadWriteLock(((TRI_primary_collection_t*) collection)->_compactionLock);

  TRI_ReleaseCollectionVocBase(_vocbase, col);

  if (res == TRI_ERROR_NO_ERROR) {
    // generate the JSON result
    _response = createResponse(HttpResponse::OK);
    _response->setContentType("application/json; charset=utf-8");

    _response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));
    buffer._buffer = 0;
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, res);
  }

  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
