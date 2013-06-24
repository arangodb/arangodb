////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "document-collection.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "CapConstraint/cap-constraint.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/edge-collection.h"
#include "VocBase/index.h"
#include "VocBase/key-generator.h"
#include "VocBase/marker.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static int FillIndex (TRI_document_collection_t*, 
                      TRI_index_t*);

static int CapConstraintFromJson (TRI_document_collection_t*,
                                  TRI_json_t*,
                                  TRI_idx_iid_t);

static int BitarrayIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t*,
                                  TRI_idx_iid_t);

static int GeoIndexFromJson (TRI_document_collection_t*,
                             TRI_json_t*,
                             TRI_idx_iid_t);

static int HashIndexFromJson (TRI_document_collection_t*,
                              TRI_json_t*,
                              TRI_idx_iid_t);

static int SkiplistIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t*,
                                  TRI_idx_iid_t);

static int FulltextIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t*,
                                  TRI_idx_iid_t);

static int PriorityQueueFromJson (TRI_document_collection_t*,
                                  TRI_json_t*,
                                  TRI_idx_iid_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a header is visible in the current context
////////////////////////////////////////////////////////////////////////////////

static bool IsVisible (TRI_doc_mptr_t const* header) {
  return (header != NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection revision id with the marker's tick value
////////////////////////////////////////////////////////////////////////////////

static void SetRevision (TRI_document_collection_t* document,
                         TRI_voc_tick_t tick) {
  TRI_col_info_t* info = &document->base.base._info;

  if (tick > info->_tick) {
    info->_tick = tick;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the document counter
////////////////////////////////////////////////////////////////////////////////

static void IncreaseDocumentCount (TRI_primary_collection_t* primary) {
  primary->_numberDocuments++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the document counter
////////////////////////////////////////////////////////////////////////////////

static void DecreaseDocumentCount (TRI_primary_collection_t* primary) {
  TRI_ASSERT_MAINTAINER(primary->_numberDocuments > 0);

  primary->_numberDocuments--;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header,
                               const bool isRollback) {
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t* found;

  TRI_ASSERT_MAINTAINER(document != NULL);
  TRI_ASSERT_MAINTAINER(header != NULL);
  TRI_ASSERT_MAINTAINER(header->_key != NULL);
  
  primary = &document->base;

  // add a new header
  found = TRI_InsertKeyAssociativePointer(&primary->_primaryIndex, header->_key, (void*) header, false);

  // TODO: if TRI_InsertKeyAssociativePointer fails with OOM, it returns NULL.
  // in case the call succeeds but does not find any previous value, it also returns NULL
  // this function here will continue happily in both cases.
  // These two cases must be distinguishable in order to notify the caller about an error

  if (found == NULL) {
    // success
    IncreaseDocumentCount(primary);

    return TRI_ERROR_NO_ERROR;
  } 
  else {
    // we found a previous revision in the index
    // the found revision is still alive
    LOG_TRACE("document '%s' already existed with revision %llu while creating revision %llu",
              header->_key,
              (unsigned long long) found->_rid,
              (unsigned long long) header->_rid);

    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int InsertSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header,
                                   const bool isRollback) {
  size_t i, n;
  int result;

  result = TRI_ERROR_NO_ERROR;
  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = document->_allIndexes._buffer[i];
    res = idx->insert(idx, header, isRollback);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the primary index
////////////////////////////////////////////////////////////////////////////////

static int DeletePrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header,
                               const bool isRollback) {
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t* found;

  // .............................................................................
  // remove from main index
  // .............................................................................

  primary = &document->base;
  found = TRI_RemoveKeyAssociativePointer(&primary->_primaryIndex, header->_key);

  if (found == NULL) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  DecreaseDocumentCount(primary);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int DeleteSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header,
                                   const bool isRollback) {
  size_t i, n;
  int result;

  n = document->_allIndexes._length;
  result = TRI_ERROR_NO_ERROR;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = document->_allIndexes._buffer[i];
    res = idx->remove(idx, header, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new deletion marker in memory
////////////////////////////////////////////////////////////////////////////////

static int CreateDeletionMarker (TRI_voc_tid_t tid,
                                 TRI_doc_deletion_key_marker_t** result,
                                 TRI_voc_size_t* totalSize,
                                 char* keyBody,
                                 TRI_voc_size_t keyBodySize) {
  TRI_doc_deletion_key_marker_t* marker;

  *result = NULL;
  *totalSize = sizeof(TRI_doc_deletion_key_marker_t) + keyBodySize + 1;

  marker = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, *totalSize * sizeof(char), false);
  if (marker == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
   
  TRI_InitMarker(&marker->base, TRI_DOC_MARKER_KEY_DELETION, *totalSize);
  assert(marker->_rid == 0);
  marker->_rid = TRI_NewTickVocBase();
  
  // note: the transaction id is 0 for standalone operations 
  marker->_tid = tid;
  marker->_offsetKey = sizeof(TRI_doc_deletion_key_marker_t);

  // copy the key into the marker
  memcpy(((char*) marker) + marker->_offsetKey, keyBody, keyBodySize + 1);
  
  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge marker in memory, based on another
/// existing marker
////////////////////////////////////////////////////////////////////////////////

static int CloneDocumentMarker (TRI_voc_tid_t tid,
                                TRI_df_marker_t const* original,
                                TRI_doc_document_key_marker_t** result,
                                TRI_voc_size_t* totalSize,
                                const TRI_df_marker_type_e markerType,
                                TRI_shaped_json_t const* shaped) {

  TRI_doc_document_key_marker_t* marker;
  TRI_voc_tick_t tick;
  size_t baseLength;

  *result = NULL;

  if (markerType != original->_type) {
    // cannot clone a different marker type
    return TRI_ERROR_INTERNAL;
  }

  // calculate the basic marker size
  if (markerType == TRI_DOC_MARKER_KEY_DOCUMENT) {
    // document marker
    TRI_doc_document_key_marker_t const* o = (TRI_doc_document_key_marker_t const*) original;

    baseLength = o->_offsetJson;
    TRI_ASSERT_MAINTAINER(baseLength > sizeof(TRI_doc_document_key_marker_t));
  }
  else if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    // edge marker
    TRI_doc_edge_key_marker_t const* o = (TRI_doc_edge_key_marker_t const*) original;
    
    baseLength = o->base._offsetJson;
    TRI_ASSERT_MAINTAINER(baseLength > sizeof(TRI_doc_edge_key_marker_t));
  }
  else {
    // invalid marker type
    LOG_WARNING("invalid marker type %d", (int) markerType);
    return TRI_ERROR_INTERNAL;
  }

  // calculate the total size for the marker (= marker base size + key(s) + shaped json)
  *totalSize = baseLength + shaped->_data.length;
  
  marker = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, *totalSize * sizeof(char), false);

  if (marker == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  tick = TRI_NewTickVocBase();
  // copy non-changed data (e.g. key(s)) from old marker into new marker
  TRI_CloneMarker(&marker->base, original, baseLength, *totalSize);
  assert(marker->_rid != 0);
  // the new revision must be greater than the old one
  assert((TRI_voc_rid_t) tick > marker->_rid);

  // give the marker a new revision id
  marker->_rid   = (TRI_voc_rid_t) tick;
  marker->_tid   = tid;
  marker->_shape = shaped->_sid;

  // copy shaped json into the marker
  memcpy(((char*) marker) + baseLength, (char*) shaped->_data.data, shaped->_data.length);
  
  // no need to adjust _offsetKey, _offsetJson etc. as we copied it from the old marker
  

#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_ASSERT_MAINTAINER(marker->_offsetKey  == ((TRI_doc_document_key_marker_t const*) original)->_offsetKey);
  TRI_ASSERT_MAINTAINER(marker->_offsetJson == ((TRI_doc_document_key_marker_t const*) original)->_offsetJson);

  if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* o = (TRI_doc_edge_key_marker_t const*) original;
    TRI_doc_edge_key_marker_t const* c = (TRI_doc_edge_key_marker_t const*) marker;

    TRI_ASSERT_MAINTAINER(c->_toCid == o->_toCid);
    TRI_ASSERT_MAINTAINER(c->_fromCid == o->_fromCid);
    TRI_ASSERT_MAINTAINER(c->_offsetToKey == o->_offsetToKey);
    TRI_ASSERT_MAINTAINER(c->_offsetFromKey == o->_offsetFromKey);
  }
#endif
  
  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge marker in memory
////////////////////////////////////////////////////////////////////////////////

static int CreateDocumentMarker (TRI_transaction_collection_t* trxCollection,
                                 TRI_doc_document_key_marker_t** result,
                                 TRI_voc_size_t* totalSize,
                                 char** keyBody, 
                                 const TRI_df_marker_type_e markerType,
                                 TRI_voc_key_t key,
                                 TRI_shaped_json_t const* shaped, 
                                 void const* data) {
  TRI_primary_collection_t* primary;
  TRI_doc_document_key_marker_t* marker;
  TRI_key_generator_t* keyGenerator;
  char* position;
  char keyBuffer[TRI_VOC_KEY_MAX_LENGTH + 1]; 
  TRI_voc_tid_t tid;
  TRI_voc_size_t keyBodySize;
  TRI_voc_tick_t tick;
  size_t markerSize;
  size_t keySize;
  size_t fromSize;
  size_t toSize;
  int res;

  *result = NULL;
  tick = TRI_NewTickVocBase();
  tid = TRI_GetMarkerIdTransaction(trxCollection->_transaction);
  primary = trxCollection->_collection->_collection;

  // generate the key
  keyGenerator = (TRI_key_generator_t*) primary->_keyGenerator;
  TRI_ASSERT_MAINTAINER(keyGenerator != NULL);
  
  // create key using key generator
  res = keyGenerator->generate(keyGenerator, 
                               TRI_VOC_KEY_MAX_LENGTH, 
                               tick,
                               key, 
                               (char*) &keyBuffer, 
                               &keySize);
  
  if (res != TRI_ERROR_NO_ERROR) {
    // key generation failed
    return res;
  }
   
  // add 0 byte 
  keySize += 1;
  
  // calculate the basic marker size
  if (markerType == TRI_DOC_MARKER_KEY_DOCUMENT) {
    // document marker
    fromSize    = 0;
    toSize      = 0;
    keyBodySize = TRI_DF_ALIGN_BLOCK(keySize);
    markerSize  = sizeof(TRI_doc_document_key_marker_t);
  }
  else if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    // edge marker
    TRI_document_edge_t const* edge = data;

    fromSize    = strlen(edge->_fromKey) + 1;    
    toSize      = strlen(edge->_toKey) + 1; 

    keyBodySize = TRI_DF_ALIGN_BLOCK(keySize + fromSize + toSize);
    markerSize  = sizeof(TRI_doc_edge_key_marker_t);
  }
  else {
    LOG_WARNING("invalid marker type %d", (int) markerType);
    return TRI_ERROR_INTERNAL;
  }
  

  // calculate the total size for the marker (= marker base size + key(s) + shaped json)
  *totalSize = markerSize + keyBodySize + shaped->_data.length;
  
  marker = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, *totalSize * sizeof(char), false);

  if (marker == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // set the marker type, size, revision id etc. 
  TRI_InitMarker(&marker->base, markerType, *totalSize);
  assert(marker->_rid == 0);
  marker->_rid   = (TRI_voc_rid_t) tick; 
  marker->_tid   = tid;
  marker->_shape = shaped->_sid;

  *keyBody = ((char*) marker) + markerSize;

  // copy the key into the marker
  position = *keyBody;
  memcpy(position, (char*) &keyBuffer, keySize);

  if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    // additional attributes for an edge marker
    TRI_doc_edge_key_marker_t* edgeMarker = (TRI_doc_edge_key_marker_t*) marker;
    TRI_document_edge_t const* edge = data;

    position += keySize;
    TRI_CopyString(position, (char*) edge->_toKey, toSize);
    position += toSize;
    TRI_CopyString(position, (char*) edge->_fromKey, fromSize);
    
    edgeMarker->_offsetToKey     = markerSize + keySize;
    edgeMarker->_offsetFromKey   = markerSize + keySize + toSize;
    edgeMarker->_fromCid         = edge->_fromCid;
    edgeMarker->_toCid           = edge->_toCid;
  }

  // copy shaped json into the marker
  position = ((char*) marker) + markerSize + keyBodySize;
  memcpy(position, (char*) shaped->_data.data, shaped->_data.length);
  
  // set the offsets for _key and shaped json
  marker->_offsetKey  = markerSize;
  marker->_offsetJson = markerSize + keyBodySize;
  
  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates and initially populates a document master pointer
////////////////////////////////////////////////////////////////////////////////

static int CreateHeader (TRI_document_collection_t* document,
                         const TRI_doc_document_key_marker_t* marker,
                         TRI_voc_fid_t fid,
                         TRI_doc_mptr_t** result) {
  TRI_doc_mptr_t* header;
  size_t markerSize;

  markerSize = (size_t) marker->base._size;
  assert(markerSize > 0);

  // get a new header pointer
  header = document->_headers->request(document->_headers, markerSize);

  if (header == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  header->_rid       = marker->_rid;
  header->_fid       = fid;
  header->_data      = marker;
  header->_key       = ((char*) marker) + marker->_offsetKey;

  *result = header;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          JOURNALS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal, and triggers creation of a new one
/// this is used internally for testing
////////////////////////////////////////////////////////////////////////////////

static int RotateJournal (TRI_document_collection_t* document) {
  TRI_collection_t* base;
  int res;

  base = &document->base.base;
  res = TRI_ERROR_ARANGO_NO_JOURNAL;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  if (base->_state == TRI_COL_STATE_WRITE) {
    size_t n;

    n = base->_journals._length;

    if (n > 0) {
      TRI_datafile_t* datafile;

      datafile = base->_journals._buffer[0];
      datafile->_full = true;
    
      TRI_INC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);
      TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(document);
      TRI_DEC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);

      res = TRI_ERROR_NO_ERROR;
    }
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a journal, possibly waits until a journal appears
///
/// Note that the function grabs a lock. We have to release this lock, in order
/// to allow the gc to start when waiting for a journal to appear.
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectJournal (TRI_document_collection_t* document,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  TRI_collection_t* base;
  int res;
  size_t i;
  size_t n;

  base = &document->base.base;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  while (base->_state == TRI_COL_STATE_WRITE) {
    n = base->_journals._length;

    for (i = 0;  i < n;  ++i) {
      // select datafile
      datafile = base->_journals._buffer[i];

      // try to reserve space
      res = TRI_ReserveElementDatafile(datafile, size, result, document->base.base._info._maximalSize);

      // in case of full datafile, try next
      if (res == TRI_ERROR_NO_ERROR) {
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

        return datafile;
      }
      else if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
        // some other error
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    
        LOG_ERROR("cannot select datafile: '%s'", TRI_last_error());

        return NULL;
      }
    }

    TRI_INC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);
    TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    TRI_DEC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for synchronisation
///
/// Note that a datafile is never freed. If the datafile is closed the state
/// is set to TRI_DF_STATE_CLOSED - but the datafile pointer is still valid.
/// If a datafile is closed - then the data has been copied to some other
/// datafile and has been synced.
////////////////////////////////////////////////////////////////////////////////

static void WaitSync (TRI_document_collection_t* document,
                      TRI_datafile_t* journal,
                      char const* position) {
  TRI_collection_t* base;

  base = &document->base.base;

  // no condition at all. Do NOT acquire a lock, in the worst
  // case we will miss a parameter change.

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  // wait until the sync condition is fullfilled
  while (true) {

    // check for error
    if (journal->_state == TRI_DF_STATE_WRITE_ERROR) {
      break;
    }

    // check for close
    if (journal->_state == TRI_DF_STATE_CLOSED) {
      break;
    }

    // always sync
    if (position <= journal->_synced) {
      break;
    }

    // we have to wait a bit longer
    // signal the synchroniser that there is work to do
    TRI_INC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);
    TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    TRI_DEC_SYNCHRONISER_WAITER_VOCBASE(base->_vocbase);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes data to the journal
////////////////////////////////////////////////////////////////////////////////

static int WriteElement (TRI_document_collection_t* document,
                         TRI_datafile_t* journal,
                         TRI_df_marker_t* marker,
                         TRI_voc_size_t markerSize,
                         TRI_df_marker_t* result) {
  int res;

  res = TRI_WriteCrcElementDatafile(journal, result, marker, markerSize, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  journal->_written = ((char*) result) + marker->_size;

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     DOCUMENT CRUD
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a single insert operation
////////////////////////////////////////////////////////////////////////////////

static int RollbackInsert (TRI_document_collection_t* document,
                           TRI_doc_mptr_t* newHeader,
                           TRI_doc_mptr_t* oldHeader) {
  int res;

  // there is no old header
  assert(oldHeader == NULL);

  // ignore any errors we're getting from this
  DeletePrimaryIndex(document, newHeader, true); 
  DeleteSecondaryIndexes(document, newHeader, true);

  // release the header. nobody else should point to it now
  document->_headers->release(document->_headers, newHeader, true);

  res = TRI_ERROR_NO_ERROR;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a single update operation
////////////////////////////////////////////////////////////////////////////////

static int RollbackUpdate (TRI_document_collection_t* document,
                           TRI_doc_mptr_t* newHeader,
                           TRI_doc_mptr_t* oldHeader,
                           bool adjustHeader) {
  int res;

  assert(newHeader != NULL);
  assert(oldHeader != NULL);

  // ignore any errors we're getting from this
  DeleteSecondaryIndexes(document, newHeader, true);

  if (adjustHeader) {  
    // put back the header into its old position
    document->_headers->move(document->_headers, newHeader, oldHeader);
  }

  *newHeader = *oldHeader; 

  res = InsertSecondaryIndexes(document, newHeader, true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("error rolling back update operation");
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a single remove operation
////////////////////////////////////////////////////////////////////////////////

static int RollbackRemove (TRI_document_collection_t* document,
                           TRI_doc_mptr_t* newHeader,
                           TRI_doc_mptr_t* oldHeader, 
                           bool adjustHeader) {
  int res;

  // there is no new header
  assert(newHeader == NULL);
  
  res = InsertPrimaryIndex(document, oldHeader, true);

  if (res == TRI_ERROR_NO_ERROR) {
    res = InsertSecondaryIndexes(document, oldHeader, true);
  }
  else {
    LOG_ERROR("error rolling back remove operation");
  }

  if (adjustHeader) {
    // put back the header into its old position
    document->_headers->relink(document->_headers, oldHeader, oldHeader);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes an insert marker into the datafile
////////////////////////////////////////////////////////////////////////////////

static int WriteInsertMarker (TRI_document_collection_t* document,
                              TRI_doc_document_key_marker_t* marker,
                              TRI_doc_mptr_t* header,
                              TRI_voc_size_t totalSize,
                              bool waitForSync) {
  TRI_df_marker_t* result;
  TRI_voc_fid_t fid;
  int res;

  res = TRI_WriteMarkerDocumentCollection(document, &marker->base, totalSize, &fid, &result, waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    // writing the element into the datafile has succeeded
    TRI_doc_datafile_info_t* dfi;

    // update the header with the correct fid and the positions in the datafile
    header->_fid  = fid;
    header->_data = ((char*) result);
    header->_key  = ((char*) result) + marker->_offsetKey; 

    // update the datafile info
    dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, fid, true);

    if (dfi != NULL) {
      dfi->_numberAlive++;
      dfi->_sizeAlive += (int64_t) marker->base._size;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into indexes and datafile
///
/// when this function is called, the write-lock on the indexes & documents for
/// the collection must be held
////////////////////////////////////////////////////////////////////////////////

static int InsertDocument (TRI_transaction_collection_t* trxCollection,
                           TRI_doc_document_key_marker_t* marker,
                           TRI_doc_mptr_t* header,
                           TRI_voc_size_t totalSize,
                           bool forceSync,
                           TRI_doc_mptr_t* mptr,
                           bool* freeMarker) {
  TRI_document_collection_t* document;
  bool directOperation;
  int res;

  TRI_ASSERT_MAINTAINER(*freeMarker == true);
  TRI_ASSERT_MAINTAINER(header != NULL);
  TRI_ASSERT_MAINTAINER(marker != NULL);
  TRI_ASSERT_MAINTAINER(totalSize > 0);
  TRI_ASSERT_MAINTAINER(marker->base._size == totalSize);

  document = (TRI_document_collection_t*) trxCollection->_collection->_collection;

  // .............................................................................
  // insert into indexes
  // .............................................................................

  // insert into primary index first
  res = InsertPrimaryIndex(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    document->_headers->release(document->_headers, header, true);
    return res;
  }

  // insert into secondary indexes
  res = InsertSecondaryIndexes(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // insertion into secondary indexes failed
    DeletePrimaryIndex(document, header, true);
    document->_headers->release(document->_headers, header, true);

    return res;
  }
 
  
  TRI_ASSERT_MAINTAINER(res == TRI_ERROR_NO_ERROR);

  res = TRI_AddOperationCollectionTransaction(trxCollection, 
                                              TRI_VOC_DOCUMENT_OPERATION_INSERT, 
                                              header,
                                              NULL, 
                                              NULL, 
                                              &marker->base, 
                                              totalSize, 
                                              marker->_rid,
                                              forceSync, 
                                              &directOperation);
  if (! directOperation) {
    *freeMarker = false;
  }
      
  if (res == TRI_ERROR_NO_ERROR) {
    size_t i, n;
    
    // .............................................................................
    // post process insert
    // .............................................................................
   
    *mptr = *header;

    n = document->_allIndexes._length;

    for (i = 0;  i < n;  ++i) {
      TRI_index_t* idx;

      idx = document->_allIndexes._buffer[i];

      if (idx->postInsert != NULL) {
        idx->postInsert(trxCollection, idx, header);
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  // something has failed.... now delete from the indexes again
  RollbackInsert(document, header, NULL);
  TRI_ASSERT_MAINTAINER(*freeMarker == true);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a remove marker into the datafile
////////////////////////////////////////////////////////////////////////////////

static int WriteRemoveMarker (TRI_document_collection_t* document,
                              TRI_doc_deletion_key_marker_t* marker,
                              TRI_doc_mptr_t* header,
                              TRI_voc_size_t totalSize,
                              bool waitForSync) {
  TRI_df_marker_t* result;
  TRI_voc_fid_t fid;
  int res;

  res = TRI_WriteMarkerDocumentCollection(document, &marker->base, totalSize, &fid, &result, waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    // writing the element into the datafile has succeeded
    TRI_doc_datafile_info_t* dfi;

    // update the datafile info
    dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, header->_fid, true);

    if (dfi != NULL) {
      int64_t size;

      TRI_ASSERT_MAINTAINER(header->_data != NULL);

      size = (int64_t) ((TRI_df_marker_t*) (header->_data))->_size;

      dfi->_numberAlive--;
      dfi->_sizeAlive -= size;

      dfi->_numberDead++;
      dfi->_sizeDead += size;
    }

    if (header->_fid != fid) {
      // only need to look up datafile if it is not the same
      dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, fid, true);
    }

    if (dfi != NULL) {
      dfi->_numberDeletion++;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document from the indexes and datafile
///
/// when this function is called, the write-lock on the indexes & documents for
/// the collection must be held
////////////////////////////////////////////////////////////////////////////////

static int RemoveDocument (TRI_transaction_collection_t* trxCollection,
                           TRI_doc_update_policy_t const* policy,
                           TRI_doc_deletion_key_marker_t* marker,
                           const TRI_voc_size_t totalSize,
                           const bool forceSync,
                           bool* freeMarker) {
  TRI_primary_collection_t* primary;
  TRI_document_collection_t* document;
  TRI_doc_mptr_t* header;
  bool directOperation;
  int res;

  TRI_ASSERT_MAINTAINER(*freeMarker == true);

  primary = trxCollection->_collection->_collection;
  document = (TRI_document_collection_t*) primary;

  // get the existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, ((char*) marker) + marker->_offsetKey);

  if (! IsVisible(header)) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // .............................................................................
  // check the revision
  // .............................................................................

  res = TRI_CheckUpdatePolicy(policy, header->_rid);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
 
  
  // delete from indexes
  res = DeleteSecondaryIndexes(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("deleting document from indexes failed");

    // deletion failed. roll back
    InsertSecondaryIndexes(document, header, true);

    return res;
  }
  
  res = DeletePrimaryIndex(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("deleting document from indexes failed");
   
    // deletion failed. roll back 
    InsertSecondaryIndexes(document, header, true);

    return res;
  }


  TRI_ASSERT_MAINTAINER(res == TRI_ERROR_NO_ERROR);

  res = TRI_AddOperationCollectionTransaction(trxCollection, 
                                              TRI_VOC_DOCUMENT_OPERATION_REMOVE,
                                              NULL,
                                              header, 
                                              header, 
                                              &marker->base, 
                                              totalSize, 
                                              marker->_rid,
                                              forceSync, 
                                              &directOperation);

  if (! directOperation) {
    *freeMarker = false;
  }
  
  if (res == TRI_ERROR_NO_ERROR) {
    if (directOperation) {
      // release the header pointer
      document->_headers->release(document->_headers, header, true);
    }

    return TRI_ERROR_NO_ERROR;
  }

  // deletion failed. roll back
  RollbackRemove(document, NULL, header, ! directOperation);
  TRI_ASSERT_MAINTAINER(*freeMarker == true);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing header
////////////////////////////////////////////////////////////////////////////////

static void UpdateHeader (TRI_voc_fid_t fid,
                          TRI_df_marker_t const* m,
                          TRI_doc_mptr_t* newHeader,
                          TRI_doc_mptr_t const* oldHeader) {
  TRI_doc_document_key_marker_t const* marker;

  marker = (TRI_doc_document_key_marker_t const*) m;

  assert(marker != NULL);
  assert(m->_size > 0);

  newHeader->_rid     = marker->_rid;
  newHeader->_fid     = fid;
  newHeader->_data    = marker;
  newHeader->_key     = ((char*) marker) + marker->_offsetKey;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes an update marker into the datafile
////////////////////////////////////////////////////////////////////////////////

static int WriteUpdateMarker (TRI_document_collection_t* document,
                              TRI_doc_document_key_marker_t* marker,
                              TRI_doc_mptr_t* header,
                              const TRI_doc_mptr_t* oldHeader,
                              TRI_voc_size_t totalSize,
                              bool waitForSync) {
  TRI_df_marker_t* result;
  TRI_voc_fid_t fid;
  int res;

  assert(totalSize == marker->base._size);
  res = TRI_WriteMarkerDocumentCollection(document, &marker->base, totalSize, &fid, &result, waitForSync);

  if (res == TRI_ERROR_NO_ERROR) {
    // writing the element into the datafile has succeeded
    TRI_doc_datafile_info_t* dfi;

    assert(result != NULL);

    // update the header with the correct fid and the positions in the datafile
    header->_fid  = fid;
    header->_data = ((char*) result);
    header->_key  = ((char*) result) + marker->_offsetKey;  

    // update the datafile info
    dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, fid, true);

    if (dfi != NULL) {
      dfi->_numberAlive++;
      dfi->_sizeAlive += (int64_t) marker->base._size;
    }

    if (oldHeader->_fid != fid) {
      dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, oldHeader->_fid, true);
    }

    if (dfi != NULL) {
      int64_t size = (int64_t) ((TRI_df_marker_t*) oldHeader->_data)->_size;

      dfi->_numberAlive--;
      dfi->_sizeAlive -= size;
      dfi->_numberDead++;
      dfi->_sizeDead += size;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static int UpdateDocument (TRI_transaction_collection_t* trxCollection,
                           TRI_doc_mptr_t* oldHeader,
                           TRI_doc_document_key_marker_t* marker,
                           const TRI_voc_size_t totalSize,
                           const bool forceSync,
                           TRI_doc_mptr_t* mptr,
                           bool *freeMarker) {
  TRI_document_collection_t* document;
  TRI_doc_mptr_t* newHeader;
  TRI_doc_mptr_t oldData;
  int res;
  bool directOperation;

  TRI_ASSERT_MAINTAINER(*freeMarker == true);
  document = (TRI_document_collection_t*) trxCollection->_collection->_collection;
 
  // save the old data, remember
  oldData = *oldHeader;
  
  // .............................................................................
  // update indexes
  // .............................................................................

  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)

  res = DeleteSecondaryIndexes(document, oldHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // re-enter the document in case of failure, ignore errors during rollback
    InsertSecondaryIndexes(document, oldHeader, true);

    return res;
  }
  
  
  // .............................................................................
  // update header
  // .............................................................................

  // TODO: this will be identical for non-transactional collections only
  newHeader = CONST_CAST(oldHeader);
  
  // update the header. this will modify oldHeader !!!
  UpdateHeader(0, &marker->base, newHeader, oldHeader);


  // insert new document into secondary indexes
  res = InsertSecondaryIndexes(document, newHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // rollback
    DeleteSecondaryIndexes(document, newHeader, true);
    
    // copy back old header data
    *oldHeader = oldData;
    
    InsertSecondaryIndexes(document, oldHeader, true);

    return res;
  }

  
  TRI_ASSERT_MAINTAINER(res == TRI_ERROR_NO_ERROR);

  res = TRI_AddOperationCollectionTransaction(trxCollection, 
                                              TRI_VOC_DOCUMENT_OPERATION_UPDATE, 
                                              newHeader,
                                              oldHeader, 
                                              &oldData, 
                                              &marker->base, 
                                              totalSize, 
                                              marker->_rid,
                                              forceSync, 
                                              &directOperation);

  if (! directOperation) {
    *freeMarker = false;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    if (directOperation) {
      document->_headers->moveBack(document->_headers, newHeader, &oldData);
    }

    // write new header into result  
    *mptr = *((TRI_doc_mptr_t*) newHeader);

    return TRI_ERROR_NO_ERROR;
  }

  RollbackUpdate(document, newHeader, &oldData, ! directOperation);
  TRI_ASSERT_MAINTAINER(*freeMarker == true);
    
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set the index cleanup flag for the collection
////////////////////////////////////////////////////////////////////////////////

static void SetIndexCleanupFlag (TRI_document_collection_t* document, bool value) {
  document->_cleanupIndexes = value;

  LOG_DEBUG("setting cleanup indexes flag for collection '%s' to %d",
             document->base.base._info._name,
             (int) value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an index to the collection
///
/// The caller must hold the index lock for the collection
////////////////////////////////////////////////////////////////////////////////

static void AddIndex (TRI_document_collection_t* document, TRI_index_t* idx) {
  LOG_DEBUG("adding index of type %s for collection '%s'",
            idx->typeName(idx),
            document->base.base._info._name);

  TRI_PushBackVectorPointer(&document->_allIndexes, idx);

  if (idx->cleanup != NULL) {
    SetIndexCleanupFlag(document, true);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gather aggregate information about the collection's indexes
///
/// The caller must hold the index lock for the collection
////////////////////////////////////////////////////////////////////////////////

static void RebuildIndexInfo (TRI_document_collection_t* document) {
  size_t i, n;
  bool result;

  result = false;

  n = document->_allIndexes._length;
  for (i = 0 ; i < n ; ++i) {
    TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

    if (idx->cleanup != NULL) {
      result = true;
      break;
    }
  }

  SetIndexCleanupFlag(document, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage-collect a collection's indexes
////////////////////////////////////////////////////////////////////////////////

static int CleanupIndexes (TRI_document_collection_t* document) {
  int res;

  res = TRI_ERROR_NO_ERROR;

  // cleaning indexes is expensive, so only do it if the flag is set for the
  // collection
  if (document->_cleanupIndexes) {
    TRI_primary_collection_t* primary;
    size_t i, n;

    primary = &document->base;

    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
    n = document->_allIndexes._length;
    for (i = 0 ; i < n ; ++i) {
      TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

      if (idx->cleanup != NULL) {
        res = idx->cleanup(idx);
        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }

    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for headers
////////////////////////////////////////////////////////////////////////////////

static void DebugHeadersDocumentCollection (TRI_document_collection_t* collection) {
  TRI_primary_collection_t* primary;
  void** end;
  void** ptr;

  primary = &collection->base;

  ptr = primary->_primaryIndex._table;
  end = ptr + primary->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_doc_mptr_t const* d = *ptr;

      printf("fid %llu, key %s, rid %llu\n",
             (unsigned long long) d->_fid,
             (char*) d->_key,
             (unsigned long long) d->_rid);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief notify a collection about transaction begin/commit/abort
////////////////////////////////////////////////////////////////////////////////

static int NotifyTransaction (TRI_primary_collection_t* primary, 
                              TRI_transaction_status_e status) {
  TRI_document_collection_t* document;
  size_t i, n;

  document = (TRI_document_collection_t*) primary;

  n = document->_allIndexes._length;

  for (i = 0; i < n ; ++i) {
    TRI_index_t* idx = TRI_AtVectorPointer(&document->_allIndexes, i);

    if (status == TRI_TRANSACTION_RUNNING) {
      if (idx->beginTransaction != NULL) {
        idx->beginTransaction(idx, primary);
      }
    }
    else if (status == TRI_TRANSACTION_ABORTED) {
      if (idx->abortTransaction != NULL) {
        idx->abortTransaction(idx, primary);
      }
    }
    else if (status == TRI_TRANSACTION_COMMITTED) {
      if (idx->commitTransaction != NULL) {
        idx->commitTransaction(idx, primary);
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document into the collection
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

static int InsertShapedJson (TRI_transaction_collection_t* trxCollection,
                             const TRI_voc_key_t key,
                             TRI_doc_mptr_t* mptr,
                             TRI_df_marker_type_e markerType,
                             TRI_shaped_json_t const* shaped,
                             void const* data,
                             const bool lock,
                             const bool forceSync) {

  TRI_primary_collection_t* primary;
  TRI_doc_document_key_marker_t* marker;
  TRI_doc_mptr_t* header;
  char* keyBody;
  TRI_voc_size_t totalSize;
  bool freeMarker;
  int res;

  freeMarker = true;
  primary = trxCollection->_collection->_collection;

  TRI_ASSERT_MAINTAINER(primary != NULL);
  TRI_ASSERT_MAINTAINER(shaped != NULL);

  // first create a new marker in memory
  // this does not require any locks

  res = CreateDocumentMarker(trxCollection, &marker, &totalSize, &keyBody, markerType, key, shaped, data);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
          
  TRI_ASSERT_MAINTAINER(marker != NULL);
  TRI_ASSERT_MAINTAINER(keyBody != NULL);
  TRI_ASSERT_MAINTAINER(totalSize > 0);

  if (lock) {
    // WRITE-LOCK START
    primary->beginWrite(primary);
  }

  header = NULL;
  res = CreateHeader((TRI_document_collection_t*) primary, marker, 0, &header);

  if (res == TRI_ERROR_NO_ERROR) {
    res = InsertDocument(trxCollection, marker, header, totalSize, forceSync, mptr, &freeMarker);
  }

  if (lock) {
    primary->endWrite(primary);
    // WRITE-LOCK END
  }
       
  if (freeMarker) { 
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT_MAINTAINER(mptr->_key != NULL);
    TRI_ASSERT_MAINTAINER(mptr->_data != NULL);
    TRI_ASSERT_MAINTAINER(mptr->_rid > 0);
  }
#endif

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

static int ReadShapedJson (TRI_transaction_collection_t* trxCollection,
                           const TRI_voc_key_t key,
                           TRI_doc_mptr_t* mptr,
                           const bool lock) {
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t const* header;
  
  primary = trxCollection->_collection->_collection;

  if (lock) {
    primary->beginRead(primary);
  }

  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  if (! IsVisible(header)) {
    if (lock) {
      primary->endRead(primary);
    }

    // make an empty result
    memset(mptr, 0, sizeof(TRI_doc_mptr_t));

    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // we found a document, now copy it over
  *mptr = *((TRI_doc_mptr_t*) header);

  if (lock) {
    primary->endRead(primary);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_ASSERT_MAINTAINER(mptr->_key != NULL);
  TRI_ASSERT_MAINTAINER(mptr->_data != NULL);
  TRI_ASSERT_MAINTAINER(mptr->_rid > 0);
#endif

  return TRI_ERROR_NO_ERROR;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static int UpdateShapedJson (TRI_transaction_collection_t* trxCollection,
                             const TRI_voc_key_t key,
                             TRI_doc_mptr_t* mptr,
                             TRI_shaped_json_t const* shaped,
                             TRI_doc_update_policy_t const* policy,
                             const bool lock,
                             const bool forceSync) {
  TRI_primary_collection_t* primary;
  TRI_doc_document_key_marker_t* marker;
  TRI_doc_mptr_t* header;
  TRI_voc_size_t totalSize;
  TRI_voc_tid_t tid;
  bool freeMarker;
  int res;

  freeMarker = true;
  primary = trxCollection->_collection->_collection;
  
  TRI_ASSERT_MAINTAINER(mptr != NULL);
  
  // initialise the result
  mptr->_key  = NULL;
  mptr->_data = NULL;
  
  marker = NULL;  

  if (lock) {
    primary->beginWrite(primary);
  }

  TRI_ASSERT_MAINTAINER(key != NULL);

  // get the header pointer of the previous revision
  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  if (IsVisible(header)) {
    // document found, now check revision
    res = TRI_CheckUpdatePolicy(policy, header->_rid);
  }
  else {
    // document not found
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }


  if (res == TRI_ERROR_NO_ERROR) {
    TRI_df_marker_t const* original;

    original = header->_data;

    tid = TRI_GetMarkerIdTransaction(trxCollection->_transaction);
    res = CloneDocumentMarker(tid, original, &marker, &totalSize, original->_type, shaped);

    if (res == TRI_ERROR_NO_ERROR) {
      res = UpdateDocument(trxCollection, header, marker, totalSize, forceSync, mptr, &freeMarker);
    }
  }

  if (lock) {
    primary->endWrite(primary);
  }

  if (marker != NULL && freeMarker) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT_MAINTAINER(mptr->_key != NULL);
    TRI_ASSERT_MAINTAINER(mptr->_data != NULL);
    TRI_ASSERT_MAINTAINER(mptr->_rid > 0);
  }
#endif

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static int RemoveShapedJson (TRI_transaction_collection_t* trxCollection,
                             const TRI_voc_key_t key,
                             TRI_doc_update_policy_t const* policy,
                             const bool lock,
                             const bool forceSync) {
  TRI_primary_collection_t* primary;
  TRI_doc_deletion_key_marker_t* marker;
  TRI_voc_size_t totalSize;
  TRI_voc_tid_t tid;
  bool freeMarker;
  int res;

  freeMarker = true;
  primary = trxCollection->_collection->_collection;
  TRI_ASSERT_MAINTAINER(key != NULL);

  marker = NULL;
  tid = TRI_GetMarkerIdTransaction(trxCollection->_transaction);
  res = CreateDeletionMarker(tid, &marker, &totalSize, key, strlen(key));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT_MAINTAINER(marker != NULL);

  if (lock) {
    primary->beginWrite(primary);
  }

  res = RemoveDocument(trxCollection, policy, marker, totalSize, forceSync, &freeMarker);

  if (lock) {
    primary->endWrite(primary);
  }
 
  if (freeMarker) {   
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginRead (TRI_primary_collection_t* primary) {
  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndRead (TRI_primary_collection_t* primary) {
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginWrite (TRI_primary_collection_t* primary) {
  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndWrite (TRI_primary_collection_t* primary) {
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection, with a timeout (in µseconds)
////////////////////////////////////////////////////////////////////////////////

static int BeginReadTimed (TRI_primary_collection_t* primary, 
                           uint64_t timeout,
                           uint64_t sleepPeriod) {
  uint64_t waited = 0;

  while (! TRI_TRY_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary)) {
    usleep(sleepPeriod);

    waited += sleepPeriod;

    if (waited > timeout) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection, with a timeout
////////////////////////////////////////////////////////////////////////////////

static int BeginWriteTimed (TRI_primary_collection_t* primary, 
                            uint64_t timeout,
                            uint64_t sleepPeriod) {
  uint64_t waited = 0;

  while (! TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary)) {
    usleep(sleepPeriod);

    waited += sleepPeriod;

    if (waited > timeout) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dumps information about a collection
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

static void DumpCollection (TRI_primary_collection_t* primary) {
  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  printf("----------------------------\n");
  printf("primary index, nrUsed: %lu\n", (unsigned long) primary->_primaryIndex._nrUsed);
  printf("number of documents: %lu\n", (unsigned long) primary->_numberDocuments);
  document->_headers->dump(document->_headers);
  printf("----------------------------\n\n");
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     Open iterator 
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief size of operations buffer for the open iterator
////////////////////////////////////////////////////////////////////////////////

static size_t OpenIteratorBufferSize = 128;

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
/// @brief state during opening of a collection
////////////////////////////////////////////////////////////////////////////////

typedef struct open_iterator_state_s {
  TRI_document_collection_t* _document;
  TRI_voc_tid_t              _tid;
  TRI_voc_fid_t              _fid;
  TRI_doc_datafile_info_t*   _dfi;
  TRI_vector_t               _operations;
  TRI_vocbase_t*             _vocbase; 
  uint32_t                   _trxCollections;
  bool                       _trxPrepared;
}
open_iterator_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief container for a single collection operation (used during opening)
////////////////////////////////////////////////////////////////////////////////

typedef struct open_iterator_operation_s {
  TRI_voc_document_operation_e  _type;
  TRI_df_marker_t const*        _marker;
  TRI_voc_fid_t                 _fid;
}
open_iterator_operation_t;

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
/// @brief apply an insert/update operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyInsert (open_iterator_state_t* state,
                                    open_iterator_operation_t* operation) {

  TRI_document_collection_t* document;
  TRI_primary_collection_t* primary;
  TRI_df_marker_t const* marker;
  TRI_doc_document_key_marker_t const* d;
  TRI_doc_mptr_t const* found;
  TRI_voc_key_t key;
  
  document = state->_document; 
  primary  = &document->base;
  
  marker = operation->_marker;
  d = (TRI_doc_document_key_marker_t const*) marker;

  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = TRI_FindDatafileInfoPrimaryCollection(primary, operation->_fid, true);
  }
  
  SetRevision(document, (TRI_voc_tick_t) d->_rid);

#ifdef TRI_ENABLE_LOGGER
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    LOG_TRACE("document: fid %llu, key %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
              (unsigned long long) operation->_fid,
              ((char*) d + d->_offsetKey),
              (unsigned long long) d->_rid,
              (unsigned long) d->_offsetJson,
              (unsigned long) d->_offsetKey);
  }
  else {
    TRI_doc_edge_key_marker_t const* e = (TRI_doc_edge_key_marker_t const*) marker;

    LOG_TRACE("edge: fid %llu, key %s, fromKey %s, toKey %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
              (unsigned long long) operation->_fid,
              ((char*) d + d->_offsetKey),
              ((char*) e + e->_offsetFromKey),
              ((char*) e + e->_offsetToKey),
              (unsigned long long) d->_rid,
              (unsigned long) d->_offsetJson,
              (unsigned long) d->_offsetKey);
  }
#endif
  
  key = ((char*) d) + d->_offsetKey;
  if (primary->_keyGenerator->track != NULL) {
    primary->_keyGenerator->track(primary->_keyGenerator, key);
  }

  found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  // it is a new entry
  if (found == NULL) {
    TRI_doc_mptr_t* header;
    int res;

    // get a header
    res = CreateHeader(document, (TRI_doc_document_key_marker_t*) marker, operation->_fid, &header);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("out of memory");
      
      return TRI_set_errno(res);
    }

    TRI_ASSERT_MAINTAINER(header != NULL);

    // insert into primary index
    res = InsertPrimaryIndex(document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      // insertion failed
      LOG_ERROR("inserting document into indexes failed");
      document->_headers->release(document->_headers, header, true);

      return res;
    }

    // update the datafile info
    if (state->_dfi != NULL) {
      state->_dfi->_numberAlive++;
      state->_dfi->_sizeAlive += (int64_t) marker->_size;
    }
  }

  // it is an update, but only if found has a smaller revision identifier
  else if (found->_rid < d->_rid || 
           (found->_rid == d->_rid && found->_fid <= operation->_fid)) {
    TRI_doc_mptr_t* newHeader;
    TRI_doc_mptr_t oldData;
    TRI_doc_datafile_info_t* dfi;

    // save the old data
    oldData = *found;

    newHeader = CONST_CAST(found);

    // update the header info
    UpdateHeader(operation->_fid, marker, newHeader, found);
    document->_headers->moveBack(document->_headers, newHeader, &oldData);
      
    // update the datafile info
    if (oldData._fid == state->_fid) {
      dfi = state->_dfi;
    }
    else {
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, oldData._fid, true);
    }

    if (dfi != NULL && found->_data != NULL) {
      int64_t size;

      TRI_ASSERT_MAINTAINER(found->_data != NULL);
      size = (int64_t) ((TRI_df_marker_t*) found->_data)->_size;

      dfi->_numberAlive--;
      dfi->_sizeAlive -= size;

      dfi->_numberDead++;
      dfi->_sizeDead += size;
    }

    if (state->_dfi != NULL) {
      state->_dfi->_numberAlive++;
      state->_dfi->_sizeAlive += (int64_t) marker->_size;
    }
  }

  // it is a stale update
  else {
    if (state->_dfi != NULL) {
      TRI_ASSERT_MAINTAINER(found->_data != NULL);

      state->_dfi->_numberDead++;
      state->_dfi->_sizeDead += (int64_t) ((TRI_df_marker_t*) found->_data)->_size;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a delete operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyRemove (open_iterator_state_t* state,
                                    open_iterator_operation_t* operation) {

  TRI_document_collection_t* document;
  TRI_primary_collection_t* primary;
  TRI_df_marker_t const* marker;
  TRI_doc_deletion_key_marker_t const* d;
  TRI_doc_mptr_t const* found;
  TRI_voc_key_t key;
  
  document = state->_document; 
  primary  = &document->base;
  
  marker = operation->_marker;
  d = (TRI_doc_deletion_key_marker_t const*) marker;
  
  SetRevision(document, (TRI_voc_tick_t) d->_rid);
  
  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = TRI_FindDatafileInfoPrimaryCollection(primary, operation->_fid, true);
  }

  key = ((char*) d) + d->_offsetKey;

  LOG_TRACE("deletion: fid %llu, key %s, rid %llu, deletion %llu",
            (unsigned long long) operation->_fid,
            (char*) key,
            (unsigned long long) d->_rid,
            (unsigned long long) marker->_tick);

  if (primary->_keyGenerator->track != NULL) {
    primary->_keyGenerator->track(primary->_keyGenerator, key);
  }

  found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  // it is a new entry, so we missed the create
  if (found == NULL) {
    // update the datafile info
    if (state->_dfi != NULL) {
      state->_dfi->_numberDeletion++;
    }
  }

  // it is a real delete
  else {
    TRI_doc_datafile_info_t* dfi;

    // update the datafile info
    if (found->_fid == state->_fid) {
      dfi = state->_dfi;
    }
    else {
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, found->_fid, true);
    }

    if (dfi != NULL) {
      int64_t size;

      TRI_ASSERT_MAINTAINER(found->_data != NULL);

      size = (int64_t) ((TRI_df_marker_t*) found->_data)->_size;

      dfi->_numberAlive--;
      dfi->_sizeAlive -= size;

      dfi->_numberDead++;
      dfi->_sizeDead += size;
    }

    if (state->_dfi != NULL) {
      state->_dfi->_numberDeletion++;
    }

    DeletePrimaryIndex(document, found, false);

    // free the header
    document->_headers->release(document->_headers, CONST_CAST(found), true);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyOperation (open_iterator_state_t* state,
                                       open_iterator_operation_t* operation) {
  if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    return OpenIteratorApplyRemove(state, operation);
  }
  else if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    return OpenIteratorApplyInsert(state, operation);
  }
  
  LOG_ERROR("logic error in %s", __FUNCTION__);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an operation to the list of operations when opening a collection
/// if the operation does not belong to a designated transaction, it is 
/// executed directly
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAddOperation (open_iterator_state_t* state,
                                     const TRI_voc_document_operation_e type,
                                     TRI_df_marker_t const* marker,
                                     const TRI_voc_fid_t fid) {
  open_iterator_operation_t operation;
  int res;

  operation._type   = type;
  operation._marker = marker;
  operation._fid    = fid;

  if (state->_tid == 0) {
    res = OpenIteratorApplyOperation(state, &operation);
  }
  else {
    res = TRI_PushBackVector(&state->_operations, &operation);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the list of operations during opening
////////////////////////////////////////////////////////////////////////////////

static void OpenIteratorResetOperations (open_iterator_state_t* state) {
  size_t n = state->_operations._length;

  if (n > OpenIteratorBufferSize) {
    // free some memory
    TRI_DestroyVector(&state->_operations);
    TRI_InitVector2(&state->_operations, TRI_UNKNOWN_MEM_ZONE, sizeof(open_iterator_operation_t), OpenIteratorBufferSize);
  }
  else {
    TRI_ClearVector(&state->_operations);
  }
  
  state->_tid            = 0;
  state->_trxPrepared    = false;
  state->_trxCollections = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorStartTransaction (open_iterator_state_t* state, 
                                         TRI_voc_tid_t tid,
                                         uint32_t numCollections) {
  state->_tid = tid;
  state->_trxCollections = numCollections;

  assert(state->_operations._length == 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorPrepareTransaction (open_iterator_state_t* state) {
  if (state->_tid != 0) {
    state->_trxPrepared = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a prepared transaction has a coordinator entry in _trx
////////////////////////////////////////////////////////////////////////////////

static int ReadTrxCallback (TRI_transaction_collection_t* trxCollection,
                             void* data) {
  open_iterator_state_t* state;
  TRI_voc_key_t key;
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t mptr;
  int res;
 
  state = data;
  key = TRI_StringUInt64(state->_tid);
  primary = trxCollection->_collection->_collection;

  // check whether the document exists. we will not need mptr later, so we don't need a barrier
  res = primary->read(trxCollection, key, &mptr, false);
  TRI_FreeString(TRI_CORE_MEM_ZONE, key);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAbortTransaction (open_iterator_state_t* state) {
  if (state->_tid != 0) {
    if (state->_trxCollections > 1 && state->_trxPrepared) {
      int res;

      // check whether the transaction was half-way committed...

      res = TRI_ExecuteSingleOperationTransaction(state->_vocbase, 
                                                  TRI_TRANSACTION_COORDINATOR_COLLECTION, 
                                                  TRI_TRANSACTION_READ, 
                                                  ReadTrxCallback, 
                                                  state);
      if (res == TRI_ERROR_NO_ERROR) {
        size_t i, n; 

        LOG_INFO("recovering transaction %llu", (unsigned long long) state->_tid);
        n = state->_operations._length;

        for (i = 0; i < n; ++i) {
          int r;

          open_iterator_operation_t* operation = TRI_AtVector(&state->_operations, i);

          r = OpenIteratorApplyOperation(state, operation);
          if (r != TRI_ERROR_NO_ERROR) {
            res = r;
          }
        }
      
        OpenIteratorResetOperations(state);
        return res;
      }
    }

    LOG_INFO("rolling back uncommitted transaction %llu", (unsigned long long) state->_tid);
    OpenIteratorResetOperations(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorCommitTransaction (open_iterator_state_t* state) {
  int res;

  res = TRI_ERROR_NO_ERROR;

  if (state->_trxCollections <= 1 || state->_trxPrepared) {
    size_t i, n; 

    n = state->_operations._length;

    for (i = 0; i < n; ++i) {
      int r;

      open_iterator_operation_t* operation = TRI_AtVector(&state->_operations, i);

      r = OpenIteratorApplyOperation(state, operation);
      if (r != TRI_ERROR_NO_ERROR) {
        res = r;
      }
    }
  }
  else if (state->_trxCollections > 1 && ! state->_trxPrepared) {
    OpenIteratorAbortTransaction(state);
  }
  
  // clean up
  OpenIteratorResetOperations(state);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a document (or edge) marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDocumentMarker (TRI_df_marker_t const* marker,
                                             TRI_datafile_t* datafile,
                                             open_iterator_state_t* state) {

  TRI_doc_document_key_marker_t const* d = (TRI_doc_document_key_marker_t const*) marker;
  
  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing 
      LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. "
                  "this may also be the result of an aborted transaction",
                  __FUNCTION__,
                  (unsigned long long) datafile->_fid,
                  (unsigned long long) d->_tid,
                  (unsigned long long) state->_tid);
      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }
  
  OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_INSERT, marker, datafile->_fid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a deletion marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDeletionMarker (TRI_df_marker_t const* marker,
                                             TRI_datafile_t* datafile,
                                             open_iterator_state_t* state) {
  
  TRI_doc_deletion_key_marker_t const* d = (TRI_doc_deletion_key_marker_t const*) marker;
 
  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing 
      LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. "
                  "this may also be the result of an aborted transaction",
                  __FUNCTION__,
                  (unsigned long long) datafile->_fid,
                  (unsigned long long) d->_tid,
                  (unsigned long long) state->_tid);

      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }

  OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_REMOVE, marker, datafile->_fid);

  return TRI_ERROR_NO_ERROR;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief process a "begin transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleBeginMarker (TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {
  
  TRI_doc_begin_transaction_marker_t const* m = (TRI_doc_begin_transaction_marker_t const*) marker;

  if (m->_tid != state->_tid && state->_tid != 0) {
    // some incomplete transaction was going on before us...
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. " 
                "this may also be the result of an aborted transaction",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);
    OpenIteratorAbortTransaction(state);
  }

  OpenIteratorStartTransaction(state, m->_tid, (uint32_t) m->_numCollections);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "commit transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleCommitMarker (TRI_df_marker_t const* marker,
                                           TRI_datafile_t* datafile,
                                           open_iterator_state_t* state) {
  
  TRI_doc_commit_transaction_marker_t const* m = (TRI_doc_commit_transaction_marker_t const*) marker;
  
  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);
    OpenIteratorAbortTransaction(state);
  }
  else {
    OpenIteratorCommitTransaction(state);
  }

  // reset transaction id
  state->_tid = 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "prepare transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandlePrepareMarker (TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {
  
  TRI_doc_prepare_transaction_marker_t const* m = (TRI_doc_prepare_transaction_marker_t const*) marker;
  
  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu", 
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);
    OpenIteratorAbortTransaction(state);
  }
  else {
    OpenIteratorPrepareTransaction(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an "abort transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleAbortMarker (TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {
  
  TRI_doc_abort_transaction_marker_t const* m = (TRI_doc_abort_transaction_marker_t const*) marker;
  
  if (m->_tid != state->_tid) {
    // we found an abort marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu", 
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);
  }

  OpenIteratorAbortTransaction(state);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator (TRI_df_marker_t const* marker, 
                          void* data, 
                          TRI_datafile_t* datafile, 
                          bool journal) {
  int res;

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION) {
    res = OpenIteratorHandleBeginMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION) {
    res = OpenIteratorHandleCommitMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    res = OpenIteratorHandlePrepareMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION) {
    res = OpenIteratorHandleAbortMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else {
    LOG_TRACE("skipping marker type %lu", (unsigned long) marker->_type);
    res = TRI_ERROR_NO_ERROR;
  }

  return (res == TRI_ERROR_NO_ERROR);
}

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
/// @brief fill the internal (non-user-definable) indexes
/// currently, this will only fill edge indexes
////////////////////////////////////////////////////////////////////////////////

static bool FillInternalIndexes (TRI_document_collection_t* document) {
  size_t i;
  int res;

  res = TRI_ERROR_NO_ERROR;
  
  for (i = 0;  i < document->_allIndexes._length;  ++i) {
    TRI_index_t* idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
      int r = FillIndex(document, idx);

      if (r != TRI_ERROR_NO_ERROR) {
        // return first error, but continue
        res = r;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIndexIterator (char const* filename, void* data) {
  TRI_idx_iid_t iid;
  TRI_json_t* iis;
  TRI_json_t* json;
  TRI_json_t* type;
  TRI_document_collection_t* document;
  char const* typeStr;
  char* error;
  int res;

  error = NULL;

  // load json description of the index
  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  // json must be a index description
  if (json == NULL) {
    if (error != NULL) {
      LOG_ERROR("cannot read index definition from '%s': %s", filename, error);
      TRI_Free(TRI_CORE_MEM_ZONE, error);
    }
    else {
      LOG_ERROR("cannot read index definition from '%s'", filename);
    }
    return false;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot read index definition from '%s': expecting an array", filename);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return false;
  }

  // extract the type
  type = TRI_LookupArrayJson(json, "type");

  if (type->_type != TRI_JSON_STRING) {
    LOG_ERROR("cannot read index definition from '%s': expecting a string for type", filename);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return false;
  }

  typeStr = type->_value._string.data;

  // extract the index identifier
  iis = TRI_LookupArrayJson(json, "id");

  if (iis != NULL && iis->_type == TRI_JSON_NUMBER) {
    iid = (TRI_idx_iid_t) iis->_value._number;
  }
  else if (iis != NULL && iis->_type == TRI_JSON_STRING) {
    iid = (TRI_idx_iid_t) TRI_UInt64String(iis->_value._string.data);
  }
  else {
    LOG_ERROR("ignoring index, index identifier could not be located");
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    return false;
  }
    
  TRI_UpdateTickVocBase(iid);

  // document collection of the index
  document = (TRI_document_collection_t*) data;

  // ...........................................................................
  // CAP CONSTRAINT
  // ...........................................................................

  if (TRI_EqualString(typeStr, "cap")) {
    res = CapConstraintFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }


  // ...........................................................................
  // BITARRAY INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "bitarray")) {
    res = BitarrayIndexFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // GEO INDEX (list or attribute)
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "geo1") || TRI_EqualString(typeStr, "geo2")) {
    res = GeoIndexFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // HASH INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash")) {
    res = HashIndexFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "skiplist")) {
    res = SkiplistIndexFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // FULLTEXT INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "fulltext")) {
    res = FulltextIndexFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // PRIORITY QUEUE
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "priorityqueue")) {
    res = PriorityQueueFromJson(document, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }
  
  // ...........................................................................
  // EDGES INDEX
  // ...........................................................................
  
  else if (TRI_EqualString(typeStr, "edge")) {
    // we should never get here, as users cannot create their own edge indexes
    LOG_ERROR("logic error. there should never be a JSON file describing an edges index");
    return false;
  }

  // .........................................................................
  // oops, unknown index type
  // .........................................................................

  else {
    LOG_ERROR("ignoring unknown index type '%s' for index %llu",
              typeStr,
              (unsigned long long) iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static bool InitDocumentCollection (TRI_document_collection_t* collection,
                                    TRI_shaper_t* shaper) {
  TRI_index_t* primary;
  int res;

  collection->_cleanupIndexes = false;

  res = TRI_InitPrimaryCollection(&collection->base, shaper);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyPrimaryCollection(&collection->base);

    return false;
  }

  collection->_headers = TRI_CreateSimpleHeaders();

  if (collection->_headers == NULL) {
    TRI_DestroyPrimaryCollection(&collection->base);

    return false;
  }

  // create primary index
  TRI_InitVectorPointer(&collection->_allIndexes, TRI_UNKNOWN_MEM_ZONE);

  primary = TRI_CreatePrimaryIndex(&collection->base);
  if (primary == NULL) {
    TRI_DestroyVectorPointer(&collection->_allIndexes);
    TRI_DestroyPrimaryCollection(&collection->base);

    return false;
  }

  AddIndex(collection, primary);

  // create edges index
  if (collection->base.base._info._type == TRI_COL_TYPE_EDGE) {
    TRI_index_t* edges;

    edges = TRI_CreateEdgeIndex(&collection->base);
    if (edges == NULL) {
      size_t i, n;

      n = collection->_allIndexes._length;

      for (i = 0; i < n ; ++i) {
        TRI_index_t* idx = TRI_AtVectorPointer(&collection->_allIndexes, i);
        TRI_FreeIndex(idx);
      }
      TRI_DestroyVectorPointer(&collection->_allIndexes);

      TRI_DestroyPrimaryCollection(&collection->base);

      return false;
    }

    AddIndex(collection, edges);
  }

  TRI_InitCondition(&collection->_journalsCondition);

  // setup methods
  collection->base.beginRead         = BeginRead;
  collection->base.endRead           = EndRead;

  collection->base.beginWrite        = BeginWrite;
  collection->base.endWrite          = EndWrite;

  collection->base.beginReadTimed    = BeginReadTimed;
  collection->base.beginWriteTimed   = BeginWriteTimed;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  collection->base.dump              = DumpCollection;
#endif

  collection->base.notifyTransaction = NotifyTransaction;

  // crud methods
  collection->base.insert            = InsertShapedJson;
  collection->base.read              = ReadShapedJson;
  collection->base.update            = UpdateShapedJson;
  collection->base.remove            = RemoveShapedJson;

  collection->cleanupIndexes         = CleanupIndexes;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate all markers of the collection
////////////////////////////////////////////////////////////////////////////////

static int IterateMarkersCollection (TRI_collection_t* collection) {
  open_iterator_state_t openState;

  // initialise state for iteration
  openState._document       = (TRI_document_collection_t*) collection;
  openState._tid            = 0;
  openState._trxPrepared    = false;
  openState._trxCollections = 0;
  openState._fid            = 0;
  openState._dfi            = NULL;
  openState._vocbase        = collection->_vocbase;
  
  TRI_InitVector2(&openState._operations, TRI_UNKNOWN_MEM_ZONE, sizeof(open_iterator_operation_t), OpenIteratorBufferSize);
  
  // read all documents and fill primary index
  TRI_IterateCollection(collection, OpenIterator, &openState);

  // abort any transaction that's unfinished after iterating over all markers
  OpenIteratorAbortTransaction(&openState);
  TRI_DestroyVector(&openState._operations);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_CreateDocumentCollection (TRI_vocbase_t* vocbase,
                                                         char const* path,
                                                         TRI_col_info_t* parameter,
                                                         TRI_voc_cid_t cid) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_document_collection_t* document;
  TRI_key_generator_t* keyGenerator;
  int res;
  bool waitForSync;
  bool isVolatile;

  if (cid > 0) {
    TRI_UpdateTickVocBase(cid);
  }
  else {
    cid = TRI_NewTickVocBase();
  }
  parameter->_cid = cid;

  // check if we can generate the key generator
  res = TRI_CreateKeyGenerator(parameter->_keyOptions, &keyGenerator);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG_WARNING("cannot create key generator for document collection");
    return NULL;
  }

  TRI_ASSERT_MAINTAINER(keyGenerator != NULL);


  // first create the document collection
  document = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_document_collection_t), false);

  if (document == NULL) {
    TRI_FreeKeyGenerator(keyGenerator);
    LOG_WARNING("cannot create key generator for document collection");
    return NULL;
  }

  collection = TRI_CreateCollection(vocbase, &document->base.base, path, parameter);

  if (collection == NULL) {
    TRI_FreeKeyGenerator(keyGenerator);
    LOG_ERROR("cannot create document collection");

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, document);
    return NULL;
  }

  // then the shape collection
  waitForSync = (vocbase->_forceSyncShapes || parameter->_waitForSync);
  isVolatile  = parameter->_isVolatile;

  // if the collection has the _volatile flag, the shapes collection is also volatile.
  shaper = TRI_CreateVocShaper(vocbase, collection->_directory, "SHAPES", waitForSync, isVolatile);

  if (shaper == NULL) {
    LOG_ERROR("cannot create shapes collection");

    TRI_FreeKeyGenerator(keyGenerator);
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection); // will free document

    return NULL;
  }

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    LOG_ERROR("cannot initialise document collection");

    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headers etc.?
    TRI_FreeKeyGenerator(keyGenerator);
    TRI_CloseCollection(collection);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection); // will free document

    return NULL;
  }

  document->base._keyGenerator = keyGenerator;

  // save the parameter block (within create, no need to lock)
  res = TRI_SaveCollectionInfo(collection->_directory, parameter, vocbase->_forceSyncProperties);
  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headers etc.?
    LOG_ERROR("cannot save collection parameters in directory '%s': '%s'", collection->_directory, TRI_last_error());

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection); // will free document

    return NULL;
  }


  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection (TRI_document_collection_t* collection) {
  size_t i;
  size_t n;

  TRI_DestroyCondition(&collection->_journalsCondition);

  TRI_FreeSimpleHeaders(collection->_headers);

  // free memory allocated for indexes
  n = collection->_allIndexes._length;
  for (i = 0 ; i < n ; ++i) {
    TRI_index_t* idx = (TRI_index_t*) collection->_allIndexes._buffer[i];

    TRI_FreeIndex(idx);
  }
  // free index vector
  TRI_DestroyVectorPointer(&collection->_allIndexes);

  TRI_DestroyPrimaryCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection (TRI_document_collection_t* collection) {
  TRI_DestroyDocumentCollection(collection);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
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
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection (TRI_document_collection_t* document,
                                             TRI_voc_document_operation_e type,
                                             TRI_doc_mptr_t* newHeader,
                                             TRI_doc_mptr_t* oldHeader,
                                             TRI_doc_mptr_t* oldData) {
  int res;

  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    res = RollbackInsert(document, newHeader, NULL);
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    res = RollbackUpdate(document, newHeader, oldData, true);
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    res = RollbackRemove(document, NULL, oldHeader, true);
  }
  else {
    res = TRI_ERROR_INTERNAL;
    LOG_ERROR("logic error in TRI_RollbackOperationDocumentCollection");
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a marker into the datafile
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteMarkerDocumentCollection (TRI_document_collection_t* document,
                                       TRI_df_marker_t* marker,
                                       const TRI_voc_size_t totalSize,
                                       TRI_voc_fid_t* fid,
                                       TRI_df_marker_t** result,
                                       const bool forceSync) {
  TRI_datafile_t* journal;
  int res;

  // find and select a journal
  journal = SelectJournal(document, totalSize, result);
  
  if (journal == NULL) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  if (fid != NULL) {
    *fid = journal->_fid;
  }
    
  TRI_ASSERT_MAINTAINER(*result != NULL);

  // now write marker and blob
  res = WriteElement(document, journal, marker, totalSize, *result);
    
  if (res == TRI_ERROR_NO_ERROR) {
    if (forceSync) {
      WaitSync(document, journal, ((char const*) *result) + totalSize);
    }
  }
  else {
    // writing the element into the datafile has failed
    LOG_ERROR("cannot write marker into datafile: '%s'", TRI_last_error());
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a document operation marker into the datafile
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteOperationDocumentCollection (TRI_document_collection_t* document,
                                          TRI_voc_document_operation_e type,
                                          TRI_doc_mptr_t* newHeader,
                                          TRI_doc_mptr_t* oldHeader,
                                          TRI_doc_mptr_t* oldData,
                                          TRI_df_marker_t* marker,
                                          TRI_voc_size_t totalSize,
                                          bool waitForSync) {
  int res;

  TRI_DEBUG_INTENTIONAL_FAIL_IF("TRI_WriteOperationDocumentCollection") {
    return TRI_ERROR_INTERNAL;
  }

  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    assert(oldHeader == NULL);
    assert(newHeader != NULL);
    res = WriteInsertMarker(document, (TRI_doc_document_key_marker_t*) marker, newHeader, totalSize, waitForSync);
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    assert(oldHeader != NULL);
    assert(newHeader != NULL);
    res = WriteUpdateMarker(document, (TRI_doc_document_key_marker_t*) marker, newHeader, oldHeader, totalSize, waitForSync);
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    assert(oldHeader != NULL);
    assert(newHeader == NULL);
    res = WriteRemoveMarker(document, (TRI_doc_deletion_key_marker_t*) marker, oldHeader, totalSize, waitForSync);
  }
  else {
    res = TRI_ERROR_INTERNAL;
    LOG_ERROR("logic error in %s", __FUNCTION__);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocumentCollection (TRI_document_collection_t* collection) {
  return TRI_CreateJournalPrimaryCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocumentCollection (TRI_document_collection_t* collection,
                                         size_t position) {
  return TRI_CloseJournalPrimaryCollection(&collection->base, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t* vocbase, 
                                                       char const* path) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_document_collection_t* document;
  TRI_shape_collection_t* shapeCollection;
  TRI_key_generator_t* keyGenerator;
  char* shapes;
  int res;

  // first open the document collection
  document = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_document_collection_t), false);

  if (document == NULL) {
    return NULL;
  }

  collection = TRI_OpenCollection(vocbase, &document->base.base, path);

  if (collection == NULL) {
    LOG_ERROR("cannot open document collection from path '%s'", path);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, document);
    return NULL;
  }

  // then the shape collection
  shapes = TRI_Concatenate2File(collection->_directory, "SHAPES");
  if (shapes == NULL) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    return NULL;
  }

  shaper = TRI_OpenVocShaper(vocbase, shapes);
  TRI_FreeString(TRI_CORE_MEM_ZONE, shapes);

  if (shaper == NULL) {
    LOG_ERROR("cannot open shapes collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    return NULL;
  }

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    LOG_ERROR("cannot initialise document collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    return NULL;
  }

  // check if we can generate the key generator
  res = TRI_CreateKeyGenerator(collection->_info._keyOptions, &keyGenerator);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    LOG_ERROR("cannot initialise document collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    return NULL;
  }

  TRI_ASSERT_MAINTAINER(keyGenerator != NULL);
  document->base._keyGenerator = keyGenerator;


  shapeCollection = TRI_CollectionVocShaper(shaper);

  if (shapeCollection != NULL) {
    shapeCollection->base._info._waitForSync = (vocbase->_forceSyncShapes || collection->_info._waitForSync);
  }

  // iterate over all markers of the collection
  IterateMarkersCollection(collection);

  // fill internal indexes (this is, the edges index at the moment) 
  FillInternalIndexes(document);

  // fill user-defined secondary indexes
  TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);

  // output information about datafiles and journals
  if (TRI_IsTraceLogging(__FILE__)) {
    TRI_DebugDatafileInfoPrimaryCollection(&document->base);
    DebugHeadersDocumentCollection(document);
  }

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t* collection) {
  int res;

  // closes all open compactors, journals, datafiles
  res = TRI_CloseCollection(&collection->base.base);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_CloseVocShaper(collection->base._shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // this does also destroy the shaper's underlying blob collection
  TRI_FreeVocShaper(collection->base._shaper);

  collection->base._shaper = NULL;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief pid name structure
////////////////////////////////////////////////////////////////////////////////

typedef struct pid_name_s {
  TRI_shape_pid_t _pid;
  char* _name;
}
pid_name_t;

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
/// @brief converts extracts a field list from a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ExtractFields (TRI_json_t* json, size_t* fieldCount, TRI_idx_iid_t iid) {
  TRI_json_t* fld;
  size_t j;

  fld = TRI_LookupArrayJson(json, "fields");

  if (fld == NULL || fld->_type != TRI_JSON_LIST) {
    LOG_ERROR("ignoring index %llu, 'fields' must be a list", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return NULL;
  }

  *fieldCount = fld->_value._objects._length;

  for (j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* sub = TRI_AtVector(&fld->_value._objects, j);

    if (sub->_type != TRI_JSON_STRING) {
      LOG_ERROR("ignoring index %llu, 'fields' must be a list of attribute paths", (unsigned long long) iid);

      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }
  }

  return fld;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of attribute/value pairs
///
/// Attribute/value pairs are used in the construction of static bitarray
/// indexes. These pairs are stored in a json object from which they can be
/// later extracted. Here is the extraction function given the index definition
/// as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ExtractFieldValues (TRI_json_t* jsonIndex, size_t* fieldCount, TRI_idx_iid_t iid) {
  TRI_json_t* keyValues;
  size_t j;

  keyValues = TRI_LookupArrayJson(jsonIndex, "fields");

  if (keyValues == NULL || keyValues->_type != TRI_JSON_LIST) {
    LOG_ERROR("ignoring index %llu, 'fields' must be a list", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return NULL;
  }


  *fieldCount = keyValues->_value._objects._length;


  // ...........................................................................
  // Some simple checks
  // ...........................................................................

  for (j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* keyValue;
    TRI_json_t* key;
    TRI_json_t* value;


    // .........................................................................
    // Extract the jth key value pair
    // .........................................................................

    keyValue = TRI_AtVector(&keyValues->_value._objects, j);


    // .........................................................................
    // The length of this key value pair must be two
    // .........................................................................

    if (keyValue == NULL  || keyValue->_value._objects._length != 2) {
      LOG_ERROR("ignoring index %llu, 'fields' must be a list of key value pairs", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }


    // .........................................................................
    // Extract the key
    // .........................................................................

    key = TRI_AtVector(&keyValue->_value._objects, 0);

    if (key == NULL || key->_type != TRI_JSON_STRING) {
      LOG_ERROR("ignoring index %llu, key in 'fields' pair must be an attribute (string)", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }


    // .........................................................................
    // Extract the value
    // .........................................................................

    value = TRI_AtVector(&keyValue->_value._objects, 1);

    if (value == NULL || value->_type != TRI_JSON_LIST) {
      LOG_ERROR("ignoring index %llu, value in 'fields' pair must be a list ([...])", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }

  }

  return keyValues;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static int FillIndex (TRI_document_collection_t* document, TRI_index_t* idx) {
  TRI_doc_mptr_t const* mptr;
  TRI_primary_collection_t* primary;
  uint64_t inserted;
  void** end;
  void** ptr;
  int res;

  primary = &document->base;

  // update index
  ptr = primary->_primaryIndex._table;
  end = ptr + primary->_primaryIndex._nrAlloc;

  inserted = 0;

  for (;  ptr < end;  ++ptr) {
    if (IsVisible(*ptr)) {
      mptr = *ptr;

      res = idx->insert(idx, mptr, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("failed to insert document '%llu/%s' for index %llu",
                    (unsigned long long) primary->base._info._cid,
                    (char*) mptr->_key,
                    (unsigned long long) idx->_iid);

        return res;
      }

      ++inserted;

      if (inserted % 10000 == 0) {
        LOG_DEBUG("indexed %llu documents of collection %llu", 
                  (unsigned long long) inserted, 
                  (unsigned long long) primary->base._info._cid);
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a path based, unique or non-unique index
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* LookupPathIndexDocumentCollection (TRI_document_collection_t* collection,
                                                       TRI_vector_t const* paths,
                                                       TRI_idx_type_e type,
                                                       bool unique) {
  TRI_index_t* matchedIndex = NULL;
  TRI_vector_t* indexPaths = NULL;
  size_t j;
  size_t k;

  // ...........................................................................
  // go through every index and see if we have a match
  // ...........................................................................

  for (j = 0;  j < collection->_allIndexes._length;  ++j) {
    TRI_index_t* idx = collection->_allIndexes._buffer[j];
    bool found       = true;

    // .........................................................................
    // check if the type of the index matches
    // .........................................................................

    if (idx->_type != type) {
      continue;
    }


    // .........................................................................
    // check if uniqueness matches
    // .........................................................................

    if (idx->_unique != unique) {
      continue;
    }


    // .........................................................................
    // Now perform checks which are specific to the type of index
    // .........................................................................

    switch (type) {

      case TRI_IDX_TYPE_BITARRAY_INDEX: {
        TRI_bitarray_index_t* baIndex = (TRI_bitarray_index_t*) idx;
        indexPaths = &(baIndex->_paths);
        break;
      }

      case TRI_IDX_TYPE_HASH_INDEX: {
        TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;
        indexPaths = &(hashIndex->_paths);
        break;
      }

      case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX: {
        TRI_priorityqueue_index_t* pqIndex = (TRI_priorityqueue_index_t*) idx;
        indexPaths = &(pqIndex->_paths);
        break;
      }

      case TRI_IDX_TYPE_SKIPLIST_INDEX: {
        TRI_skiplist_index_t* slIndex = (TRI_skiplist_index_t*) idx;
        indexPaths = &(slIndex->_paths);
        break;
      }

      default: {
        assert(false);
        break;
      }

    }

    if (indexPaths == NULL) {
      // this may actually happen if compiled with -DNDEBUG
      return NULL;
    }

    // .........................................................................
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................

    if (paths->_length != indexPaths->_length) {
      continue;
    }


    // .........................................................................
    // go through all the attributes and see if they match
    // .........................................................................

    for (k = 0;  k < paths->_length;  ++k) {
      TRI_shape_pid_t indexShape = *((TRI_shape_pid_t*)(TRI_AtVector(indexPaths, k)));
      TRI_shape_pid_t givenShape = *((TRI_shape_pid_t*)(TRI_AtVector(paths, k)));

      if (indexShape != givenShape) {
        found = false;
        break;
      }
    }

    // stop if we found a match
    if (found) {
      matchedIndex = idx;
      break;
    }
  }

  return matchedIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief restores a bitarray based index (template)
////////////////////////////////////////////////////////////////////////////////

static int BitarrayBasedIndexFromJson (TRI_document_collection_t* document,
                                       TRI_json_t* definition,
                                       TRI_idx_iid_t iid,
                                       TRI_index_t* (*creator)(TRI_document_collection_t*,
                                                               const TRI_vector_pointer_t*,
                                                               const TRI_vector_pointer_t*,
                                                               TRI_idx_iid_t,
                                                               bool,
                                                               bool*, int*, char**)) {
  TRI_index_t* idx;
  TRI_json_t* uniqueIndex;
  TRI_json_t* supportUndefIndex;
  TRI_json_t* keyValues;
  TRI_vector_pointer_t attributes;
  TRI_vector_pointer_t values;
  // bool unique;
  bool supportUndef;
  bool created;
  size_t fieldCount;
  size_t j;
  int errorNum;
  char* errorStr;

  // ...........................................................................
  // extract fields list (which is a list of key/value pairs for a bitarray index
  // ...........................................................................

  keyValues = ExtractFieldValues(definition, &fieldCount, iid);
  if (keyValues == NULL) {
    return TRI_errno();
  }


  // ...........................................................................
  // For a bitarray index we require at least one attribute path and one set of
  // possible values for that attribute (that is, we require at least one pair)
  // ...........................................................................

  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path and one list of values",(unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }


  // ...........................................................................
  // A bitarray index is always (for now) non-unique. Irrespective of this fact
  // attempt to extract the 'uniqueness value' from the json object representing
  // the bitarray index.
  // ...........................................................................

  // unique = false;
  uniqueIndex = TRI_LookupArrayJson(definition, "unique");
  if (uniqueIndex == NULL || uniqueIndex->_type != TRI_JSON_BOOLEAN) {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }
  // unique = uniqueIndex->_value._boolean;


  // ...........................................................................
  // A bitarray index can support documents where one or more attributes are
  // undefined. Determine if this is the case.
  // ...........................................................................

  supportUndef = false;
  supportUndefIndex = TRI_LookupArrayJson(definition, "undefined");
  if (supportUndefIndex == NULL || supportUndefIndex->_type != TRI_JSON_BOOLEAN) {
    LOG_ERROR("ignoring index %llu, could not determine if index supports undefined values", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }
  supportUndef = supportUndefIndex->_value._boolean;

  // ...........................................................................
  // Initialise the vectors in which we store the fields and their corresponding
  // values
  // ...........................................................................

  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);
  TRI_InitVectorPointer(&values, TRI_CORE_MEM_ZONE);


  // ...........................................................................
  // find fields and values and store them in the vector pointers
  // ...........................................................................

  for (j = 0;  j < fieldCount;  ++j) {
    TRI_json_t* keyValue;
    TRI_json_t* key;
    TRI_json_t* value;

    keyValue = TRI_AtVector(&keyValues->_value._objects, j);
    key      = TRI_AtVector(&keyValue->_value._objects, 0);
    value    = TRI_AtVector(&keyValue->_value._objects, 1);

    TRI_PushBackVectorPointer(&attributes, key->_value._string.data);
    TRI_PushBackVectorPointer(&values, value);
  }


  // ...........................................................................
  // attempt to create the index or retrieve an existing one
  // ...........................................................................
  errorStr = NULL;
  idx = creator(document, &attributes, &values, iid, supportUndef, &created, &errorNum, &errorStr);


  // ...........................................................................
  // cleanup
  // ...........................................................................

  TRI_DestroyVectorPointer(&attributes);
  TRI_DestroyVectorPointer(&values);


  // ...........................................................................
  // Check if the creation or lookup succeeded
  // ...........................................................................

  if (idx == NULL) {
    LOG_ERROR("cannot create bitarray index %llu", (unsigned long long) iid);
    if (errorStr != NULL) {
      LOG_TRACE("%s", errorStr);
      TRI_Free(TRI_CORE_MEM_ZONE, errorStr);
    }
    return errorNum;
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief restores a path based index (template)
////////////////////////////////////////////////////////////////////////////////

static int PathBasedIndexFromJson (TRI_document_collection_t* document,
                                   TRI_json_t* definition,
                                   TRI_idx_iid_t iid,
                                   TRI_index_t* (*creator)(TRI_document_collection_t*,
                                                           TRI_vector_pointer_t const*,
                                                           TRI_idx_iid_t,
                                                           bool,
                                                           bool*)) {
  TRI_index_t* idx;
  TRI_json_t* bv;
  TRI_json_t* fld;
  TRI_json_t* fieldStr;
  TRI_vector_pointer_t attributes;
  bool unique;
  size_t fieldCount;
  size_t j;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path",(unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the hash index is unique or non-unique
  unique = false;
  bv = TRI_LookupArrayJson(definition, "unique");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    unique = bv->_value._boolean;
  }
  else {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // Initialise the vector in which we store the fields on which the hashing
  // will be based.
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  // find fields
  for (j = 0;  j < fieldCount;  ++j) {
    fieldStr = TRI_AtVector(&fld->_value._objects, j);

    TRI_PushBackVectorPointer(&attributes, fieldStr->_value._string.data);
  }

  // create the index
  idx = creator(document, &attributes, iid, unique, NULL);

  // cleanup
  TRI_DestroyVectorPointer(&attributes);

  if (idx == NULL) {
    LOG_ERROR("cannot create hash index %llu", (unsigned long long) iid);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares pid and name
////////////////////////////////////////////////////////////////////////////////

static int ComparePidName (void const* left, void const* right) {
  pid_name_t const* l = left;
  pid_name_t const* r = right;

  return l->_pid - r->_pid;
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
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underlying collection!
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t* document) {
  TRI_vector_pointer_t* vector;
  size_t n;
  size_t i;

  vector = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);
  TRI_InitVectorPointer(vector, TRI_CORE_MEM_ZONE);

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    TRI_json_t* json;

    idx = document->_allIndexes._buffer[i];

    json = idx->json(idx, (TRI_primary_collection_t*) document);

    if (json != NULL) {
      TRI_PushBackVectorPointer(vector, json);
    }
  }

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection (TRI_document_collection_t* document, TRI_idx_iid_t iid) {
  TRI_index_t* found;
  TRI_primary_collection_t* primary;
  size_t n;
  size_t i;

  if (iid == 0) {
    return true;
  }

  found = NULL;
  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  n = document->_allIndexes._length;
  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX || idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
      // cannot remove these index types
      continue;
    }

    if (idx->_iid == iid) {
      found = TRI_RemoveVectorPointer(&document->_allIndexes, i);

      if (found != NULL && found->removeIndex != NULL) {
        // notify the index about its removal
        found->removeIndex(found, primary);
      }

      break;
    }
  }

  RebuildIndexInfo(document);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != NULL) {
    bool removeResult;

    removeResult = TRI_RemoveIndexFile(primary, found);
    TRI_FreeIndex(found);

    return removeResult;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts attribute names to lists of pids and names
///
/// In case of an error, all allocated memory in pids and names will be
/// freed.
////////////////////////////////////////////////////////////////////////////////

int TRI_PidNamesByAttributeNames (TRI_vector_pointer_t const* attributes,
                                  TRI_shaper_t* shaper,
                                  TRI_vector_t* pids,
                                  TRI_vector_pointer_t* names,
                                  bool sorted) {
  pid_name_t* pidnames;
  size_t j;

  // .............................................................................
  // sorted case
  // .............................................................................

  if (sorted) {

    // combine name and pid
    pidnames = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(pid_name_t) * attributes->_length, false);
    if (pidnames == NULL) {
      LOG_ERROR("out of memory in TRI_PidNamesByAttributeNames");
      return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    }

    for (j = 0;  j < attributes->_length;  ++j) {
      pidnames[j]._name = attributes->_buffer[j];
      pidnames[j]._pid = shaper->findAttributePathByName(shaper, pidnames[j]._name);

      if (pidnames[j]._pid == 0) {
        TRI_Free(TRI_CORE_MEM_ZONE, pidnames);

        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }
    }

    // sort according to pid
    qsort(pidnames, attributes->_length, sizeof(pid_name_t), ComparePidName);

    // split again
    TRI_InitVector(pids, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));
    TRI_InitVectorPointer(names, TRI_CORE_MEM_ZONE);

    for (j = 0;  j < attributes->_length;  ++j) {
      TRI_PushBackVector(pids, &pidnames[j]._pid);
      TRI_PushBackVectorPointer(names, pidnames[j]._name);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, pidnames);
  }

  // .............................................................................
  // unsorted case
  // .............................................................................

  else {
    TRI_InitVector(pids, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));
    TRI_InitVectorPointer(names, TRI_CORE_MEM_ZONE);

    for (j = 0;  j < attributes->_length;  ++j) {
      char* name;
      TRI_shape_pid_t pid;

      name = attributes->_buffer[j];
      pid = shaper->findAttributePathByName(shaper, name);

      if (pid == 0) {
        TRI_DestroyVector(pids);
        TRI_DestroyVectorPointer(names);

        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      TRI_PushBackVector(pids, &pid);
      TRI_PushBackVectorPointer(names, name);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a cap constraint to a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                           size_t count,
                                                           int64_t size,
                                                           TRI_idx_iid_t iid,
                                                           bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;
  int res;

  primary = &document->base;

  if (created != NULL) {
    *created = false;
  }

  // check if we already know a cap constraint
  if (primary->_capConstraint != NULL) {
    if (primary->_capConstraint->_count == count &&
        primary->_capConstraint->_size == size) {
      return &primary->_capConstraint->base;
    }
    else {
      TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
      return NULL;
    }
  }

  // create a new index
  idx = TRI_CreateCapConstraint(primary, count, size);

  if (idx == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeCapConstraint(idx);
    return NULL;
  }

  // and store index
  AddIndex(document, idx);
  primary->_capConstraint = (TRI_cap_constraint_t*) idx;

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int CapConstraintFromJson (TRI_document_collection_t* document,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  TRI_json_t* val1;
  TRI_json_t* val2;
  TRI_index_t* idx;
  size_t count;
  int64_t size;

  val1 = TRI_LookupArrayJson(definition, "size");
  val2 = TRI_LookupArrayJson(definition, "byteSize");

  if ((val1 == NULL || val1->_type != TRI_JSON_NUMBER) &&
      (val2 == NULL || val2->_type != TRI_JSON_NUMBER)) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' and 'byteSize' missing", 
              (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  count = 0;
  if (val1 != NULL && val1->_value._number > 0.0) {
    count = (size_t) val1->_value._number;
  }

  size = 0;
  if (val2 != NULL && val2->_value._number > (double) TRI_CAP_CONSTRAINT_MIN_SIZE) {
    size = (int64_t) val2->_value._number;
  }

  if (count == 0 && size == 0) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' must be at least 1, "
              "or 'byteSize' must be at least " 
              TRI_CAP_CONSTRAINT_MIN_SIZE_STR,
              (unsigned long long) iid); 

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }
  
  idx = CreateCapConstraintDocumentCollection(document, count, size, iid, NULL);

  return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
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
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                        size_t count,
                                                        int64_t size,
                                                        bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateCapConstraintDocumentCollection(document, count, size, 0, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateGeoIndexDocumentCollection (TRI_document_collection_t* document,
                                                      char const* location,
                                                      char const* latitude,
                                                      char const* longitude,
                                                      bool geoJson,
                                                      bool unique,
                                                      bool ignoreNull,
                                                      TRI_idx_iid_t iid,
                                                      bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;
  TRI_shape_pid_t lat;
  TRI_shape_pid_t loc;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;
  int res;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = NULL;

  primary = &document->base;
  shaper = primary->_shaper;

  if (location != NULL) {
    loc = shaper->findAttributePathByName(shaper, location);

    if (loc == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  if (latitude != NULL) {
    lat = shaper->findAttributePathByName(shaper, latitude);

    if (lat == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  if (longitude != NULL) {
    lon = shaper->findAttributePathByName(shaper, longitude);

    if (lon == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  // check, if we know the index
  if (location != NULL) {
    idx = TRI_LookupGeoIndex1DocumentCollection(document, loc, geoJson, unique, ignoreNull);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, lat, lon, unique, ignoreNull);
  }
  else {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");
    return NULL;
  }

  if (idx != NULL) {
    LOG_TRACE("geo-index already created for location '%s'", location);

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // create a new index
  if (location != NULL) {
    idx = TRI_CreateGeo1Index(primary, location, loc, geoJson, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeo2Index(primary, latitude, lat, longitude, lon, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld, %ld",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);
    return NULL;
  }

  // and store index
  AddIndex(document, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int GeoIndexFromJson (TRI_document_collection_t* document,
                             TRI_json_t* definition,
                             TRI_idx_iid_t iid) {
  TRI_index_t* idx;
  TRI_json_t* bv;
  TRI_json_t* fld;
  bool unique;
  bool ignoreNull;
  char const* typeStr;
  size_t fieldCount;

  typeStr = TRI_LookupArrayJson(definition, "type")->_value._string.data;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract unique
  unique = false;
  // first try "unique" attribute
  bv = TRI_LookupArrayJson(definition, "unique");
  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    unique = bv->_value._boolean;
  }
  else {
    // then "constraint" 
    bv = TRI_LookupArrayJson(definition, "constraint");

    if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
      unique = bv->_value._boolean;
    }
  }

  // extract ignore null
  ignoreNull = false;
  bv = TRI_LookupArrayJson(definition, "ignoreNull");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    ignoreNull = bv->_value._boolean;
  }

  // list style
  if (TRI_EqualString(typeStr, "geo1")) {
    bool geoJson;

    // extract geo json
    geoJson = false;
    bv = TRI_LookupArrayJson(definition, "geoJson");

    if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
      geoJson = bv->_value._boolean;
    }

    // need just one field
    if (fieldCount == 1) {
      TRI_json_t* loc;

      loc = TRI_AtVector(&fld->_value._objects, 0);

      idx = CreateGeoIndexDocumentCollection(document,
                                        loc->_value._string.data,
                                        NULL,
                                        NULL,
                                        geoJson,
                                        unique,
                                        ignoreNull,
                                        iid,
                                        NULL);

      return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
    }
    else {
      LOG_ERROR("ignoring %s-index %llu, 'fields' must be a list with 1 entries",
                typeStr, (unsigned long long) iid);

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  // attribute style
  else if (TRI_EqualString(typeStr, "geo2")) {
    if (fieldCount == 2) {
      TRI_json_t* lat;
      TRI_json_t* lon;

      lat = TRI_AtVector(&fld->_value._objects, 0);
      lon = TRI_AtVector(&fld->_value._objects, 1);

      idx = CreateGeoIndexDocumentCollection(document,
                                        NULL,
                                        lat->_value._string.data,
                                        lon->_value._string.data,
                                        false,
                                        unique,
                                        ignoreNull,
                                        iid,
                                        NULL);

      return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
    }
    else {
      LOG_ERROR("ignoring %s-index %llu, 'fields' must be a list with 2 entries",
                typeStr, (unsigned long long) iid);

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  else {
    assert(false);
  }

  return 0; // shut the vc++ up
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
/// @brief finds a geo index, list style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                    TRI_shape_pid_t location,
                                                    bool geoJson,
                                                    bool unique,
                                                    bool ignoreNull) {
  size_t n;
  size_t i;

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO1_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_location != 0 && geo->_location == location && geo->_geoJson == geoJson && idx->_unique == unique) {
        if (! unique || geo->base._ignoreNull == ignoreNull) {
          return idx;
        }
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                    TRI_shape_pid_t latitude,
                                                    TRI_shape_pid_t longitude,
                                                    bool unique,
                                                    bool ignoreNull) {
  size_t n;
  size_t i;

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO2_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_latitude != 0 && geo->_longitude != 0 && geo->_latitude == latitude && geo->_longitude == longitude && idx->_unique == unique) {
        if (! unique || geo->base._ignoreNull == ignoreNull) {
          return idx;
        }
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                    char const* location,
                                                    bool geoJson,
                                                    bool unique,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateGeoIndexDocumentCollection(document, location, NULL, NULL, geoJson, unique, ignoreNull, 0, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                    char const* latitude,
                                                    char const* longitude,
                                                    bool unique,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateGeoIndexDocumentCollection(document, NULL, latitude, longitude, false, unique, ignoreNull, 0, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                       TRI_vector_pointer_t const* attributes,
                                                       TRI_idx_iid_t iid,
                                                       bool unique,
                                                       bool* created) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  idx = NULL;

  // determine the sorted shape ids for the attributes
  res = TRI_PidNamesByAttributeNames(attributes,
                                     document->base._shaper,
                                     &paths,
                                     &fields,
                                     true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }

    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_HASH_INDEX, unique);

  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("hash-index already created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // create the hash index. we'll provide it with the current number of documents
  // in the collection so the index can do a sensible memory preallocation
  idx = TRI_CreateHashIndex(&document->base,
                            &fields,
                            &paths,
                            unique,
                            document->base._primaryIndex._nrUsed);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // if index id given, use it otherwise use the default.
  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);
    return NULL;
  }

  // store index and return
  AddIndex(document, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int HashIndexFromJson (TRI_document_collection_t* document,
                              TRI_json_t* definition,
                              TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(document, definition, iid, CreateHashIndexDocumentCollection);
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
/// @brief finds a hash index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                    TRI_vector_pointer_t const* attributes,
                                                    bool unique) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  primary = &document->base;

  // determine the sorted shape ids for the attributes
  res = TRI_PidNamesByAttributeNames(attributes,
                                     primary->_shaper,
                                     &paths,
                                     &fields,
                                     true);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_HASH_INDEX, unique);

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                    TRI_vector_pointer_t const* attributes,
                                                    bool unique,
                                                    bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // given the list of attributes (as strings)
  idx = CreateHashIndexDocumentCollection(document, attributes, 0, unique, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skiplist index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                           TRI_vector_pointer_t const* attributes,
                                                           TRI_idx_iid_t iid,
                                                           bool unique,
                                                           bool* created) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = TRI_PidNamesByAttributeNames(attributes,
                                     document->base._shaper,
                                     &paths,
                                     &fields,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }

    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique);

  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("skiplist-index already created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // Create the skiplist index
  idx = TRI_CreateSkiplistIndex(&document->base, &fields, &paths, unique);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // If index id given, use it otherwise use the default.
  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);
    return NULL;
  }

  // store index and return
  AddIndex(document, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(document, definition, iid, CreateSkiplistIndexDocumentCollection);
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
/// @brief finds a skiplist index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_vector_pointer_t const* attributes,
                                                        bool unique) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  primary = &document->base;

  // determine the unsorted shape ids for the attributes
  res = TRI_PidNamesByAttributeNames(attributes,
                                     primary->_shaper,
                                     &paths,
                                     &fields,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique);

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_vector_pointer_t const* attributes,
                                                        bool unique,
                                                        bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateSkiplistIndexDocumentCollection(document, attributes, 0, unique, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                           const char* attributeName,
                                                           const bool indexSubstrings,
                                                           int minWordLength) {
  size_t i;

  assert(attributeName);

  for (i = 0; i < document->_allIndexes._length; ++i) {
    TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_FULLTEXT_INDEX) {
      TRI_fulltext_index_t* fulltext = (TRI_fulltext_index_t*) idx;
      char* fieldName;

      // 2013-01-17: deactivated substring indexing
      // if (fulltext->_indexSubstrings != indexSubstrings) {
      //   continue;
      // }

      if (fulltext->_minWordLength != minWordLength) {
        continue;
      }

      if (fulltext->base._fields._length != 1) {
        continue;
      }

      fieldName = (char*) fulltext->base._fields._buffer[0];

      if (fieldName != 0 && TRI_EqualString(fieldName, attributeName)) {
        return idx;
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a fulltext index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                           const char* attributeName,
                                                           const bool indexSubstrings,
                                                           int minWordLength,
                                                           TRI_idx_iid_t iid,
                                                           bool* created) {
  TRI_index_t* idx;
  int res;

  // ...........................................................................
  // Attempt to find an existing index with the same attribute
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength);
  if (idx != NULL) {
    LOG_TRACE("fulltext-index already created");

    if (created != NULL) {
      *created = false;
    }
    return idx;
  }

  // Create the fulltext index
  idx = TRI_CreateFulltextIndex(&document->base, attributeName, indexSubstrings, minWordLength);

  // If index id given, use it otherwise use the default.
  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFulltextIndex(idx);
    return NULL;
  }

  // store index and return
  AddIndex(document, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int FulltextIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  TRI_index_t* idx;
  TRI_json_t* attribute;
  TRI_json_t* fld;
  // TRI_json_t* indexSubstrings;
  TRI_json_t* minWordLength;
  char* attributeName;
  size_t fieldCount;
  bool doIndexSubstrings;
  int minWordLengthValue;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount != 1) {
    LOG_ERROR("ignoring index %llu, has an invalid number of attributes", (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  attribute = TRI_AtVector(&fld->_value._objects, 0);
  attributeName = attribute->_value._string.data;

  // 2013-01-17: deactivated substring indexing
  // indexSubstrings = TRI_LookupArrayJson(definition, "indexSubstrings");

  doIndexSubstrings = false;
  // if (indexSubstrings != NULL && indexSubstrings->_type == TRI_JSON_BOOLEAN) {
  //  doIndexSubstrings = indexSubstrings->_value._boolean;
  // }

  minWordLength = TRI_LookupArrayJson(definition, "minLength");
  minWordLengthValue = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
  if (minWordLength != NULL && minWordLength->_type == TRI_JSON_NUMBER) {
    minWordLengthValue = (int) minWordLength->_value._number;
  }

  // create the index
  idx = LookupFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue);

  if (idx == NULL) {
    bool created;
    idx = CreateFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue, iid, &created);
  }

  if (idx == NULL) {
    LOG_ERROR("cannot create fulltext index %llu", (unsigned long long) iid);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
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
/// @brief finds a fulltext index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const char* attributeName,
                                                        const bool indexSubstrings,
                                                        int minWordLength) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = LookupFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength);

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const char* attributeName,
                                                        const bool indexSubstrings,
                                                        int minWordLength,
                                                        bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength, 0, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a priroity queue index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreatePriorityQueueIndexDocumentCollection (TRI_document_collection_t* document,
                                                                TRI_vector_pointer_t const* attributes,
                                                                TRI_idx_iid_t iid,
                                                                bool unique,
                                                                bool* created) {
  TRI_index_t* idx     = NULL;
  TRI_shaper_t* shaper = document->base._shaper;
  TRI_vector_t paths;
  TRI_vector_pointer_t fields;
  int res;
  size_t j;


  TRI_InitVector(&paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));
  TRI_InitVectorPointer(&fields, TRI_UNKNOWN_MEM_ZONE);

  // ...........................................................................
  // Determine the shape ids for the attributes
  // ...........................................................................

  for (j = 0;  j < attributes->_length;  ++j) {
    char* path = attributes->_buffer[j];
    TRI_shape_pid_t shape = shaper->findAttributePathByName(shaper, path);

    if (shape == 0) {
      TRI_DestroyVector(&paths);
      TRI_DestroyVectorPointer(&fields);
      return NULL;
    }

    TRI_PushBackVector(&paths, &shape);
    TRI_PushBackVectorPointer(&fields, path);
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = TRI_LookupPriorityQueueIndexDocumentCollection(document, &paths);

  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);

    LOG_TRACE("priority queue  index already created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // ...........................................................................
  // Create the priority queue index
  // ...........................................................................

  idx = TRI_CreatePriorityQueueIndex(&document->base, &fields, &paths, unique);

  // ...........................................................................
  // If index id given, use it otherwise use the default.
  // ...........................................................................

  if (iid) {
    idx->_iid = iid;
  }

  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................

  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreePriorityQueueIndex(idx);
    return NULL;
  }

  // ...........................................................................
  // store index
  // ...........................................................................

  AddIndex(document, idx);

  // ...........................................................................
  // release memory allocated to vector
  // ...........................................................................

  TRI_DestroyVector(&paths);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int PriorityQueueFromJson (TRI_document_collection_t* document,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(document, definition, iid, CreatePriorityQueueIndexDocumentCollection);
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
/// @brief finds a priority queue index (non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupPriorityQueueIndexDocumentCollection (TRI_document_collection_t* document,
                                                             const TRI_vector_t* paths) {
  TRI_index_t* matchedIndex = NULL;
  size_t j, k;

  // ...........................................................................
  // go through every index and see if we have a match
  // ...........................................................................

  for (j = 0;  j < document->_allIndexes._length;  ++j) {
    TRI_index_t* idx                    = document->_allIndexes._buffer[j];
    TRI_priorityqueue_index_t* pqIndex  = (TRI_priorityqueue_index_t*) idx;
    bool found                          = true;

    // .........................................................................
    // check that the type of the index is in fact a skiplist index
    // .........................................................................

    if (idx->_type != TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX) {
      continue;
    }

    // .........................................................................
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................

    if (paths->_length != pqIndex->_paths._length) {
      continue;
    }

    // .........................................................................
    // Go through all the attributes and see if they match
    // .........................................................................

    for (k = 0; k < paths->_length; ++k) {
      TRI_shape_pid_t field = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,k)));
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,k)));

      if (field != shape) {
        found = false;
        break;
      }
    }


    if (found) {
      matchedIndex = idx;
      break;
    }
  }

  return matchedIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a priority queue index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsurePriorityQueueIndexDocumentCollection(TRI_document_collection_t* document,
                                                            TRI_vector_pointer_t const* attributes,
                                                            bool unique,
                                                            bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // Given the list of attributes (as strings)
  idx = CreatePriorityQueueIndexDocumentCollection(document, attributes, 0, unique, created);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a bitarray index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                      const TRI_vector_pointer_t* attributes,
                                                      const TRI_vector_pointer_t* values,
                                                      TRI_idx_iid_t iid,
                                                      bool supportUndef,
                                                      bool* created,
                                                      int* errorNum,
                                                      char** errorStr) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = TRI_PidNamesByAttributeNames(attributes,
                                     document->base._shaper,
                                     &paths,
                                     &fields,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }
    *errorNum = res;
    *errorStr  = TRI_DuplicateString("Bitarray index attributes could not be accessed.");
    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_BITARRAY_INDEX, false);

  if (idx != NULL) {

    // .........................................................................
    // existing index has been located which matches the list of attributes
    // return this one
    // .........................................................................

    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("bitarray-index previously created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }


  // ...........................................................................
  // Create the bitarray index
  // ...........................................................................

  idx = TRI_CreateBitarrayIndex(&document->base, &fields, &paths, (TRI_vector_pointer_t*)(values), supportUndef, errorNum, errorStr);


  // ...........................................................................
  // release memory allocated to fields & paths vectors
  // ...........................................................................

  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);


  // ...........................................................................
  // Perhaps the index was not created in the function TRI_CreateBitarrayIndex
  // ...........................................................................

  if (idx == NULL) {
    LOG_TRACE("bitarray index could not be created in TRI_CreateBitarrayIndex");
    if (created != NULL) {
      *created = false;
    }
    return idx;
  }

  // ...........................................................................
  // If an index id given, use it otherwise use the default (generate one)
  // ...........................................................................

  if (iid) {
    idx->_iid = iid;
  }


  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................

  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {

    // .........................................................................
    // for some reason one or more of the existing documents has caused the
    // index to fail. Remove the index from the collection and return null.
    // .........................................................................

    *errorNum = res;
    *errorStr = TRI_DuplicateString("Bitarray index creation aborted due to documents within collection.");
    TRI_FreeBitarrayIndex(idx);
    return NULL;
  }


  // ...........................................................................
  // store index within the collection and return
  // ...........................................................................

  AddIndex(document, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int BitarrayIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return BitarrayBasedIndexFromJson(document, definition, iid, CreateBitarrayIndexDocumentCollection);
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
/// @brief finds a bitarray index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const TRI_vector_pointer_t* attributes) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int result;

  primary = &document->base;

  // ...........................................................................
  // determine the unsorted shape ids for the attributes
  // ...........................................................................

  result = TRI_PidNamesByAttributeNames(attributes, primary->_shaper, &paths,
                                     &fields, false);

  if (result != TRI_ERROR_NO_ERROR) {
    return NULL;
  }


  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);


  // .............................................................................
  // attempt to go through the indexes within the collection and see if we can
  // locate the index
  // .............................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, false);


  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  // .............................................................................
  // release memory allocated to vector
  // .............................................................................

  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const TRI_vector_pointer_t* attributes,
                                                        const TRI_vector_pointer_t* values,
                                                        bool supportUndef,
                                                        bool* created,
                                                        int* errorCode,
                                                        char** errorStr) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  *errorCode = TRI_ERROR_NO_ERROR;
  *errorStr  = NULL;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateBitarrayIndexDocumentCollection(document, attributes, values, 0, supportUndef, created, errorCode, errorStr);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  // .............................................................................
  // outside write-lock
  // .............................................................................


  // .............................................................................
  // The index is 'new' so save it
  // .............................................................................

  if (idx == NULL) {
    return NULL;
  }

  if (created) {
    int res;

    res = TRI_SaveIndex(primary, idx);

    // ...........................................................................
    // If index could not be saved, report the error and return NULL
    // TODO: get TRI_SaveIndex to report the error
    // ...........................................................................

    if (res == TRI_ERROR_NO_ERROR) {
      return idx;
    }

    *errorCode = res;
    *errorStr  = TRI_DuplicateString("Bitarray index could not be saved.");
    return NULL;
  }

  // .............................................................................
  // Index already exists so simply return it
  // .............................................................................

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                           SELECT BY EXAMPLE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for match of an example
////////////////////////////////////////////////////////////////////////////////

static bool IsExampleMatch (TRI_shaper_t* shaper,
                            TRI_doc_mptr_t const* doc,
                            size_t len,
                            TRI_shape_pid_t* pids,
                            TRI_shaped_json_t** values) {
  TRI_shaped_json_t document;
  TRI_shaped_json_t* example;
  TRI_shaped_json_t result;
  TRI_shape_t const* shape;
  bool ok;
  size_t i;

  TRI_EXTRACT_SHAPED_JSON_MARKER(document, doc->_data);

  for (i = 0;  i < len;  ++i) {
    example = values[i];

    ok = TRI_ExtractShapedJsonVocShaper(shaper,
                                        &document,
                                        example->_sid,
                                        pids[i],
                                        &result,
                                        &shape);

    if (! ok || shape == NULL) {
      return false;
    }

    if (result._data.length != example->_data.length) {
      // suppress excessive log spam
      // LOG_TRACE("expecting length %lu, got length %lu for path %lu",
      //           (unsigned long) result._data.length,
      //           (unsigned long) example->_data.length,
      //           (unsigned long) pids[i]);

      return false;
    }

    if (memcmp(result._data.data, example->_data.data, example->_data.length) != 0) {
      // suppress excessive log spam
      // LOG_TRACE("data mismatch at path %lu", (unsigned long) pids[i]);
      return false;
    }
  }

  return true;
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
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t TRI_SelectByExample (TRI_transaction_collection_t* trxCollection,
                                  size_t length,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values) {
  TRI_shaper_t* shaper;
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** end;
  TRI_vector_t filtered;

  primary = trxCollection->_collection->_collection;

  // use filtered to hold copies of the master pointer
  TRI_InitVector(&filtered, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t));

  // do a full scan
  shaper = primary->_shaper;

  ptr = (TRI_doc_mptr_t const**) (primary->_primaryIndex._table);
  end = (TRI_doc_mptr_t const**) ptr + primary->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (IsVisible(*ptr)) {
      if (IsExampleMatch(shaper, *ptr, length, pids, values)) {
        TRI_PushBackVector(&filtered, *ptr);
      }
    }
  }

  return filtered;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a documet given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                          TRI_doc_update_policy_t const* policy,
                                          TRI_doc_mptr_t* doc) {
  // no extra locking here as the collection is already locked
  return RemoveShapedJson(trxCollection, doc->_key, policy, false, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection revision id
////////////////////////////////////////////////////////////////////////////////

void TRI_SetRevisionDocumentCollection (TRI_document_collection_t* document,
                                        TRI_voc_tick_t tick) {
  TRI_col_info_t* info = &document->base.base._info;
  info->_tick = tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t* document) {
  return RotateJournal(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
