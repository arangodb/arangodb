////////////////////////////////////////////////////////////////////////////////
/// @brief replication dump functions
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

#include "replication-dump.h"

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
// --SECTION--                                                       REPLICATION
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
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief a datafile descriptor
////////////////////////////////////////////////////////////////////////////////
  
typedef struct {
  TRI_datafile_t* _data;
  TRI_voc_tick_t  _dataMin;
  TRI_voc_tick_t  _dataMax;
  bool            _isJournal;
}
df_entry_t;

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
/// @brief get the datafiles of a collection for a specific tick range
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t GetRangeDatafiles (TRI_primary_collection_t* primary,
                                       TRI_voc_tick_t dataMin,
                                       TRI_voc_tick_t dataMax) {
  TRI_vector_t datafiles;
  size_t i;

  LOG_TRACE("getting datafiles in tick range %llu - %llu", 
            (unsigned long long) dataMin, 
            (unsigned long long) dataMax);

  // determine the datafiles of the collection
  TRI_InitVector(&datafiles, TRI_CORE_MEM_ZONE, sizeof(df_entry_t));

  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(primary);

  for (i = 0; i < primary->base._datafiles._length; ++i) {
    TRI_datafile_t* df = TRI_AtVectorPointer(&primary->base._datafiles, i);

    df_entry_t entry = { 
      df,
      df->_dataMin,
      df->_dataMax,
      false 
    };
  
    LOG_TRACE("checking datafile with tick range %llu - %llu", 
              (unsigned long long) df->_dataMin, 
              (unsigned long long) df->_dataMax);
    
    if (dataMax < df->_dataMin) {
      // datafile is newer than requested range
      continue;
    }

    if (dataMin > df->_dataMax) {
      // datafile is older than requested range
      continue;
    }
     
    TRI_PushBackVector(&datafiles, &entry);
  }

  for (i = 0; i < primary->base._journals._length; ++i) {
    TRI_datafile_t* df = TRI_AtVectorPointer(&primary->base._journals, i);

    df_entry_t entry = { 
      df,
      df->_dataMin,
      df->_dataMax,
      true 
    };
    
    LOG_TRACE("checking journal with tick range %llu - %llu", 
              (unsigned long long) df->_dataMin, 
              (unsigned long long) df->_dataMax);
    
    if (dataMax < df->_dataMin) {
      // datafile is newer than requested range
      continue;
    }

    if (dataMin > df->_dataMax) {
      // datafile is older than requested range
      continue;
    }
     
    TRI_PushBackVector(&datafiles, &entry);
  }

  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

  return datafiles;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a datafile for a collection dump
////////////////////////////////////////////////////////////////////////////////

static bool StringifyMarkerDump (TRI_string_buffer_t* buffer,
                                 TRI_document_collection_t* document,
                                 TRI_df_marker_t const* marker) {
  TRI_replication_operation_e type;
  TRI_voc_key_t key;
  TRI_voc_rid_t rid;

  APPEND_CHAR(buffer, '{');
  
  if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* m = (TRI_doc_deletion_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    type = MARKER_REMOVE;
    rid = m->_rid;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    type = MARKER_DOCUMENT;
    rid = m->_rid;
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
    key = ((char*) m) + m->_offsetKey;
    type = MARKER_EDGE;
    rid = m->_rid;
  }
  else {
    return false;
  }

  APPEND_STRING(buffer, "\"type\":"); 
  APPEND_UINT64(buffer, (uint64_t) type); 
  APPEND_STRING(buffer, ",\"key\":\""); 
  // key is user-defined, but does not need escaping
  APPEND_STRING(buffer, key); 

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

    APPEND_STRING(buffer, "}}\n");
  }
  else {
    APPEND_STRING(buffer, "\"}\n");
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the attributes of a replication log marker (shaped json)
////////////////////////////////////////////////////////////////////////////////

static bool IterateShape (TRI_shaper_t* shaper,
                          TRI_shape_t const* shape,
                          char const* name,
                          char const* data,
                          uint64_t size,
                          void* ptr) {
  bool append   = false;
  bool withName = false;

  if (TRI_EqualString(name, "data")) {
    append   = true;
    withName = false;
  }
  else if (TRI_EqualString(name, "type") ||
           TRI_EqualString(name, "tid")) {
    append = true;
    withName = true;
  }

  if (append) {
    TRI_replication_dump_t* dump;
    TRI_string_buffer_t* buffer;
    int res;
  
    dump   = (TRI_replication_dump_t*) ptr;
    buffer = dump->_buffer;

    // append ,
    res = TRI_AppendCharStringBuffer(buffer, ',');
    
    if (res != TRI_ERROR_NO_ERROR) {
      dump->_failed = true;
      return false;
    }

    if (withName) {
      // append attribute name and value
      res = TRI_AppendCharStringBuffer(buffer, '"');

      if (res != TRI_ERROR_NO_ERROR) {
        dump->_failed = true;
        return false;
      }

      res = TRI_AppendStringStringBuffer(buffer, name);

      if (res != TRI_ERROR_NO_ERROR) {
        dump->_failed = true;
        return false;
      }

      res = TRI_AppendStringStringBuffer(buffer, "\":");

      if (shape->_type == TRI_SHAPE_NUMBER) {
        if (! TRI_StringifyJsonShapeData(shaper, buffer, shape, data, size)) {
          res = TRI_ERROR_OUT_OF_MEMORY;
        }
      }
      else if (shape->_type == TRI_SHAPE_SHORT_STRING ||
               shape->_type == TRI_SHAPE_LONG_STRING) {
        char* value;
        size_t length;

        res = TRI_AppendCharStringBuffer(buffer, '"');
      
        if (res != TRI_ERROR_NO_ERROR) {
          dump->_failed = true;
          return false;
        }

        TRI_StringValueShapedJson(shape, data, &value, &length);

        if (value != NULL && length > 0) {
          res = TRI_AppendString2StringBuffer(dump->_buffer, value, length);

          if (res != TRI_ERROR_NO_ERROR) {
            dump->_failed = true;
            return false;
          }
        }
    
        res = TRI_AppendCharStringBuffer(buffer, '"');
      }
    }
    else {
      // append raw value
      char* value;
      size_t length;

      TRI_StringValueShapedJson(shape, data, &value, &length);

      if (value != NULL && length > 2) {
        res = TRI_AppendString2StringBuffer(dump->_buffer, value + 1, length - 2);
      }
    }


    if (res != TRI_ERROR_NO_ERROR) {
      dump->_failed = true;
      return false;
    }
  }

  // continue iterating
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a datafile for a log dump
////////////////////////////////////////////////////////////////////////////////

static bool StringifyMarkerLog (TRI_replication_dump_t* dump,
                                TRI_document_collection_t* document,
                                TRI_df_marker_t const* marker) {

  TRI_doc_document_key_marker_t const* m = (TRI_doc_document_key_marker_t const*) marker; 
  TRI_shaper_t* shaper;
  TRI_shaped_json_t shaped;
  
  assert(marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT);
  shaper = document->base._shaper;

  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);

  if (shaped._sid != 0) {
    TRI_shape_t const* shape;

    if (shaped._sid != dump->_lastSid || dump->_lastShape == NULL) {
      shape = shaper->lookupShapeId(shaper, shaped._sid);
      dump->_lastSid   = shaped._sid;
      dump->_lastShape = shape;
    }
    else {
      shape            = dump->_lastShape;
    }
  
    APPEND_STRING(dump->_buffer, "{\"tick\":\"");
    APPEND_UINT64(dump->_buffer, (uint64_t) marker->_tick);
    APPEND_CHAR(dump->_buffer, '"');
    TRI_IterateShapeDataArray(shaper, shape, shaped._data.data, &IterateShape, dump); 
    APPEND_STRING(dump->_buffer, "}\n");
  }
  else {
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a transaction id is contained in the list of failed
/// transactions
////////////////////////////////////////////////////////////////////////////////

static bool InFailedList (TRI_vector_t const* list, TRI_voc_tid_t search) {
  size_t n;

  assert(list != NULL);

  n = list->_length;
 
  // decide how to search based on size of list
  if (n == 0) {
    // simple case: list is empty
    return false;
  }

  else if (n < 16) {
    // list is small: use a linear search
    size_t i;

    for (i = 0; i < n; ++i) {
      TRI_voc_tid_t* tid = TRI_AtVector(list, i);

      if (*tid == search) {
        return true;
      }
    }

    return false;
  }

  else {
    // list is somewhat bigger, use a binary search
    size_t l = 0;
    size_t r = (size_t) (n - 1);

    while (true) {
      // determine midpoint
      TRI_voc_tid_t* tid;
      size_t m;

      m = l + ((r - l) / 2);
      tid = TRI_AtVector(list, m);

      if (*tid == search) {
        return true;
      }

      if (*tid > search) {
        if (m == 0) {
          // we must abort because the following subtraction would
          // make the size_t underflow
          return false;
        }
        
        r = m - 1;
      }
      else {
        l = m + 1;
      }

      if (r < l) {
        return false;
      }
    }
  }

  // we should never get here
  assert(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

static int DumpCollection (TRI_replication_dump_t* dump, 
                           TRI_primary_collection_t* primary,
                           TRI_voc_tick_t dataMin,
                           TRI_voc_tick_t dataMax,
                           uint64_t chunkSize) {
  TRI_vector_t datafiles;
  TRI_document_collection_t* document;
  TRI_string_buffer_t* buffer;
  TRI_voc_tick_t lastFoundTick;
  TRI_voc_tid_t lastTid;
  size_t i; 
  int res;
  bool hasMore;
  bool bufferFull;
  bool ignoreMarkers;
    
  LOG_TRACE("dumping collection %llu, tick range %llu - %llu, chunk size %llu", 
            (unsigned long long) primary->base._info._cid,
            (unsigned long long) dataMin,
            (unsigned long long) dataMax,
            (unsigned long long) chunkSize);

  buffer         = dump->_buffer;
  datafiles      = GetRangeDatafiles(primary, dataMin, dataMax);
  document       = (TRI_document_collection_t*) primary;
 
  // setup some iteration state
  lastFoundTick  = 0;
  lastTid        = 0;
  res            = TRI_ERROR_NO_ERROR;
  hasMore        = true;
  bufferFull     = false;
  ignoreMarkers  = false;

  for (i = 0; i < datafiles._length; ++i) {
    df_entry_t* e = (df_entry_t*) TRI_AtVector(&datafiles, i);
    TRI_datafile_t* datafile = e->_data;
    TRI_vector_t* failedList;
    char const* ptr;
    char const* end;

    failedList    = NULL;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    if (e->_isJournal) {
      TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

      if (document->_failedTransactions._length > 0) {
        // there are failed transactions. just reference them
        failedList = &document->_failedTransactions;
      }
    }
    else {
      assert(datafile->_isSealed);

      TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
      
      if (document->_failedTransactions._length > 0) {
        // there are failed transactions. copy the list of ids
        failedList = TRI_CopyVector(TRI_UNKNOWN_MEM_ZONE, &document->_failedTransactions);

        if (failedList == NULL) {
          res = TRI_ERROR_OUT_OF_MEMORY;
        }
      }

      TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
    }
    
    ptr = datafile->_data;

    if (res == TRI_ERROR_NO_ERROR) { 
      // no error so far. start iterating
      end = ptr + datafile->_currentSize;
    }
    else {
      // some error occurred. don't iterate
      end = ptr;
    }

    while (ptr < end) {
      TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
      TRI_voc_tick_t foundTick;
      TRI_voc_tid_t tid;

      if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
        // end of datafile
        break;
      }
      
      ptr += TRI_DF_ALIGN_BLOCK(marker->_size);
          
      if (marker->_type != TRI_DOC_MARKER_KEY_DOCUMENT &&
          marker->_type != TRI_DOC_MARKER_KEY_EDGE &&
          marker->_type != TRI_DOC_MARKER_KEY_DELETION) {
        continue;
      }

      // get the marker's tick and check whether we should include it
      foundTick = marker->_tick;
      
      if (foundTick <= dataMin) {
        // marker too old
        continue;
      }

      if (foundTick > dataMax) {
        // marker too new
        hasMore = false;
        goto NEXT_DF;
      }

      // note the last tick we processed
      lastFoundTick = foundTick;


      // handle aborted/unfinished transactions

      if (failedList == NULL) {
        // there are no failed transactions
        ignoreMarkers = false;
      }
      else {
        // get transaction id of marker
        if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
          tid = ((TRI_doc_deletion_key_marker_t const*) marker)->_tid;
        }
        else {
          tid = ((TRI_doc_document_key_marker_t const*) marker)->_tid;
        }

        // check if marker is from an aborted transaction
        if (tid > 0) {
          if (tid != lastTid) {
            ignoreMarkers = InFailedList(failedList, tid);
          }

          lastTid = tid;
        } 

        if (ignoreMarkers) {
          continue;
        }
      }

      if (! StringifyMarkerDump(buffer, document, marker)) {
        res = TRI_ERROR_INTERNAL;

        goto NEXT_DF;
      }

      if ((uint64_t) TRI_LengthStringBuffer(buffer) > chunkSize) {
        // abort the iteration
        bufferFull = true;

        goto NEXT_DF;
      }
    }

NEXT_DF:
    if (e->_isJournal) {
      // read-unlock the journal
      TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
    }
    else {
      // free our copy of the failed list
      if (failedList != NULL) {
        TRI_FreeVector(TRI_UNKNOWN_MEM_ZONE, failedList);
      }
    }

    if (res != TRI_ERROR_NO_ERROR || ! hasMore || bufferFull) {
      break;
    }
  }
  
  TRI_DestroyVector(&datafiles);

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore       = hasMore;
      dump->_bufferFull    = bufferFull;
    }
    else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore       = false;
      dump->_bufferFull    = false;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

static int DumpLog (TRI_replication_dump_t* dump, 
                    TRI_primary_collection_t* primary,
                    TRI_voc_tick_t dataMin,
                    TRI_voc_tick_t dataMax,
                    uint64_t chunkSize) {
  TRI_vector_t datafiles;
  TRI_document_collection_t* document;
  TRI_string_buffer_t* buffer;
  TRI_voc_tick_t lastFoundTick;
  size_t i; 
  int res;
  bool hasMore;
  bool bufferFull;
    
  LOG_TRACE("dumping collection %llu, tick range %llu - %llu, chunk size %llu", 
            (unsigned long long) primary->base._info._cid,
            (unsigned long long) dataMin,
            (unsigned long long) dataMax,
            (unsigned long long) chunkSize);

  buffer         = dump->_buffer;
  datafiles      = GetRangeDatafiles(primary, dataMin, dataMax);
  document       = (TRI_document_collection_t*) primary;
 
  // setup some iteration state
  lastFoundTick  = 0;
  res            = TRI_ERROR_NO_ERROR;
  hasMore        = true;
  bufferFull     = false;

  for (i = 0; i < datafiles._length; ++i) {
    df_entry_t* e = (df_entry_t*) TRI_AtVector(&datafiles, i);
    TRI_datafile_t* datafile = e->_data;
    char const* ptr;
    char const* end;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    if (e->_isJournal) {
      TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
    }
    else {
      assert(datafile->_isSealed);
    }
    
    ptr = datafile->_data;

    if (res == TRI_ERROR_NO_ERROR) { 
      // no error so far. start iterating
      end = ptr + datafile->_currentSize;
    }
    else {
      // some error occurred. don't iterate
      end = ptr;
    }

    while (ptr < end) {
      TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
      TRI_voc_tick_t foundTick;

      if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
        // end of datafile
        break;
      }
      
      ptr += TRI_DF_ALIGN_BLOCK(marker->_size);
          
      if (marker->_type != TRI_DOC_MARKER_KEY_DOCUMENT) {
        // we're only interested in document markers here
        // the replication collection does not contain any edge markers
        // and deletion markers in the replication collection
        // will not be replicated
        continue;
      }

      // get the marker's tick and check whether we should include it
      foundTick = marker->_tick;
      
      if (foundTick <= dataMin) {
        // marker too old
        continue;
      }

      if (foundTick > dataMax) {
        // marker too new
        hasMore = false;
        goto NEXT_DF;
      }

      // note the last tick we processed
      lastFoundTick = foundTick;

      if (! StringifyMarkerLog(dump, document, marker)) {
        res = TRI_ERROR_INTERNAL;

        goto NEXT_DF;
      }

      if ((uint64_t) TRI_LengthStringBuffer(buffer) > chunkSize) {
        // abort the iteration
        bufferFull = true;

        goto NEXT_DF;
      }
    }

NEXT_DF:
    if (e->_isJournal) {
      // read-unlock the journal
      TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
    }

    if (res != TRI_ERROR_NO_ERROR || ! hasMore || bufferFull) {
      break;
    }
  }
  
  TRI_DestroyVector(&datafiles);

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore       = hasMore;
      dump->_bufferFull    = bufferFull;
    }
    else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore       = false;
      dump->_bufferFull    = false;
    }
  }

  return res;
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
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpCollectionReplication (TRI_replication_dump_t* dump,
                                   TRI_vocbase_col_t* col,
                                   TRI_voc_tick_t dataMin,
                                   TRI_voc_tick_t dataMax,
                                   uint64_t chunkSize) {
  TRI_primary_collection_t* primary;
  TRI_barrier_t* b;
  int res;

  assert(col != NULL);
  assert(col->_collection != NULL);

  primary = (TRI_primary_collection_t*) col->_collection;

  // create a barrier so the underlying collection is not unloaded
  b = TRI_CreateBarrierReplication(&primary->_barrierList);

  if (b == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // block compaction
  TRI_ReadLockReadWriteLock(&primary->_compactionLock);

  res = DumpCollection(dump, primary, dataMin, dataMax, chunkSize);
  
  TRI_ReadUnlockReadWriteLock(&primary->_compactionLock);

  TRI_FreeBarrier(b);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpLogReplication (TRI_vocbase_t* vocbase,
                            TRI_replication_dump_t* dump,
                            TRI_voc_tick_t dataMin,
                            TRI_voc_tick_t dataMax,
                            uint64_t chunkSize) {
  TRI_vocbase_col_t* col;
  TRI_primary_collection_t* primary;
  TRI_barrier_t* b;
  int res;

  col = TRI_UseCollectionByNameVocBase(vocbase, TRI_COL_NAME_REPLICATION);

  if (col == NULL || col->_collection == NULL) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  primary = (TRI_primary_collection_t*) col->_collection;

  // create a barrier so the underlying collection is not unloaded
  b = TRI_CreateBarrierReplication(&primary->_barrierList);

  if (b == NULL) {
    TRI_ReleaseCollectionVocBase(vocbase, col);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // block compaction
  TRI_ReadLockReadWriteLock(&primary->_compactionLock);

  res = DumpLog(dump, primary, dataMin, dataMax, chunkSize);
  
  TRI_ReadUnlockReadWriteLock(&primary->_compactionLock);

  TRI_FreeBarrier(b);
  
  TRI_ReleaseCollectionVocBase(vocbase, col);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a replication dump container
////////////////////////////////////////////////////////////////////////////////

void TRI_InitDumpReplication (TRI_replication_dump_t* dump) {
  dump->_buffer        = NULL;
  dump->_lastFoundTick = 0;
  dump->_lastSid       = 0;
  dump->_lastShape     = NULL;
  dump->_failed        = false;
  dump->_bufferFull    = false;
  dump->_hasMore       = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
