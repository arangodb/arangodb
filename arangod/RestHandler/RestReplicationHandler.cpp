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
  else if (command == "inventory") {
    handleCommandInventory();
  }
/*
  else if (command == "dump-initial-collection" && foundCollection) {
  bool foundCollection;
  char const* collection = _request->value("collection", foundCollection);

    handleInitialDumpCollection(collection);
  }
  else if (command == "dump-initial-end") {
    handleInitialDumpEnd();
  }
  else if (command == "dump-continuous") {
    handleContinuousDump();
  }
  */
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
/// @brief whether or not to include a collection in replication
////////////////////////////////////////////////////////////////////////////////
    
bool RestReplicationHandler::shouldIncludeCollection (const char* name) {
  // TODO: exclude certain collections from replication
  return true; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a part of a collection
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::dumpCollection (TRI_voc_cid_t cid,
                                            TRI_voc_tick_t tickStart,
                                            TRI_voc_tick_t tickEnd,
                                            uint64_t chunkSize) {
  return TRI_ERROR_NO_ERROR;
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
/// @brief return the inventory (current replication and collection state)
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_replication_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res);
  }
  else {
    TRI_json_t result;
    
    TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, state._active));

    char* firstString = TRI_StringUInt64(state._firstTick);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "firstTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, firstString));

    char* lastString = TRI_StringUInt64(state._lastTick);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "lastTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastString));

    generateResult(&result);
  
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle replication initialisation
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::handleInitialDumpStart () {
  TRI_voc_tick_t tick = TRI_CurrentTickVocBase();

  // TODO: activate replication log here

  TRI_json_t result;
  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
  

  // collections
  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(_vocbase);
  const size_t n = colls._length;
  TRI_json_t* collections = TRI_CreateList2Json(TRI_CORE_MEM_ZONE, n);

  for (size_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* c = (TRI_vocbase_col_t const*) colls._buffer[i];

    if (! shouldIncludeCollection(c->_name)) {
      continue;
    }
    
    TRI_json_t* name = TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, c->_name);
    TRI_json_t* cid  = TRI_CreateStringJson(TRI_CORE_MEM_ZONE, TRI_StringUInt64((uint64_t) c->_cid));

    TRI_json_t* collection = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, collection, "name", name);
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, collection, "cid", cid);

    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, collections, collection);
  }
  
  TRI_DestroyVectorPointer(&colls);

  // server data  
  TRI_json_t* server = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, server, "version", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRIAGENS_VERSION));
  
  // put it all together 
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "tick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, TRI_StringUInt64((uint64_t) tick)));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "server", server);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "collections", collections);

  generateResult(&result);

  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle dump of a single collection
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::handleInitialDumpCollection (const char* collection) {
  static const uint64_t minChunkSize = 1 * 1024 * 1024;
  
  assert(collection != 0);
    
  if (! shouldIncludeCollection(collection)) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection");
    return false;
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
    return false;
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

  LOGGER_DEBUG("request collection dump for collection " << collection << 
               ", tickStart: " << tickStart << ", tickEnd: " << tickEnd);

  TRI_vocbase_col_t* col = TRI_UseCollectionByNameVocBase(_vocbase, collection);
  if (col == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection");
    return false;
  }

  dumpCollection(col->_cid, tickStart, tickEnd, chunkSize);
  TRI_ReleaseCollectionVocBase(_vocbase, col);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle end of initial dump
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::handleInitialDumpEnd () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continuous streaming of replication log
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::handleContinuousDump () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
