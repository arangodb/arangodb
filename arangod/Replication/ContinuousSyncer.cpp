////////////////////////////////////////////////////////////////////////////////
/// @brief replication continuous data synchroniser
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

#include "ContinuousSyncer.h"

#include "BasicsC/json.h"
#include "Basics/JsonHelper.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace triagens::httpclient;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write-lock the status
////////////////////////////////////////////////////////////////////////////////

#define WRITE_LOCK_STATUS(applier) \
  while (! TRI_TryWriteLockReadWriteLock(&(applier->_statusLock))) { \
    usleep(1000); \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief write-unlock the status
////////////////////////////////////////////////////////////////////////////////

#define WRITE_UNLOCK_STATUS(applier) \
  TRI_WriteUnlockReadWriteLock(&(applier->_statusLock))

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ContinuousSyncer::ContinuousSyncer (TRI_vocbase_t* vocbase,
                                    TRI_replication_applier_configuration_t const* configuration,
                                    TRI_voc_tick_t initialTick,
                                    bool useTick) :
  Syncer(vocbase, configuration),
  _applier(vocbase->_replicationApplier),
  _transactionState(),
  _chunkSize(),
  _initialTick(initialTick),
  _useTick(useTick) {
  
  _transactionState._trx         = 0;
  _transactionState._externalTid = 0;

  uint64_t c = configuration->_chunkSize;
  if (c == 0) {
    c = (uint64_t) 256 * 1024; // 256 kb
  }
  
  assert(c > 0);

  _chunkSize = StringUtils::itoa(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ContinuousSyncer::~ContinuousSyncer () {
  abortOngoingTransaction();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs continuous synchronisation
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::run () {
  if (_client == 0 || _connection == 0 || _endpoint == 0) {
    return TRI_ERROR_INTERNAL;
  }

  string errorMsg;

  int res = TRI_ERROR_NO_ERROR;
  uint64_t connectRetries = 0;
     
  // reset failed connects 
  WRITE_LOCK_STATUS(_applier);
  _applier->_state._failedConnects = 0;
  WRITE_UNLOCK_STATUS(_applier);

  while (_vocbase->_state < 2) {
    setProgress("fetching master state information");
    res = getMasterState(errorMsg);

    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE ||
        res == TRI_ERROR_REPLICATION_MASTER_ERROR) {
      // master error. try again after a sleep period
      connectRetries++;
      
      WRITE_LOCK_STATUS(_applier);
      _applier->_state._failedConnects = connectRetries;
      _applier->_state._totalRequests++;
      _applier->_state._totalFailedConnects++;
      WRITE_UNLOCK_STATUS(_applier);

      if (connectRetries <= _configuration._maxConnectRetries) {
        // check if we are aborted externally 
        if (TRI_WaitReplicationApplier(_applier, 10 * 1000 * 1000)) {
          continue;
        }

        // somebody stopped the applier
        res = TRI_ERROR_REPLICATION_APPLIER_STOPPED;
      }
    }

    // we either got a connection or an error
    break;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    WRITE_LOCK_STATUS(_applier);
    res = getLocalState(errorMsg);

    _applier->_state._failedConnects = 0;
    _applier->_state._totalRequests++; 
    WRITE_UNLOCK_STATUS(_applier);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // stop ourselves
    TRI_StopReplicationApplier(_applier, false);

    return TRI_SetErrorReplicationApplier(_applier, res, errorMsg.c_str());
  }

  if (res == TRI_ERROR_NO_ERROR) {
    res = runContinuousSync(errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_SetErrorReplicationApplier(_applier, res, errorMsg.c_str());

    // stop ourselves
    TRI_StopReplicationApplier(_applier, false);

    return res;
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
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set the applier progress
////////////////////////////////////////////////////////////////////////////////
  
void ContinuousSyncer::setProgress (char const* msg) {
  TRI_SetProgressReplicationApplier(_applier, msg, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save the current applier state
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::saveApplierState () {
  LOG_TRACE("saving replication applier state. last applied continuous tick: %llu",
            (unsigned long long) _applier->_state._lastAppliedContinuousTick);

  int res = TRI_SaveStateReplicationApplier(_vocbase, &_applier->_state, false);
        
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("unable to save replication applier state: %s",
                TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get local replication apply state
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::getLocalState (string& errorMsg) { 
  int res;

  uint64_t oldTotalRequests       = _applier->_state._totalRequests;
  uint64_t oldTotalFailedConnects = _applier->_state._totalFailedConnects;

  res = TRI_LoadStateReplicationApplier(_vocbase, &_applier->_state);
  _applier->_state._active              = true;
  _applier->_state._totalRequests       = oldTotalRequests;
  _applier->_state._totalFailedConnects = oldTotalFailedConnects;

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // no state file found, so this is the initialisation
    _applier->_state._serverId = _masterInfo._serverId;

    res = TRI_SaveStateReplicationApplier(_vocbase, &_applier->_state, true);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "could not save replication state information";
    }
  }
  else if (res == TRI_ERROR_NO_ERROR) {
    if (_masterInfo._serverId != _applier->_state._serverId &&
        _applier->_state._serverId != 0) {
      res = TRI_ERROR_REPLICATION_MASTER_CHANGE;
      errorMsg = "encountered wrong master id in replication state file. " 
                 "found: " + StringUtils::itoa(_masterInfo._serverId) + ", " 
                 "expected: " + StringUtils::itoa(_applier->_state._serverId);
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
/// @brief abort any ongoing transaction
////////////////////////////////////////////////////////////////////////////////

void ContinuousSyncer::abortOngoingTransaction () {
  if (_transactionState._trx != 0) {
    LOG_TRACE("aborting replication transaction %llu",
              (unsigned long long) _transactionState._externalTid); 

    TRI_FreeTransaction(_transactionState._trx);
    _transactionState._trx = 0;
    _transactionState._externalTid = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a transaction for a single operation
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* ContinuousSyncer::createSingleOperationTransaction (TRI_voc_cid_t cid,
                                                                       int* result) {
  TRI_transaction_t* trx = TRI_CreateTransaction(_vocbase,
                                                 _masterInfo._serverId,
                                                 false, 
                                                 0.0, 
                                                 false);

  if (trx == 0) {
    *result = TRI_ERROR_OUT_OF_MEMORY;

    return 0;
  }

  int res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);
    *result = res;

    return 0;
  }
  
  res = TRI_BeginTransaction(trx, getHint(1), TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);
    *result = res;

    return 0;
  }

  *result = TRI_ERROR_NO_ERROR;

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::processDocument (TRI_replication_operation_e type,
                                       TRI_json_t const* json,
                                       bool& updateTick, 
                                       string& errorMsg) {
  updateTick = false;

  // extract "cname"
  const string cname = JsonHelper::getStringValue(json, "cname", "");
  TRI_voc_cid_t cid = 0;

  if (! cname.empty()) {
    TRI_vocbase_col_t const* col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());
 
    if (col != 0) {
      cid = col->_cid;
    }
  }

  if (cid == 0) {
    cid = getCid(json);
  }

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }


  // extract "key"
  TRI_json_t const* keyJson = JsonHelper::getArrayElement(json, "key");

  if (! JsonHelper::isString(keyJson)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // extract "rev"
  TRI_voc_rid_t rid;

  const string ridString = JsonHelper::getStringValue(json, "rev", "");
  if (ridString.empty()) {
    rid = 0;
  }
  else {
    rid = StringUtils::uint64(ridString.c_str(), ridString.size());
  }
  
  // extract "data"
  TRI_json_t const* doc = JsonHelper::getArrayElement(json, "data");

  // extract "tid"
  const string id = JsonHelper::getStringValue(json, "tid", "");
  TRI_voc_tid_t tid;

  if (id.empty()) {
    // standalone operation
    tid = 0;
  }
  else {
    // operation is part of a transaction
    tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());
  }
    
  if (tid != _transactionState._externalTid) {
    // unexpected transaction id
    abortOngoingTransaction();
    
    if (tid > 0) {
      // transactional operation but no transaction for it
      return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
    }

    // continue and apply standalone operations
  }


  if (_transactionState._trx != 0) {
    // transactional operation
    assert(tid > 0);

    TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(_transactionState._trx, cid, TRI_TRANSACTION_WRITE);
  
    if (trxCollection == 0) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    int res = applyCollectionDumpMarker(trxCollection, 
                                        type, 
                                        (const TRI_voc_key_t) keyJson->_value._string.data, 
                                        rid,
                                        doc, 
                                        errorMsg);

    return res;
  }

  else {
    // standalone operation
    assert(tid == 0);

    // update the apply tick for all standalone operations
    updateTick = true;

    int res;
    TRI_transaction_t* trx = createSingleOperationTransaction(cid, &res);
  
    if (trx == 0) {
      errorMsg = "unable to create replication transaction: " + string(TRI_errno_string(res));

      return res;
    }
    
    TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE);
    
    if (trxCollection == 0) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
    
    res = applyCollectionDumpMarker(trxCollection, 
                                    type, 
                                    (const TRI_voc_key_t) keyJson->_value._string.data, 
                                    rid,
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

int ContinuousSyncer::startTransaction (TRI_json_t const* json) {
  // {"type":2200,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}

  abortOngoingTransaction();

  const string id = JsonHelper::getStringValue(json, "tid", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  TRI_voc_tid_t tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());

  TRI_json_t const* collections = JsonHelper::getArrayElement(json, "collections");

  if (! JsonHelper::isList(collections)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  LOG_TRACE("starting replication transaction %llu", (unsigned long long) tid); 
  TRI_transaction_t* trx = TRI_CreateTransaction(_vocbase, 
                                                 _masterInfo._serverId,
                                                 false, 
                                                 0.0, 
                                                 false);

  if (trx == 0) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res;
  uint64_t totalOperations = 0;

  const size_t n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = (TRI_json_t const*) TRI_AtVector(&collections->_value._objects, i);

    if (! JsonHelper::isArray(collection)) {
      TRI_FreeTransaction(trx);

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    uint64_t numOperations = JsonHelper::getNumericValue<uint64_t>(collection, "operations", 0);

    if (numOperations > 0) {
      // extract collection name
      const string cname = JsonHelper::getStringValue(collection, "name", "");
      TRI_voc_cid_t cid = 0;

      if (! cname.empty()) {
        TRI_vocbase_col_t* col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());

        if (col != 0) {
          cid = col->_cid;
        }
      }

      if (cid == 0) {
        cid = getCid(collection);
      }

      if (cid == 0) {
        TRI_FreeTransaction(trx);

        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_FreeTransaction(trx);

        return res;
      }

      totalOperations += numOperations;
    }
  }
    
  res = TRI_BeginTransaction(trx, getHint((const size_t) totalOperations), TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);

    return res;
  }

  _transactionState._trx = trx;
  _transactionState._externalTid = tid;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commits a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ContinuousSyncer::commitTransaction (TRI_json_t const* json) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  const string id = JsonHelper::getStringValue(json, "tid", "");
  
  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  const TRI_voc_tid_t tid = (TRI_voc_tid_t) StringUtils::uint64(id.c_str(), id.size());

  if (_transactionState._trx == 0) {
    // invalid state, no transaction was started. 
    return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION; 
  }

  if (_transactionState._externalTid != tid) {
    // unexpected transaction id. 
    abortOngoingTransaction();

    return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION; 
  }
  
  LOG_TRACE("committing replication transaction %llu", (unsigned long long) tid); 

  int res = TRI_CommitTransaction(_transactionState._trx, TRI_TRANSACTION_TOP_LEVEL);

  TRI_FreeTransaction(_transactionState._trx);
  _transactionState._trx = 0;
  _transactionState._externalTid = 0;
    
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int ContinuousSyncer::renameCollection (TRI_json_t const* json) {
  TRI_json_t const* collectionJson = TRI_LookupArrayJson(json, "collection");
  const string name = JsonHelper::getStringValue(collectionJson, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const string cname = JsonHelper::getStringValue(json, "cname", "");
  TRI_vocbase_col_t* col = 0;

  if (! cname.empty()) {
    col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());
  }

  if (col == 0) {
    TRI_voc_cid_t cid = getCid(json);
    col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
  }

  if (col == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  return TRI_RenameCollectionVocBase(_vocbase, 
                                     col, 
                                     name.c_str(), 
                                     true, 
                                     _masterInfo._serverId);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a single marker from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::applyLogMarker (TRI_json_t const* json,
                                      bool& updateTick,
                                      string& errorMsg) {

  static const string invalidMsg = "received invalid JSON data";

  updateTick = false;

  // check data
  if (! JsonHelper::isArray(json)) {
    errorMsg = invalidMsg;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // fetch marker "type"
  int typeValue = JsonHelper::getNumericValue<int>(json, "type", 0);
 
  // fetch "tick"
  const string tick = JsonHelper::getStringValue(json, "tick", "");

  if (! tick.empty()) {
    TRI_voc_tick_t newTick = (TRI_voc_tick_t) StringUtils::uint64(tick.c_str(), tick.size());

    WRITE_LOCK_STATUS(_applier);
    if (newTick > _applier->_state._lastProcessedContinuousTick) {
      _applier->_state._lastProcessedContinuousTick = newTick;
    }
    else {
      LOG_WARNING("replication marker tick value %llu is lower than last processed tick value %llu",
                  (unsigned long long) newTick,
                  (unsigned long long) _applier->_state._lastProcessedContinuousTick);
    }
    WRITE_UNLOCK_STATUS(_applier);
  }

  // handle marker type   
  TRI_replication_operation_e type = (TRI_replication_operation_e) typeValue;

  if (type == MARKER_DOCUMENT || type == MARKER_EDGE || type == MARKER_REMOVE) {
    return processDocument(type, json, updateTick, errorMsg);
  }

  else if (type == TRI_TRANSACTION_START) {
    updateTick = false;

    return startTransaction(json);
  }
  
  else if (type == TRI_TRANSACTION_COMMIT) {
    updateTick = true;

    return commitTransaction(json);
  }

  else if (type == COLLECTION_CREATE) {
    TRI_json_t const* collectionJson = TRI_LookupArrayJson(json, "collection");
    updateTick = true;

    return createCollection(collectionJson, 0);
  }
  
  else if (type == COLLECTION_DROP) {
    updateTick = true;

    return dropCollection(json, false);
  }
  
  else if (type == COLLECTION_RENAME) {
    updateTick = true;

    return renameCollection(json);
  }
  
  else if (type == INDEX_CREATE) {
    updateTick = true;

    return createIndex(json);
  }
  
  else if (type == INDEX_DROP) {
    updateTick = true;

    return dropIndex(json); 
  }
  
  else if (type == REPLICATION_STOP) {
    abortOngoingTransaction();
    updateTick = true;

    return TRI_ERROR_NO_ERROR;
  }
  
  else if (type == REPLICATION_START) {
    abortOngoingTransaction();
    updateTick = true;

    return TRI_ERROR_NO_ERROR;
  }
  
  else {
    errorMsg = "unexpected marker type " + StringUtils::itoa(type);
    updateTick = true;

    return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::applyLog (SimpleHttpResult* response,
                                string& errorMsg,
                                uint64_t& processedMarkers,
                                uint64_t& ignoreCount) {
  
  std::stringstream& data = response->getBody();

  while (true) {
    string line;
    
    std::getline(data, line, '\n');

    if (line.size() < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }

    processedMarkers++;
      
    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, line.c_str());

    bool updateTick; 
    int res = applyLogMarker(json, updateTick, errorMsg);

    if (json != 0) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }

    if (res == TRI_ERROR_NO_ERROR) {
      // apply ok
    }
    else {
      // apply error

      if (errorMsg.empty()) {
        // don't overwrite previous error message
        errorMsg = TRI_errno_string(res);
      }

      if (ignoreCount == 0) {
        if (line.size() > 128) {
          errorMsg += ", offending marker: " + line.substr(128) + "...";
        }
        else {
          errorMsg += ", offending marker: " + line;;
        }

        return res;
      }
      else {
        ignoreCount--;
        LOG_WARNING("ignoring replication error for database '%s': %s",
                    _applier->_databaseName,
                    errorMsg.c_str()); 
        errorMsg = "";
      }
    }

    if (updateTick) {
      // update tick value
      WRITE_LOCK_STATUS(_applier);
      if (_applier->_state._lastProcessedContinuousTick > _applier->_state._lastAppliedContinuousTick) {
        _applier->_state._lastAppliedContinuousTick = _applier->_state._lastProcessedContinuousTick;
      }
      WRITE_UNLOCK_STATUS(_applier);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform a continuous sync with the master
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::runContinuousSync (string& errorMsg) {
  uint64_t connectRetries = 0;
  uint64_t inactiveCycles = 0;
  int res                 = TRI_ERROR_INTERNAL;

  // get start tick
  // ---------------------------------------
  TRI_voc_tick_t fromTick = 0;
    
  WRITE_LOCK_STATUS(_applier);

  if (_useTick) {
    // use user-defined tick
    fromTick = _initialTick;
    _applier->_state._lastAppliedContinuousTick = 0;
    _applier->_state._lastProcessedContinuousTick = 0;
    saveApplierState();
  }
  else {
    // if we already transferred some data, we'll use the last applied tick
    if (_applier->_state._lastAppliedContinuousTick > fromTick) {
      fromTick = _applier->_state._lastAppliedContinuousTick;
    }
  }

  WRITE_UNLOCK_STATUS(_applier);

  if (fromTick == 0) {
    return TRI_ERROR_REPLICATION_NO_START_TICK; 
  }

  // run in a loop. the loop is terminated when the applier is stopped or an
  // error occurs
  while (1) {
    bool worked;
    bool masterActive = false;

    // fromTick is passed by reference!
    res = followMasterLog(errorMsg, fromTick, _configuration._ignoreErrors, worked, masterActive);
    
    uint64_t sleepTime;
  
    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE ||
        res == TRI_ERROR_REPLICATION_MASTER_ERROR) {
      // master error. try again after a sleep period
      sleepTime = 30 * 1000 * 1000;
      connectRetries++;
      
      WRITE_LOCK_STATUS(_applier);
      _applier->_state._failedConnects = connectRetries;
      _applier->_state._totalRequests++;
      _applier->_state._totalFailedConnects++;
      WRITE_UNLOCK_STATUS(_applier);

      if (connectRetries > _configuration._maxConnectRetries) {
        // halt
        return res;
      }
    }
    else {
      connectRetries = 0;
      
      WRITE_LOCK_STATUS(_applier);
      _applier->_state._failedConnects = connectRetries;
      _applier->_state._totalRequests++;
      WRITE_UNLOCK_STATUS(_applier);

      if (res != TRI_ERROR_NO_ERROR) {
        // some other error we will not ignore
        return res;
      }
      else {
        // no error
        if (worked) {
          // we have done something, so we won't sleep (but check for cancellation)
          inactiveCycles = 0;
          sleepTime      = 0;
        }
        else {
          if (masterActive) {
            sleepTime = 500 * 1000;
          }
          else {
            sleepTime = 5 * 1000 * 1000;
          }

          if (_configuration._adaptivePolling) {
            inactiveCycles++;
            if (inactiveCycles > 60) {
              sleepTime *= 5;
            }
            else if (inactiveCycles > 30) {
              sleepTime *= 3;
            }
            if (inactiveCycles > 15) {
              sleepTime *= 2;
            }
          }
        }
      }
    }

    LOG_TRACE("master active: %d, worked: %d, sleepTime: %llu",
              (int) masterActive,
              (int) worked,
              (unsigned long long) sleepTime);

    // this will make the applier thread sleep if there is nothing to do, 
    // but will also check for cancellation
    if (! TRI_WaitReplicationApplier(_applier, sleepTime)) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the continuous synchronisation
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::followMasterLog (string& errorMsg,
                                       TRI_voc_tick_t& fromTick, 
                                       uint64_t& ignoreCount,
                                       bool& worked,
                                       bool& masterActive) {
  const string baseUrl = BaseUrl + 
                         "/logger-follow?chunkSize=" + _chunkSize;

  map<string, string> headers;
  worked = false;

  const string tickString = StringUtils::itoa(fromTick); 
  const string url = baseUrl + 
                     "&from=" + tickString +
                     "&serverId=" + _localServerIdString;
  
  LOG_TRACE("running continuous replication request with tick %llu, url %s",
            (unsigned long long) fromTick,
            url.c_str());

  // send request
  const string progress = "fetching master log from offset " + tickString;
  setProgress(progress.c_str());
  
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                url,
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
               ": " + _client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

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
  TRI_voc_tick_t tick;

  bool found;
  string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);

  if (found) {
    checkMore = StringUtils::boolean(header);
    res = TRI_ERROR_NO_ERROR;
  
    header = response->getHeaderField(TRI_REPLICATION_HEADER_ACTIVE, found);
    if (found) {
      active = StringUtils::boolean(header);
    }
  
    header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
    if (found) {
      tick = StringUtils::uint64(header);

      if (tick > fromTick) {
        fromTick = tick;
        worked = true;
      }
      else {
        // we got the same tick again, this indicates we're at the end
        checkMore = false;
      }

      header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);
      if (found) {
        tick = StringUtils::uint64(header);

        WRITE_LOCK_STATUS(_applier);
        _applier->_state._lastAvailableContinuousTick = tick;
        WRITE_UNLOCK_STATUS(_applier);
      }
    }
  }

  if (! found) {
    res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
                ": required header is missing";
  }


  if (res == TRI_ERROR_NO_ERROR) {
    WRITE_LOCK_STATUS(_applier);
    TRI_voc_tick_t lastAppliedTick = _applier->_state._lastAppliedContinuousTick;
    WRITE_UNLOCK_STATUS(_applier);

    uint64_t processedMarkers = 0;
    res = applyLog(response, errorMsg, processedMarkers, ignoreCount);

    if (processedMarkers > 0) {
      worked = true;

      WRITE_LOCK_STATUS(_applier);
      _applier->_state._totalEvents += processedMarkers;

      if (_applier->_state._lastAppliedContinuousTick != lastAppliedTick) {
        saveApplierState();
      }
      WRITE_UNLOCK_STATUS(_applier);
    }
  }

  delete response;

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  masterActive = active;

  if (! worked) { 
    if (checkMore) {
      worked = true;
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
