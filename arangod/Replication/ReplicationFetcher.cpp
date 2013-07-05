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
#include "BasicsC/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/DocumentHelper.h"
#include "VocBase/collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/primary-collection.h"
#include "VocBase/replication.h"
#include "VocBase/server-id.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
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
/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges), then name 
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::sortCollections (const void* l, const void* r) {
  TRI_json_t const* left  = JsonHelper::getArrayElement((TRI_json_t const*) l, "parameters");
  TRI_json_t const* right = JsonHelper::getArrayElement((TRI_json_t const*) r, "parameters");

  int leftType  = (int) JsonHelper::getNumberValue(left, "type", 2);
  int rightType = (int) JsonHelper::getNumberValue(right, "type", 2);


  if (leftType != rightType) {
    return leftType - rightType;
  }

  string leftName  = JsonHelper::getStringValue(left, "name", "");
  string rightName = JsonHelper::getStringValue(right, "name", "");

  return strcmp(leftName.c_str(), rightName.c_str());
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
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyMarker (TRI_transaction_collection_t* trxCollection,
                                     char const* type,
                                     const TRI_voc_key_t key,
                                     TRI_json_t const* json,
                                     string& errorMsg) {

  if (TRI_EqualString(type, "marker-document") ||
      TRI_EqualString(type, "marker-edge")) {
    // {"type":"marker-document","key":"230274209405676","doc":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_primary_collection_t* primary = trxCollection->_collection->_collection;

    TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);

    int res;

    if (shaped != 0) {
      TRI_doc_mptr_t mptr;

      res = primary->read(trxCollection, key, &mptr, false);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert
        const TRI_voc_rid_t rid = StringUtils::uint64(JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_REV, ""));

        if (type[7] == 'e') {
          // edge
          if (primary->base._info._type != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_ERROR_NO_ERROR;
          }

          string from = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_FROM, "");
          string to   = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_TO, "");
          

          // parse _from
          TRI_document_edge_t edge;
          if (! DocumentHelper::parseDocumentId(from.c_str(), edge._fromCid, &edge._fromKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }
          
          // parse _to
          if (! DocumentHelper::parseDocumentId(to.c_str(), edge._toCid, &edge._toKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          if (res == TRI_ERROR_NO_ERROR) {
            res = primary->insert(trxCollection, key, rid, &mptr, TRI_DOC_MARKER_KEY_EDGE, shaped, &edge, false, false);
          }
        }
        else {
          // document
          if (primary->base._info._type != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = primary->insert(trxCollection, key, rid, &mptr, TRI_DOC_MARKER_KEY_DOCUMENT, shaped, 0, false, false);
          }
        }
      }
      else {
        // update
    
        TRI_doc_update_policy_t policy;
        TRI_InitUpdatePolicy(&policy, TRI_DOC_UPDATE_LAST_WRITE, 0, 0); 

        res = primary->update(trxCollection, key, &mptr, shaped, &policy, false, false);
      }
      
      TRI_FreeShapedJson(primary->_shaper, shaped);
    }
    else {
      res = TRI_ERROR_OUT_OF_MEMORY;
      errorMsg = TRI_errno_string(res);
    }

    return res; 
  }

  else if (TRI_EqualString(type, "marker-deletion")) {
    // {"type":"marker-deletion","key":"592063"}

    TRI_doc_update_policy_t policy;
    TRI_InitUpdatePolicy(&policy, TRI_DOC_UPDATE_LAST_WRITE, 0, 0); 

    TRI_primary_collection_t* primary = trxCollection->_collection->_collection;

    int res = primary->remove(trxCollection, key, &policy, false, false);

    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
      else {
        errorMsg = "document removal operation failed: " + string(TRI_errno_string(res));
      }
    }

    return res;
  }

  else {
    errorMsg = "unexpected marker type '" + string(type) + "'";

    return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyCollectionDump (TRI_transaction_collection_t* trxCollection,
                                             SimpleHttpResult* response,
                                             string& errorMsg,
                                             uint64_t& markerCount) {
  
  const string invalidMsg = "received invalid JSON data for collection " + 
                            StringUtils::itoa(trxCollection->_cid);

  std::stringstream& data = response->getBody();

  while (true) {
    string line;
    
    std::getline(data, line, '\n');

    if (line.size() < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }
      
    ++markerCount;

    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, line.c_str());

    if (! JsonHelper::isArray(json)) {
      if (json != 0) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      }

      errorMsg = invalidMsg;
      
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    const char* type = 0;
    const char* key  = 0;
    TRI_json_t const* doc = 0;
    bool isValid     = true;

    const size_t n = json->_value._objects._length;

    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* element = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i);

      if (element == 0 || 
          element->_type != TRI_JSON_STRING ||
          element->_value._string.data == 0 ||
          (i + 1) == n) {
        isValid = false;
      }
 
      if (isValid) {
        const char* attributeName = element->_value._string.data;
        TRI_json_t const* value = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i + 1);
        
        if (TRI_EqualString(attributeName, "type")) {
          if (value == 0 || 
              value->_type != TRI_JSON_STRING ||
              value->_value._string.data == 0) {
            isValid = false;
          }
          else {
            type = value->_value._string.data;
          }
        }

        else if (TRI_EqualString(attributeName, "key")) {
          if (value == 0 || 
              value->_type != TRI_JSON_STRING ||
              value->_value._string.data == 0) {
            isValid = false;
          }
          else {
            key = value->_value._string.data;
          }
        }

        else if (TRI_EqualString(attributeName, "doc")) {
          if (value == 0 || 
              value->_type != TRI_JSON_ARRAY) {
            isValid = false;
          }
          else {
            doc = value;
          }
        }
      }
    }

    if (! isValid) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      errorMsg = invalidMsg;
      
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
 
    int res = applyMarker(trxCollection, type, (const TRI_voc_key_t) key, doc, errorMsg);
      
    if (res != TRI_ERROR_NO_ERROR) {
      std::cout << JsonHelper::toString(json);
    }

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
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

      if (json != 0 && json->_type == TRI_JSON_ARRAY) {
        res = handleInventoryResponse(json, errorMsg);

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      else {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

        errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                   ": invalid JSON";
      }
    }
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::handleCollectionDump (TRI_transaction_collection_t* trxCollection,
                                              TRI_voc_tick_t maxTick,
                                              string& errorMsg) {
  static const uint64_t chunkSize = 2 * 1024 * 1024; 

  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }


  const string baseUrl = "/_api/replication/dump"  
                         "?collection=" + StringUtils::itoa(trxCollection->_cid) + 
                         "&chunkSize=" + StringUtils::itoa(chunkSize);

  map<string, string> headers;

  TRI_voc_tick_t fromTick    = 0;
  uint64_t       markerCount = 0;

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

    int res;
    bool checkMore = false;
    bool found;
    string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);

    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;
    }
    else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                 ": header '" TRI_REPLICATION_HEADER_CHECKMORE "' is missing";
    }
   
    if (checkMore) { 
      header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTFOUND, found);

      if (found) {
        TRI_voc_tick_t tick = StringUtils::uint64(header);

        if (tick > fromTick) {
          fromTick = tick;
        }
        else {
          // we got the same tick again, this indicates we're at the end
          checkMore = false;
        }
      }
      else {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                   ": header '" TRI_REPLICATION_HEADER_LASTFOUND "' is missing";
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = applyCollectionDump(trxCollection, response, errorMsg, markerCount);
    }

    delete response;

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
      // done
      if (markerCount > 0) {
        LOGGER_INFO("successfully transferred " << markerCount << " data markers");
      }
      
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
  TRI_voc_cid_t cid = StringUtils::uint64(masterId->_value._string.data, masterId->_value._string.length - 1);
  
  TRI_json_t const* masterType = JsonHelper::getArrayElement(json, "type");
  ENSURE_JSON(masterType, isNumber, "collection type is missing in response");
  
  TRI_vocbase_col_t* col;
  
  // first look up the collection by the cid
  col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (phase == PHASE_DROP) { 
    if (col == 0) {
      // not found, try name next
      col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName->_value._string.data);
    }

    if (col != 0) {
      LOGGER_INFO("dropping collection '" << col->_name << "', id " << cid);

      int res = TRI_DropCollectionVocBase(_vocbase, col);
 
      if (res != TRI_ERROR_NO_ERROR) {
        LOGGER_ERROR("unable to drop collection " << cid << ": " << TRI_errno_string(res));

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

    LOGGER_INFO("creating collection '" << masterName->_value._string.data << "', id " << cid);

    col = TRI_CreateCollectionVocBase(_vocbase, &params, cid);

    if (col == 0) {
      int res = TRI_errno();

      LOGGER_ERROR("unable to create collection " << cid << ": " << TRI_errno_string(res));

      return res;
    }
  
    return TRI_ERROR_NO_ERROR;
  }
  else if (phase == PHASE_DATA) {
    int res;
  
    LOGGER_INFO("syncing data for collection '" << masterName->_value._string.data << "', id " << cid);

    TRI_transaction_t* trx = TRI_CreateTransaction(_vocbase->_transactionContext, false, 0.0, false);

    if (trx == 0) {
      errorMsg = "unable to start transaction";

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeTransaction(trx);
      errorMsg = "unable to start transaction";

      return res;
    }
    
    res = TRI_BeginTransaction(trx, (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION, TRI_TRANSACTION_TOP_LEVEL);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeTransaction(trx);
      errorMsg = "unable to start transaction";
    
      return TRI_ERROR_INTERNAL;
    }
  
    TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE);

    if (trxCollection == NULL) {
      res = TRI_ERROR_INTERNAL;
    }
    else {
      res = handleCollectionDump(trxCollection, _masterInfo._state._lastTick, errorMsg);
    }


    if (res == TRI_ERROR_NO_ERROR) {
      TRI_CommitTransaction(trx, 0);
    }
      
    TRI_FreeTransaction(trx);

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
  const TRI_voc_tick_t firstTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  tick = JsonHelper::getArrayElement(state, "lastTick");
  ENSURE_JSON(tick, isString, "lastTick is missing in response");
  const TRI_voc_tick_t lastTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getArrayElement(json, "server");
  ENSURE_JSON(server, isArray, "server section is missing in response");

  TRI_json_t const* version = JsonHelper::getArrayElement(server, "version");
  ENSURE_JSON(version, isString, "server version is missing in response");
  
  TRI_json_t const* id = JsonHelper::getArrayElement(server, "serverId");
  ENSURE_JSON(id, isString, "server id is missing in response");


  // validate all values we got
  const TRI_server_id_t masterId = StringUtils::uint64(id->_value._string.data, id->_value._string.length);

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
  TRI_json_t* collections = JsonHelper::getArrayElement(json, "collections");
  ENSURE_JSON(collections, isList, "collections section is missing from response");
  
  const size_t n = collections->_value._objects._length;
  if (n > 1) {
    // sort by collection type (vertices before edges), then name
    qsort(collections->_value._objects._buffer, n, sizeof(TRI_json_t), &ReplicationFetcher::sortCollections);
  }

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
