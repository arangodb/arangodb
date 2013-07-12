////////////////////////////////////////////////////////////////////////////////
/// @brief replication logger
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-logger.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"


#ifdef TRI_ENABLE_REPLICATION

// -----------------------------------------------------------------------------
// --SECTION--                                                REPLICATION LOGGER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut function
////////////////////////////////////////////////////////////////////////////////

#define FAIL_IFNOT(func, buffer, val)                                     \
  if (func(buffer, val) != TRI_ERROR_NO_ERROR) {                          \
    return false;                                                         \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a string-buffer function name
////////////////////////////////////////////////////////////////////////////////

#define APPEND_FUNC(name) TRI_ ## name ## StringBuffer

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_CHAR(buffer, c)      FAIL_IFNOT(APPEND_FUNC(AppendChar), buffer, c)

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_STRING(buffer, str)  FAIL_IFNOT(APPEND_FUNC(AppendString), buffer, str)

////////////////////////////////////////////////////////////////////////////////
/// @brief append uint64 to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_UINT64(buffer, val)  FAIL_IFNOT(APPEND_FUNC(AppendUInt64), buffer, val)

////////////////////////////////////////////////////////////////////////////////
/// @brief append json to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_JSON(buffer, json)   FAIL_IFNOT(TRI_StringifyJson, buffer, json)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief number of pre-allocated string buffers for logging
////////////////////////////////////////////////////////////////////////////////

static size_t NumBuffers = 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief pre-allocated size for each log buffer
////////////////////////////////////////////////////////////////////////////////

static size_t BufferSize = 256;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a document operation
////////////////////////////////////////////////////////////////////////////////

static TRI_replication_operation_e TranslateDocumentOperation (TRI_voc_document_operation_e type,
                                                               TRI_document_collection_t const* document) {
  const bool isEdge = (document->base.base._info._type == TRI_COL_TYPE_EDGE);

  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT || type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    return isEdge ? MARKER_EDGE : MARKER_DOCUMENT;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    return MARKER_REMOVE;
  }

  return REPLICATION_INVALID;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a buffer to write an event in
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* GetBuffer (TRI_replication_logger_t* logger) {
  TRI_string_buffer_t* buffer;
  size_t n;
   
  assert(logger != NULL);
  buffer = NULL;

  // locked section
  // ---------------------------------------
  TRI_LockSpin(&logger->_bufferLock);

  n = logger->_buffers._length;

  if (n > 0) {
    buffer = TRI_RemoveVectorPointer(&logger->_buffers, (size_t) (n - 1));
  }

  TRI_UnlockSpin(&logger->_bufferLock);
  // ---------------------------------------
  // locked section end

  assert(buffer != NULL);

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a buffer to the list of available buffers
////////////////////////////////////////////////////////////////////////////////

static void ReturnBuffer (TRI_replication_logger_t* logger,
                          TRI_string_buffer_t* buffer) {
  assert(logger != NULL);
  assert(buffer != NULL);

  // make the buffer usable again
  if (buffer->_buffer == NULL) {
    TRI_InitSizedStringBuffer(buffer, TRI_CORE_MEM_ZONE, BufferSize);
  }
  else {
    TRI_ResetStringBuffer(buffer);
  }

  // locked section
  // ---------------------------------------
  TRI_LockSpin(&logger->_bufferLock);

  TRI_PushBackVectorPointer(&logger->_buffers, buffer);
  assert(logger->_buffers._length <= NumBuffers);
  
  TRI_UnlockSpin(&logger->_bufferLock);
  // ---------------------------------------
  // locked section end
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a replication event contained in the buffer
/// the function will always free the buffer passed
////////////////////////////////////////////////////////////////////////////////

static int LogEvent (TRI_replication_logger_t* logger,
                     TRI_voc_tid_t tid,
                     bool isStandaloneOperation,
                     TRI_replication_operation_e type,
                     TRI_string_buffer_t* buffer) {
  TRI_primary_collection_t* primary;
  TRI_shaped_json_t* shaped;
  TRI_json_t json;
  TRI_doc_mptr_t mptr;
  size_t len;
  int res;
  bool forceSync;
  bool withTid;

  assert(logger != NULL);
  assert(buffer != NULL);

  len = TRI_LengthStringBuffer(buffer);

  if (len < 1) {
    // buffer is empty
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_NO_ERROR;
  }

  // do we have a transaction id?
  withTid = (tid > 0);

  // this type of operation will be synced. all other operations will not be synced.
  forceSync = (type == REPLICATION_STOP);
 
  // TODO: instead of using JSON here, we could directly use ShapedJson.
  // this will be a performance optimisation
  TRI_InitArray2Json(TRI_CORE_MEM_ZONE, &json, withTid ? 3 : 2);

  // add "type" attribute
  {
    TRI_json_t typeAttribute;
    TRI_InitNumberJson(&typeAttribute, (double) type);

    TRI_Insert4ArrayJson(TRI_CORE_MEM_ZONE, 
                         &json, 
                         "type", 
                         4, // strlen("type")
                         &typeAttribute,
                         true);
  }

  // "tid" attribute
  if (withTid) {
    TRI_json_t tidAttribute;
    TRI_InitStringJson(&tidAttribute, TRI_StringUInt64(tid));

    TRI_Insert4ArrayJson(TRI_CORE_MEM_ZONE, 
                         &json, 
                         "tid", 
                         3, // strlen("tid")
                         &tidAttribute,
                         true);
  }

  // "data" attribute
  {
    TRI_json_t dataAttribute;
    // pass the string-buffer buffer pointer to the JSON
    TRI_InitStringReference2Json(&dataAttribute, TRI_BeginStringBuffer(buffer), TRI_LengthStringBuffer(buffer));

    TRI_Insert4ArrayJson(TRI_CORE_MEM_ZONE, 
                         &json, 
                         "data", 
                         4, // strlen("data")
                         &dataAttribute,
                         true);
  }
  
  primary = logger->_trxCollection->_collection->_collection;
  shaped = TRI_ShapedJsonJson(primary->_shaper, &json);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
  
  ReturnBuffer(logger, buffer);

  if (shaped == NULL) {
    return TRI_ERROR_ARANGO_SHAPER_FAILED;
  }

  res = primary->insert(logger->_trxCollection, 
                        NULL, 
                        0,
                        &mptr, 
                        TRI_DOC_MARKER_KEY_DOCUMENT, 
                        shaped, 
                        NULL, 
                        isStandaloneOperation, 
                        forceSync);

  TRI_FreeShapedJson(primary->_shaper, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  assert(mptr._data != NULL);
  
  // note the last id that we've logged
  TRI_LockSpin(&logger->_idLock);
  logger->_state._lastLogTick = ((TRI_df_marker_t*) mptr._data)->_tick;
  TRI_UnlockSpin(&logger->_idLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a collection context 
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCollection (TRI_string_buffer_t* buffer,
                                 const TRI_voc_cid_t cid) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "\"cid\":\"");
  APPEND_UINT64(buffer, (uint64_t) cid);
  APPEND_CHAR(buffer, '"');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "replication" operation with a tick
////////////////////////////////////////////////////////////////////////////////

static bool StringifyTickReplication (TRI_string_buffer_t* buffer,
                                      TRI_voc_tick_t tick) {
  if (buffer == NULL) {
    return false;
  }

  APPEND_STRING(buffer, "{\"lastTick\":\"");
  APPEND_UINT64(buffer, (uint64_t) tick);
  APPEND_STRING(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateCollection (TRI_string_buffer_t* buffer,
                                       TRI_json_t const* json) {
  APPEND_STRING(buffer, "{\"collection\":");
  APPEND_JSON(buffer, json);
  APPEND_CHAR(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropCollection (TRI_string_buffer_t* buffer,
                                     TRI_voc_cid_t cid) {
  APPEND_CHAR(buffer, '{');

  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  APPEND_CHAR(buffer, '}');

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyRenameCollection (TRI_string_buffer_t* buffer,
                                       TRI_voc_cid_t cid,
                                       char const* name) {
  
  APPEND_CHAR(buffer, '{');

  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  APPEND_STRING(buffer, ",\"collection\":{\"name\":\"");
  // name is user-defined, but does not need escaping as collection names are "safe"
  APPEND_STRING(buffer, name);
  APPEND_STRING(buffer, "\"}}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "create index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyCreateIndex (TRI_string_buffer_t* buffer,
                                  TRI_voc_cid_t cid,
                                  TRI_json_t const* json) {
  APPEND_CHAR(buffer, '{');
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }

  APPEND_STRING(buffer, ",\"index\":");
  APPEND_JSON(buffer, json);
  APPEND_CHAR(buffer, '}'); 

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDropIndex (TRI_string_buffer_t* buffer,
                                TRI_voc_cid_t cid,
                                TRI_idx_iid_t iid) {
  APPEND_CHAR(buffer, '{');
  
  if (! StringifyCollection(buffer, cid)) {
    return false;
  }
  
  APPEND_STRING(buffer, ",\"id\":\"");
  APPEND_UINT64(buffer, (uint64_t) iid);
  APPEND_STRING(buffer, "\"}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyDocumentOperation (TRI_string_buffer_t* buffer,
                                        TRI_document_collection_t* document,
                                        TRI_voc_document_operation_e type,
                                        TRI_df_marker_t const* marker,
                                        TRI_doc_mptr_t const* oldHeader,
                                        bool withCid) {
  TRI_voc_key_t key;
  TRI_voc_rid_t oldRev;
  TRI_voc_rid_t rid;

  if (TRI_ReserveStringBuffer(buffer, 256) != TRI_ERROR_NO_ERROR) {
    return false;
  }
  
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    oldRev = 0;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    oldRev = 0;
    if (oldHeader != NULL) {
      oldRev = oldHeader->_rid;
    }
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    oldRev = 0;
    if (oldHeader != NULL) {
      oldRev = oldHeader->_rid;
    }
  }
  else {
    return false;
  }
  
  APPEND_CHAR(buffer, '{');
  
  if (withCid) {
    if (! StringifyCollection(buffer, document->base.base._info._cid)) {
      return false;
    }
    APPEND_CHAR(buffer, ',');
  }
  
  if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* m = (TRI_doc_deletion_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    rid = m->_rid;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    rid = m->_rid;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    rid = m->_rid;
  }
  else {
    return false;
  }

  APPEND_STRING(buffer, "\"key\":\""); 
  // key is user-defined, but does not need escaping
  APPEND_STRING(buffer, key); 

  if (oldRev > 0) {
    APPEND_STRING(buffer, "\",\"oldRev\":\""); 
    APPEND_UINT64(buffer, (uint64_t) oldRev); 
  }

  // document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    TRI_shaped_json_t shaped;
    
    APPEND_STRING(buffer, "\",\"data\":{");
    
    // common document meta-data
    APPEND_STRING(buffer, "\"" TRI_VOC_ATTRIBUTE_KEY "\":\"");
    APPEND_STRING(buffer, key);
    APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"");
    APPEND_UINT64(buffer, (uint64_t) rid);
    APPEND_CHAR(buffer, '"');

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* e = (TRI_doc_edge_key_marker_t const*) marker;
      TRI_voc_key_t fromKey = ((char*) e) + e->_offsetFromKey;
      TRI_voc_key_t toKey = ((char*) e) + e->_offsetToKey;

      APPEND_STRING(buffer, ",\"" TRI_VOC_ATTRIBUTE_FROM "\":\"");
      APPEND_UINT64(buffer, (uint64_t) e->_fromCid);
      APPEND_CHAR(buffer, '/');
      APPEND_STRING(buffer, fromKey);
      APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_TO "\":\"");
      APPEND_UINT64(buffer, (uint64_t) e->_toCid);
      APPEND_CHAR(buffer, '/');
      APPEND_STRING(buffer, toKey);
      APPEND_CHAR(buffer, '"');
    }

    // the actual document data
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
    TRI_StringifyArrayShapedJson(document->base._shaper, buffer, &shaped, true);

    APPEND_STRING(buffer, "}}");
  }
  else {
    APPEND_STRING(buffer, "\"}");
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify meta data about a transaction operation
////////////////////////////////////////////////////////////////////////////////

static bool StringifyMetaTransaction (TRI_string_buffer_t* buffer,
                                      TRI_transaction_t const* trx) {
  size_t i, n;
  bool printed;

  APPEND_STRING(buffer, "{\"collections\":[");

  printed = false;
  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection;
    TRI_document_collection_t* document;

    trxCollection = TRI_AtVectorPointer(&trx->_collections, i);

    if (trxCollection->_operations == NULL) {
      // no markers available for collection
      continue;
    }
    
    document = (TRI_document_collection_t*) trxCollection->_collection->_collection;
      
    if (printed) {
      APPEND_CHAR(buffer, ',');
    }
    else {
      printed = true;
    }
  
    APPEND_STRING(buffer, "{\"cid\":\"");
    APPEND_UINT64(buffer, (uint64_t) document->base.base._info._cid);
    APPEND_STRING(buffer, "\",\"operations\":");
    APPEND_UINT64(buffer, (uint64_t) trxCollection->_operations->_length);
    APPEND_CHAR(buffer, '}');
  }
  APPEND_STRING(buffer, "]}");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get current state from the replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int GetStateReplicationLogger (TRI_replication_logger_t* logger,
                                      TRI_replication_log_state_t* dst) {
  assert(logger->_state._active);

  TRI_LockSpin(&logger->_idLock);
  memcpy(dst, &logger->_state, sizeof(TRI_replication_log_state_t));
  TRI_UnlockSpin(&logger->_idLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StartReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_transaction_t* trx;
  TRI_vocbase_col_t* collection;
  TRI_vocbase_t* vocbase;
  TRI_transaction_hint_t hint;
  TRI_string_buffer_t* buffer;
  TRI_voc_cid_t cid;
  int res;
  
  if (logger->_state._active) {
    return TRI_ERROR_INTERNAL;
  }

  assert(logger->_trx == NULL);
  assert(logger->_trxCollection == NULL);
  assert(logger->_state._lastLogTick == 0);

  vocbase = logger->_vocbase;
  collection = TRI_LookupCollectionByNameVocBase(vocbase, TRI_COL_NAME_REPLICATION);

  if (collection == NULL) {
    LOG_ERROR("could not open collection '" TRI_COL_NAME_REPLICATION "'");

    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  cid = collection->_cid;

  trx = TRI_CreateTransaction(vocbase->_transactionContext, false, 0.0, false);

  if (trx == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = TRI_AddCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);

    return TRI_ERROR_INTERNAL;
  }

  // the SINGLE_OPERATION hint is actually a hack:
  // the logger does not write just one operation, but it is used to prevent locking the collection
  // for the entire duration of the transaction
  hint = (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION;
  res = TRI_BeginTransaction(trx, hint, TRI_TRANSACTION_TOP_LEVEL);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeTransaction(trx);
    
    return TRI_ERROR_INTERNAL;
  }

  logger->_trxCollection = TRI_GetCollectionTransaction(trx, cid, TRI_TRANSACTION_WRITE);
  logger->_trx = trx;

  assert(logger->_trxCollection != NULL);
  assert(logger->_state._active == false);

  logger->_state._lastLogTick = ((TRI_collection_t*) collection->_collection)->_info._tick; 
  logger->_state._active = true;
  
  LOG_INFO("started replication logger for database '%s', last tick: %llu", 
           logger->_databaseName,
           (unsigned long long) logger->_state._lastLogTick);
  
  buffer = GetBuffer(logger);
  
  if (! StringifyTickReplication(buffer, logger->_state._lastLogTick)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  res = LogEvent(logger, 0, true, REPLICATION_START, buffer); 

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StopReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_string_buffer_t* buffer;
  TRI_voc_tick_t lastTick;
  int res;

  if (! logger->_state._active) {
    return TRI_ERROR_INTERNAL;
  }

  TRI_LockSpin(&logger->_idLock);
  lastTick = logger->_state._lastLogTick;
  TRI_UnlockSpin(&logger->_idLock);

  assert(logger->_trx != NULL);
  assert(logger->_trxCollection != NULL);

  buffer = GetBuffer(logger);
  
  if (! StringifyTickReplication(buffer, lastTick)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, REPLICATION_STOP, buffer); 

  TRI_CommitTransaction(logger->_trx, 0);
  TRI_FreeTransaction(logger->_trx);
  
  LOG_INFO("stopped replication logger for database '%s', last tick: %llu", 
           logger->_databaseName,
           (unsigned long long) lastTick);


  logger->_trx                = NULL;
  logger->_trxCollection      = NULL;
  logger->_state._lastLogTick = 0;
  logger->_state._active      = false;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the state of the _replication collection for a non-running
/// replication logger
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int GetStateInactive (TRI_vocbase_t* vocbase,
                             TRI_replication_log_state_t* dst) {
  TRI_vocbase_col_t* col;
  TRI_primary_collection_t* primary;
 
  col = TRI_UseCollectionByNameVocBase(vocbase, TRI_COL_NAME_REPLICATION);

  if (col == NULL || col->_collection == NULL) {
    LOG_ERROR("could not open collection '" TRI_COL_NAME_REPLICATION "'");

    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  primary = (TRI_primary_collection_t*) col->_collection;

  dst->_active       = false;
  dst->_lastLogTick  = primary->base._info._tick;

  TRI_ReleaseCollectionVocBase(vocbase, col);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free all allocated buffers
////////////////////////////////////////////////////////////////////////////////

static void FreeBuffers (TRI_replication_logger_t* logger) {
  size_t i, n;

  LOG_TRACE("freeing buffers");

  n = logger->_buffers._length;

  for (i = 0; i < n; ++i) {
    TRI_string_buffer_t* buffer = (TRI_string_buffer_t*) TRI_AtVectorPointer(&logger->_buffers, i);

    assert(buffer != NULL);
    TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, buffer);
  }

  TRI_DestroyVectorPointer(&logger->_buffers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise buffers
////////////////////////////////////////////////////////////////////////////////

static int InitBuffers (TRI_replication_logger_t* logger) {
  size_t i;
  int res;

  assert(NumBuffers > 0);
  
  LOG_TRACE("initialising buffers");

  res = TRI_InitVectorPointer2(&logger->_buffers, TRI_CORE_MEM_ZONE, NumBuffers); 

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  for (i = 0; i < NumBuffers; ++i) {
    TRI_string_buffer_t* buffer = TRI_CreateSizedStringBuffer(TRI_CORE_MEM_ZONE, BufferSize);

    if (buffer == NULL) {
      FreeBuffers(logger);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    TRI_PushBackVectorPointer(&logger->_buffers, buffer);
  }

  assert(logger->_buffers._length == NumBuffers);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle logging of a transaction
////////////////////////////////////////////////////////////////////////////////

static int HandleTransaction (TRI_replication_logger_t* logger,
                              TRI_transaction_t const* trx) {
  TRI_string_buffer_t* buffer;
  size_t i, n;
  int res;

  // write "start"
  buffer = GetBuffer(logger);
  
  if (! StringifyMetaTransaction(buffer, trx)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
   
  res = LogEvent(logger, trx->_id, false, TRANSACTION_START, buffer);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // write the individual operations
  n = trx->_collections._length;
  assert(n > 0);

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection;
    TRI_document_collection_t* document;
    size_t j, k;

    trxCollection = TRI_AtVectorPointer(&trx->_collections, i);

    if (trxCollection->_operations == NULL) {
      // no markers available for collection
      continue;
    }
    
    document = (TRI_document_collection_t*) trxCollection->_collection->_collection;
    k = trxCollection->_operations->_length;

    for (j = 0; j < k; ++j) {
      TRI_transaction_operation_t* trxOperation;
      TRI_replication_operation_e type;

      trxOperation = TRI_AtVector(trxCollection->_operations, j);
  
      buffer = GetBuffer(logger);

      if (! StringifyDocumentOperation(buffer, 
                                       document, 
                                       trxOperation->_type, 
                                       trxOperation->_marker, 
                                       trxOperation->_oldHeader, 
                                       true)) {
        ReturnBuffer(logger, buffer);

        return false;
      }

      type = TranslateDocumentOperation(trxOperation->_type, document);

      if (type == REPLICATION_INVALID) {
        ReturnBuffer(logger, buffer);

        return TRI_ERROR_INTERNAL;
      }

      res = LogEvent(logger, trx->_id, false, type, buffer);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }


  // write "commit"  
  buffer = GetBuffer(logger);

  if (! StringifyMetaTransaction(buffer, trx)) {
    ReturnBuffer(logger, buffer);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  res = LogEvent(logger, trx->_id, false, TRANSACTION_COMMIT, buffer);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication logger
////////////////////////////////////////////////////////////////////////////////

TRI_replication_logger_t* TRI_CreateReplicationLogger (TRI_vocbase_t* vocbase) {
  TRI_replication_logger_t* logger;
  int res;

  logger = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_replication_logger_t), false);

  if (logger == NULL) {
    return NULL;
  }
 
  // init string buffers 
  res = InitBuffers(logger);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    TRI_Free(TRI_CORE_MEM_ZONE, logger);

    return NULL;
  }

  TRI_InitReadWriteLock(&logger->_statusLock);
  TRI_InitSpin(&logger->_idLock);
  TRI_InitSpin(&logger->_bufferLock);

  logger->_vocbase             = vocbase;
  logger->_trx                 = NULL;
  logger->_trxCollection       = NULL;
  logger->_state._lastLogTick  = 0;
  logger->_state._active       = false;
  logger->_databaseName        = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, vocbase->_name);

  assert(logger->_databaseName != NULL);

  return logger;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_StopReplicationLogger(logger);

  FreeBuffers(logger);
 
  TRI_FreeString(TRI_CORE_MEM_ZONE, logger->_databaseName);
  TRI_DestroySpin(&logger->_bufferLock);
  TRI_DestroySpin(&logger->_idLock);
  TRI_DestroyReadWriteLock(&logger->_statusLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeReplicationLogger (TRI_replication_logger_t* logger) {
  TRI_DestroyReplicationLogger(logger);
  TRI_Free(TRI_CORE_MEM_ZONE, logger);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StartReplicationLogger (TRI_replication_logger_t* logger) {
  int res;
  
  res = TRI_ERROR_NO_ERROR;

  TRI_WriteLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    res = StartReplicationLogger(logger);
  }

  TRI_WriteUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationLogger (TRI_replication_logger_t* logger) {
  int res;

  res = TRI_ERROR_NO_ERROR;
  
  TRI_WriteLockReadWriteLock(&logger->_statusLock);

  if (logger->_state._active) {
    res = StopReplicationLogger(logger);
  }

  TRI_WriteUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication logger state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationLogger (TRI_replication_logger_t* logger,
                                TRI_replication_log_state_t* state) {
  TRI_vocbase_t* vocbase;
  int res;

  res = TRI_ERROR_NO_ERROR;
  vocbase = logger->_vocbase;

  TRI_WriteLockReadWriteLock(&vocbase->_objectLock);
  
  TRI_WriteLockReadWriteLock(&logger->_statusLock);

  if (logger->_state._active) {
    // use state from logger
    res = GetStateReplicationLogger(logger, state);
  }
  else {
    // read first/last directly from collection
    res = GetStateInactive(logger->_vocbase, state);
  }

  TRI_WriteUnlockReadWriteLock(&logger->_statusLock);
  
  TRI_WriteUnlockReadWriteLock(&vocbase->_objectLock);

  return res;
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of a logger state
////////////////////////////////////////////////////////////////////////////////
  
TRI_json_t* TRI_JsonStateReplicationLogger (TRI_replication_log_state_t const* state) {
  TRI_json_t* json; 
  char* lastString;

  json = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 2);

  // add replication state
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, state->_active));
  
  lastString = TRI_StringUInt64(state->_lastLogTick);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "lastLogTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastString));
  
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              public log functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_LogTransactionReplication (TRI_vocbase_t* vocbase,
                                   TRI_transaction_t const* trx) {
  TRI_replication_logger_t* logger;
  int res;
   
  assert(trx->_replicate);
  assert(trx->_hasOperations);

  res = TRI_ERROR_NO_ERROR;

  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (logger->_state._active) {
    TRI_primary_collection_t* primary;

    primary = logger->_trxCollection->_collection->_collection;

    assert(primary != NULL);

    // set a lock around all individual operations
    // so a transaction is logged as an uninterrupted sequence
    primary->beginWrite(primary);
    res = HandleTransaction(logger, trx);
    primary->endWrite(primary);
  }

  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogCreateCollectionReplication (TRI_vocbase_t* vocbase,
                                        TRI_voc_cid_t cid,
                                        TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;

  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateCollection(buffer, json)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, COLLECTION_CREATE, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDropCollectionReplication (TRI_vocbase_t* vocbase,
                                      TRI_voc_cid_t cid) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;
  
  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyDropCollection(buffer, cid)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, COLLECTION_DROP, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogRenameCollectionReplication (TRI_vocbase_t* vocbase,
                                        TRI_voc_cid_t cid,
                                        char const* name) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;
  
  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyRenameCollection(buffer, cid, name)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, COLLECTION_RENAME, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "change collection properties" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogChangePropertiesCollectionReplication (TRI_vocbase_t* vocbase,
                                                  TRI_voc_cid_t cid,
                                                  TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;
  
  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateCollection(buffer, json)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, COLLECTION_CHANGE, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogCreateIndexReplication (TRI_vocbase_t* vocbase,
                                   TRI_voc_cid_t cid,
                                   TRI_idx_iid_t iid,
                                   TRI_json_t const* json) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;
  
  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyCreateIndex(buffer, cid, json)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, INDEX_CREATE, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDropIndexReplication (TRI_vocbase_t* vocbase,
                                 TRI_voc_cid_t cid,
                                 TRI_idx_iid_t iid) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  int res;
  
  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  buffer = GetBuffer(logger);

  if (! StringifyDropIndex(buffer, cid, iid)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, INDEX_DROP, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDocumentReplication (TRI_vocbase_t* vocbase,
                                TRI_document_collection_t* document,
                                TRI_voc_document_operation_e docType,
                                TRI_df_marker_t const* marker,
                                TRI_doc_mptr_t const* oldHeader) {
  TRI_string_buffer_t* buffer;
  TRI_replication_logger_t* logger;
  TRI_replication_operation_e type;
  int res;

  logger = vocbase->_replicationLogger;
  TRI_ReadLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_NO_ERROR;
  }
  
  type = TranslateDocumentOperation(docType, document);

  if (type == REPLICATION_INVALID) {
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_INTERNAL;
  }

  buffer = GetBuffer(logger);

  if (! StringifyDocumentOperation(buffer, 
                                   document, 
                                   docType, 
                                   marker, 
                                   oldHeader, 
                                   true)) {
    ReturnBuffer(logger, buffer);
    TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res = LogEvent(logger, 0, true, type, buffer);
  TRI_ReadUnlockReadWriteLock(&logger->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
