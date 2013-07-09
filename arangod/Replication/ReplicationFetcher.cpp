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

#define LOGGER_REPLICATION LOGGER_INFO
  
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

int ReplicationFetcher::run (bool forceFullSynchronisation,
                             string& errorMsg) {
  int res;

  res = getMasterState(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  res = getLocalState(errorMsg, forceFullSynchronisation);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (_applyState._lastInitialTick == 0) {
    // we had never sychronised anything
    forceFullSynchronisation = true;
  }
  else if (_applyState._lastContinuousTick > 0 && 
           _applyState._lastContinuousTick < _masterInfo._state._firstLogTick) {
    // we had synchronised something before, but that point was
    // before the start of the master logs. this would mean a gap
    // in the data, so we'll do a complete re-sync
    forceFullSynchronisation = true;
  }

  if (forceFullSynchronisation) {
    LOGGER_INFO("performing full synchronisation with master");

    // nothing applied so far. do a full sync of collections
    res = getMasterInventory(errorMsg);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  LOGGER_INFO("starting incremental synchronisation with master");

  res = runContinuous(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
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

  int leftType  = (int) JsonHelper::getNumberValue(left,  "type", 2);
  int rightType = (int) JsonHelper::getNumberValue(right, "type", 2);


  if (leftType != rightType) {
    return leftType - rightType;
  }

  string leftName  = JsonHelper::getStringValue(left,  "name", "");
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
/// @brief abort any ongoing transaction
////////////////////////////////////////////////////////////////////////////////

void ReplicationFetcher::abortOngoingTransaction () {
  if (_applyState._trx != 0) {
    TRI_FreeTransaction(_applyState._trx);
    _applyState._trx = 0;
    _applyState._externalTid = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a transaction for a single operation
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* ReplicationFetcher::createSingleOperationTransaction (TRI_voc_cid_t cid) {
  TRI_transaction_t* trx = TRI_CreateTransaction(_vocbase->_transactionContext, false, 0.0, false);

std::cout << "PROC TRX " << cid << "\n";
std::cout << "PROC TRX " << TRI_LookupCollectionByIdVocBase(_vocbase, cid) << "\n";
  if (trx == 0) {
std::cout << "PROC TRX1\n";
    return 0;
  }

  int res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
std::cout << "PROC TRX2 " << cid << "\n";
    TRI_FreeTransaction(trx);

    return 0;
  }
  
  res = TRI_BeginTransaction(trx, (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);

std::cout << "PROC TRX3 " << res << "\n";
    return 0;
  }

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from JSON
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t ReplicationFetcher::getCid (TRI_json_t const* json) const {
  if (json == 0 || json->_type != TRI_JSON_ARRAY) {
    return 0;
  }

  TRI_json_t const* id = JsonHelper::getArrayElement(json, "cid");

  if (JsonHelper::isString(id)) {
    return StringUtils::uint64(id->_value._string.data, id->_value._string.length - 1);
  }
  else if (JsonHelper::isNumber(id)) {
    return (TRI_voc_cid_t) id->_value._number;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::processDocument (TRI_replication_operation_e type,
                                         TRI_json_t const* json, 
                                         string& errorMsg) {
  // extract "cid"
  TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  // extract "key"
  TRI_json_t const* keyJson = JsonHelper::getArrayElement(json, "key");

  if (keyJson == 0 || keyJson->_type != TRI_JSON_STRING || keyJson->_value._string.data == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  // extract "data"
  TRI_json_t const* doc = JsonHelper::getArrayElement(json, "data");

  // extract "tid"
  const string id = JsonHelper::getStringValue(json, "tid", "");
  TRI_voc_tid_t tid;

  if (id.empty()) {
    tid = 0;
  }
  else {
    tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());
  }
    
  if (tid != _applyState._externalTid) {
    // unexpected transaction id. TODO: fix error number
    abortOngoingTransaction();
    
    return TRI_ERROR_INTERNAL;
  }

  if (_applyState._trx != 0) {
    TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(_applyState._trx, cid, TRI_TRANSACTION_WRITE);
  
    if (trxCollection == 0) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    int res = applyCollectionDumpMarker(trxCollection, 
                                        type, 
                                        (const TRI_voc_key_t) keyJson->_value._string.data, 
                                        doc, 
                                        errorMsg);

    return res;
  }

  else {
    TRI_transaction_t* trx = createSingleOperationTransaction(cid);
  
    if (trx == 0) {
      errorMsg = "unable to create transaction";

      return TRI_ERROR_INTERNAL;
    }
    
    TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE);
    
    if (trxCollection == 0) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
    
    int res = applyCollectionDumpMarker(trxCollection, 
                                        type, 
                                        (const TRI_voc_key_t) keyJson->_value._string.data, 
                                        doc, 
                                        errorMsg);

    if (res == TRI_ERROR_NO_ERROR) {
      TRI_CommitTransaction(trx, TRI_TRANSACTION_TOP_LEVEL);
    }
    else {
      TRI_AbortTransaction(trx, TRI_TRANSACTION_TOP_LEVEL);
    }

    TRI_FreeTransaction(trx);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::startTransaction (TRI_json_t const* json) {
  // {"type":2200,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}

  abortOngoingTransaction();

  const string id = JsonHelper::getStringValue(json, "tid", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  TRI_voc_tid_t tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());

  TRI_json_t const* collections = JsonHelper::getArrayElement(json, "collections");

  if (collections == 0 || collections->_type != TRI_JSON_LIST) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
   
  TRI_transaction_t* trx = TRI_CreateTransaction(_vocbase->_transactionContext, false, 0.0, false);

  if (trx == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res;
  uint64_t totalOperations = 0;

  const size_t n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);

    if (collection == 0 || collection->_type != TRI_JSON_ARRAY) {
      TRI_FreeTransaction(trx);

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_voc_cid_t cid = getCid(collection);

    if (cid == 0) {
      TRI_FreeTransaction(trx);

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    uint64_t numOperations = (uint64_t) JsonHelper::getNumberValue(collection, "operations", 0);

    if (numOperations > 0) {
      res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_FreeTransaction(trx);

        return res;
      }

      totalOperations += numOperations;
    }
  }
    
  TRI_transaction_hint_t hint = 0; 
  if (totalOperations == 1) {
    hint = (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION;
  }

  res = TRI_BeginTransaction(trx, hint, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);

    return res;
  }

  _applyState._trx = trx;
  _applyState._externalTid = tid;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commits a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::commitTransaction (TRI_json_t const* json) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  const string id = JsonHelper::getStringValue(json, "tid", "");
  
  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  const TRI_voc_tid_t tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());

  if (_applyState._trx == 0) {
    // invalid state, no transaction was started. TODO: fix error number
    return TRI_ERROR_INTERNAL; 
  }

  if (_applyState._externalTid != tid) {
    // unexpected transaction id. TODO: fix error number
    abortOngoingTransaction();

    return TRI_ERROR_INTERNAL; 
  }

  int res = TRI_CommitTransaction(_applyState._trx, TRI_TRANSACTION_TOP_LEVEL);
  abortOngoingTransaction();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::createCollection (TRI_json_t const* json,
                                          TRI_vocbase_col_t** dst) {
  if (dst != 0) {
    *dst = 0;
  }

  if (json == 0 || json->_type != TRI_JSON_ARRAY) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const string name = JsonHelper::getStringValue(json, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const TRI_col_type_e type = (TRI_col_type_e) JsonHelper::getNumberValue(json, "type", (double) TRI_COL_TYPE_DOCUMENT);

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col != 0 && 
      (TRI_col_type_t) col->_type == (TRI_col_type_t) type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }


  TRI_json_t* keyOptions = 0;

  if (JsonHelper::isArray(JsonHelper::getArrayElement(json, "keyOptions"))) {
    keyOptions = TRI_CopyJson(TRI_CORE_MEM_ZONE, JsonHelper::getArrayElement(json, "keyOptions"));
  }
  
  TRI_col_info_t params;
  TRI_InitCollectionInfo(_vocbase, 
                         &params, 
                         name.c_str(),
                         type,
                         (TRI_voc_size_t) JsonHelper::getNumberValue(json, "maximalSize", (double) TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
                         keyOptions);

  params._doCompact =   JsonHelper::getBooleanValue(json, "doCompact", true); 
  params._waitForSync = JsonHelper::getBooleanValue(json, "waitForSync", _vocbase->_defaultWaitForSync);
  params._isVolatile =  JsonHelper::getBooleanValue(json, "isVolatile", false); 

  LOGGER_INFO("creating collection '" << name << "', id " << cid);

  col = TRI_CreateCollectionVocBase(_vocbase, &params, cid);
  TRI_FreeCollectionInfoOptions(&params);

  if (col == NULL) {
    return TRI_errno();
  }

  if (dst != 0) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::dropCollection (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  return TRI_DropCollectionVocBase(_vocbase, col);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::renameCollection (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* collectionJson = TRI_LookupArrayJson(json, "collection");
  const string name = JsonHelper::getStringValue(collectionJson, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  return TRI_RenameCollectionVocBase(_vocbase, col, name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::createIndex (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* indexJson = JsonHelper::getArrayElement(json, "index");

  if (indexJson == 0 || indexJson->_type != TRI_JSON_ARRAY) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0 || col->_collection == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_index_t* idx;
  TRI_primary_collection_t* primary = col->_collection;

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  int res = TRI_FromJsonIndexDocumentCollection((TRI_document_collection_t*) primary, indexJson, &idx);

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_SaveIndex(primary, idx);
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  TRI_ReleaseCollectionVocBase(_vocbase, col);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::dropIndex (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const string id = JsonHelper::getStringValue(json, "id", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const TRI_idx_iid_t iid = StringUtils::uint64(id);

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0 || col->_collection == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  
  TRI_document_collection_t* document = (TRI_document_collection_t*) col->_collection;

  bool result = TRI_DropIndexDocumentCollection(document, iid);

  TRI_ReleaseCollectionVocBase(_vocbase, col);

  if (! result) {
    // TODO: index not found, should we care??
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyCollectionDumpMarker (TRI_transaction_collection_t* trxCollection,
                                                   TRI_replication_operation_e type,
                                                   const TRI_voc_key_t key,
                                                   TRI_json_t const* json,
                                                   string& errorMsg) {

  if (type == MARKER_DOCUMENT || type == MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    assert(json != 0);

    TRI_primary_collection_t* primary = trxCollection->_collection->_collection;

    TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);

    int res;

    if (shaped != 0) {
      TRI_doc_mptr_t mptr;

      res = primary->read(trxCollection, key, &mptr, false);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert
        const TRI_voc_rid_t rid = StringUtils::uint64(JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_REV, ""));

        if (type == MARKER_EDGE) {
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

  else if (type == MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}

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
    errorMsg = "unexpected marker type " + StringUtils::itoa(type);

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

    TRI_replication_operation_e type = REPLICATION_INVALID;
    const char* key  = 0;
    TRI_json_t const* doc = 0;

    const size_t n = json->_value._objects._length;

    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* element = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i);

      if (element == 0 || element->_type != TRI_JSON_STRING) { 
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        errorMsg = invalidMsg;
      
        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      const char* attributeName = element->_value._string.data;
      TRI_json_t const* value = (TRI_json_t const*) TRI_AtVector(&json->_value._objects, i + 1);

      if (TRI_EqualString(attributeName, "type")) {
        if (value != 0 && value->_type == TRI_JSON_NUMBER) {
          type = (TRI_replication_operation_e) value->_value._number;
        }
      }

      else if (TRI_EqualString(attributeName, "key")) {
        if (value != 0 && value->_type == TRI_JSON_STRING &&value->_value._string.data != 0) {
          key = value->_value._string.data;
        }
      }

      else if (TRI_EqualString(attributeName, "data")) {
        if (value != 0 && value->_type == TRI_JSON_ARRAY) {
          doc = value;
        }
      }
    }

    // key must not be 0, but doc can be 0!
    if (key == 0) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      errorMsg = invalidMsg;
      
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
 
    int res = applyCollectionDumpMarker(trxCollection, type, (const TRI_voc_key_t) key, doc, errorMsg);
      
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a single marker from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyLogMarker (TRI_json_t const* json,
                                        string& errorMsg) {
  static const string invalidMsg = "received invalid JSON data";

  if (json == 0 || json->_type != TRI_JSON_ARRAY) {
    errorMsg = invalidMsg;
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* typeJson = TRI_LookupArrayJson(json, "type");

  if (typeJson == 0 || typeJson->_type != TRI_JSON_NUMBER) {
    errorMsg = invalidMsg;
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_replication_operation_e type = (TRI_replication_operation_e) typeJson->_value._number;

  if (type == MARKER_DOCUMENT || type == MARKER_EDGE || type == MARKER_REMOVE) {
    return processDocument(type, json, errorMsg);
  }

  else if (type == TRANSACTION_START) {
    return startTransaction(json);
  }
  
  else if (type == TRANSACTION_COMMIT) {
    return commitTransaction(json);
  }

  else if (type == COLLECTION_CREATE) {
    TRI_json_t const* collectionJson = TRI_LookupArrayJson(json, "collection");

    return createCollection(collectionJson, 0);
  }
  
  else if (type == COLLECTION_DROP) {
    return dropCollection(json);
  }
  
  else if (type == COLLECTION_RENAME) {
    return renameCollection(json);
  }
  
  else if (type == INDEX_CREATE) {
    return createIndex(json);
  }
  
  else if (type == INDEX_DROP) {
    return dropIndex(json); 
  }
  
  else if (type == REPLICATION_STOP) {
    return TRI_ERROR_NO_ERROR;
  }
  
  else {
    errorMsg = "unexpected marker type " + StringUtils::itoa(type);

    return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::applyLog (SimpleHttpResult* response,
                                  string& errorMsg) {
  
  std::stringstream& data = response->getBody();

  while (true) {
    string line;
    
    std::getline(data, line, '\n');

    if (line.size() < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }
      
    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, line.c_str());

    if (line.size() < 256) {

    std::cout << "JSON: " << JsonHelper::toString(json) << "\n";
    }
    
    int res = applyLogMarker(json, errorMsg);
      
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = TRI_errno_string(res);

      return res;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get local replication apply state
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getLocalState (string& errorMsg, 
                                       bool forceFullSynchronisation) {
  int res;

  if (forceFullSynchronisation) {
    TRI_RemoveApplyStateReplication(_vocbase);
  }

  res = TRI_LoadApplyStateReplication(_vocbase, &_applyState);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // no state file found, so this is the initialisation
    _applyState._serverId = _masterInfo._serverId;

    res = TRI_SaveApplyStateReplication(_vocbase, &_applyState, true);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "could not save replication state information";
    }
  }
  else if (res == TRI_ERROR_NO_ERROR) {
    if (_masterInfo._serverId != _applyState._serverId) {
      res = TRI_ERROR_REPLICATION_MASTER_CHANGE;
      errorMsg = "encountered wrong master id in replication state file. " 
                 "found: " + StringUtils::itoa(_masterInfo._serverId) + ", " 
                 "expected: " + StringUtils::itoa(_applyState._serverId);
    }
  }
  else {
    // some error occurred
    assert(res != TRI_ERROR_NO_ERROR);

    errorMsg = TRI_errno_string(res);
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
  static const string url = "/_api/replication/state";

  // send request
  LOGGER_REPLICATION("fetching master state from " << url);
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                url,
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
        res = handleStateResponse(json, errorMsg);

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
/// @brief get master inventory
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getMasterInventory (string& errorMsg) {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  map<string, string> headers;
  static const string url = "/_api/replication/inventory";

  // send request
  LOGGER_REPLICATION("fetching master inventory from " << url);
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                url,
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
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }


  const string baseUrl = "/_api/replication/dump"  
                         "?collection=" + StringUtils::itoa(trxCollection->_cid) + 
                         "&chunkSize=" + StringUtils::itoa(getChunkSize());

  map<string, string> headers;

  TRI_voc_tick_t fromTick    = 0;
  uint64_t       markerCount = 0;

  while (true) {
    const string url = baseUrl + 
                       "&from=" + StringUtils::itoa(fromTick) + 
                       "&to=" + StringUtils::itoa(maxTick);

    // send request
    LOGGER_REPLICATION("fetching master collection dump from " << url);
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

int ReplicationFetcher::handleCollectionInitial (TRI_json_t const* parameters,
                                                 TRI_json_t const* indexes,
                                                 string& errorMsg,
                                                 setup_phase_e phase) {
  
  TRI_json_t const* masterName = JsonHelper::getArrayElement(parameters, "name");

  if (! JsonHelper::isString(masterName)) {
    errorMsg = "collection name is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  if (TRI_IsSystemCollectionName(masterName->_value._string.data)) {
    // we will not care about system collections
    return TRI_ERROR_NO_ERROR;
  }
  
  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {  
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  TRI_json_t const* masterId = JsonHelper::getArrayElement(parameters, "cid");

  if (! JsonHelper::isString(masterId)) {
    errorMsg = "collection id is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  TRI_voc_cid_t cid = StringUtils::uint64(masterId->_value._string.data, masterId->_value._string.length - 1);
  

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is invalid)
    return TRI_ERROR_NO_ERROR;
  }
  
  // drop collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP) { 
    // first look up the collection by the cid
    TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

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
  
  // re-create collections locally
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_CREATE) {
    TRI_vocbase_col_t* col = 0;

    int res = createCollection(parameters, &col);

    if (res != TRI_ERROR_NO_ERROR) {
      LOGGER_ERROR("unable to create collection " << cid << ": " << TRI_errno_string(res));

      return res;
    }
  
    return TRI_ERROR_NO_ERROR;
  }
  
  // sync collection data
  // -------------------------------------------------------------------------------------

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
      res = handleCollectionDump(trxCollection, _masterInfo._state._lastLogTick, errorMsg);
    }


    if (res == TRI_ERROR_NO_ERROR) {
      // now create indexes
      const size_t n = indexes->_value._objects._length;

      if (n > 0) {
        LOGGER_INFO("creating indexes for collection '" << masterName->_value._string.data << "', id " << cid);

        for (size_t i = 0; i < n; ++i) {
          TRI_json_t const* idxDef = (TRI_json_t const*) TRI_AtVector(&indexes->_value._objects, i);
          TRI_index_t* idx = 0;
 
          // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}
  
          res = TRI_FromJsonIndexDocumentCollection((TRI_document_collection_t*) trxCollection->_collection->_collection, idxDef, &idx);

          if (res != TRI_ERROR_NO_ERROR) {
            errorMsg = "could not create index: " + string(TRI_errno_string(res));
            break;
          }
          else {
            assert(idx != 0);

            res = TRI_SaveIndex((TRI_primary_collection_t*) trxCollection->_collection->_collection, idx);

            if (res != TRI_ERROR_NO_ERROR) {
              errorMsg = "could not save index: " + string(TRI_errno_string(res));
              break;
            }
          }
        }
      }
    }
    
    if (res == TRI_ERROR_NO_ERROR) {
      TRI_CommitTransaction(trx, TRI_TRANSACTION_TOP_LEVEL);
    }
      
    TRI_FreeTransaction(trx);

    return res;
  }

  
  // we won't get here
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

  if (! JsonHelper::isArray(state)) {
    errorMsg = "state section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."firstLogTick"
  TRI_json_t const* tick = JsonHelper::getArrayElement(state, "firstLogTick");

  if (! JsonHelper::isString(tick)) {
    errorMsg = "firstLogTick is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  const TRI_voc_tick_t firstTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  // state."lastLogTick"
  tick = JsonHelper::getArrayElement(state, "lastLogTick");

  if (! JsonHelper::isString(tick)) {
    errorMsg = "lastLogTick is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  const TRI_voc_tick_t lastTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  // state."running"
  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getArrayElement(json, "server");

  if (! JsonHelper::isArray(server)) {
    errorMsg = "server section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."version"
  TRI_json_t const* version = JsonHelper::getArrayElement(server, "version");

  if (! JsonHelper::isString(version)) {
    errorMsg = "server version is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  // server."serverId"
  TRI_json_t const* serverId = JsonHelper::getArrayElement(server, "serverId");

  if (! JsonHelper::isString(serverId)) {
    errorMsg = "server id is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // validate all values we got
  const TRI_server_id_t masterId = StringUtils::uint64(serverId->_value._string.data, serverId->_value._string.length);

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

  _masterInfo._majorVersion        = major;
  _masterInfo._minorVersion        = minor;
  _masterInfo._serverId            = masterId;
  _masterInfo._state._firstLogTick = firstTick;
  _masterInfo._state._lastLogTick  = lastTick;
  _masterInfo._state._active       = running;

  TRI_LogMasterInfoReplication(&_masterInfo, "connected to");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::handleInventoryResponse (TRI_json_t const* json, 
                                                 string& errorMsg) {
  TRI_json_t* collections = JsonHelper::getArrayElement(json, "collections");

  if (! JsonHelper::isList(collections)) {
    errorMsg = "collections section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const size_t n = collections->_value._objects._length;

  if (n > 1) {
    // sort by collection type (vertices before edges), then name
    qsort(collections->_value._objects._buffer, n, sizeof(TRI_json_t), &ReplicationFetcher::sortCollections);
  }

  int res;
  
  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------

  // iterate over all collections from the master...
  res = iterateCollections(collections, errorMsg, PHASE_VALIDATE);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 2: drop collections locally if they are also present on the master (clean up)
  // ----------------------------------------------------------------------------------

  res = iterateCollections(collections, errorMsg, PHASE_DROP);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 3: re-create empty collections locally
  // ----------------------------------------------------------------------------------

  if (n > 0) {
    // we'll sleep for a while to allow the collections to be dropped (asynchronously)
    // TODO: find a safer mechanism for waiting until we can beginning creating collections
    sleep(5);
  }
  
  res = iterateCollections(collections, errorMsg, PHASE_CREATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 4: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------
  
  res = iterateCollections(collections, errorMsg, PHASE_DATA);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  _applyState._lastInitialTick = _masterInfo._state._lastLogTick;   
  res = TRI_SaveApplyStateReplication(_vocbase, &_applyState, true);

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "could not save replication state information";
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all collections from a list and apply an action
////////////////////////////////////////////////////////////////////////////////
  
int ReplicationFetcher::iterateCollections (TRI_json_t const* collections,
                                            string& errorMsg,
                                            setup_phase_e phase) {
  const size_t n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);

    if (! JsonHelper::isArray(collection)) {
      errorMsg = "collection declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_json_t const* parameters = JsonHelper::getArrayElement(collection, "parameters");

    if (! JsonHelper::isArray(parameters)) {
      errorMsg = "collection parameters declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_json_t const* indexes = JsonHelper::getArrayElement(collection, "indexes");

    if (! JsonHelper::isList(indexes)) {
      errorMsg = "collection indexes declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res = handleCollectionInitial(parameters, indexes, errorMsg, phase);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // all ok
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the continuous synchronisation
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::runContinuous (string& errorMsg) {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  const string baseUrl = "/_api/replication/follow"  
                         "?chunkSize=" + StringUtils::itoa(getChunkSize());

  map<string, string> headers;

  TRI_voc_tick_t fromTick = _applyState._lastInitialTick;

  while (true) {
    const string url = baseUrl + "&from=" + StringUtils::itoa(fromTick); 

    // send request
    LOGGER_REPLICATION("fetching master log from " << url);
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
    bool active    = false;

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

    header = response->getHeaderField(TRI_REPLICATION_HEADER_ACTIVE, found);
    if (found) {
      active = StringUtils::boolean(header);
    }
   
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

    if (res == TRI_ERROR_NO_ERROR) {
      res = applyLog(response, errorMsg);
    }

    delete response;

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
      // nothing to do. sleep before we poll again
      if (active) {
        sleep(1);
      }
      else {
        sleep(10);
      }
    }
  }
  
  assert(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get chunk size for a transfer
////////////////////////////////////////////////////////////////////////////////

uint64_t ReplicationFetcher::getChunkSize () const {
  static const uint64_t chunkSize = 4 * 1024 * 1024; 

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
