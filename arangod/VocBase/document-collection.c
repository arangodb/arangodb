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
#include "BasicsC/strings.h"
#include "CapConstraint/cap-constraint.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/edge-collection.h"
#include "VocBase/index.h"
#include "VocBase/key-generator.h"
#include "VocBase/marker.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static int InsertPrimaryIndex (TRI_document_collection_t*,
                               TRI_doc_mptr_t const*);

static int InsertSecondaryIndexes (TRI_document_collection_t*,
                                   TRI_doc_mptr_t const*);

static int DeletePrimaryIndex (TRI_document_collection_t*,
                               TRI_doc_mptr_t const*);

static int DeleteSecondaryIndexes (TRI_document_collection_t*,
                                   TRI_doc_mptr_t const*);

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
/// @brief extracts the data length from a master pointer
////////////////////////////////////////////////////////////////////////////////

static size_t LengthDataMasterPointer (const TRI_doc_mptr_t* const mptr) {
  void const* data = mptr->_data;

  if (((TRI_df_marker_t const*) data)->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      ((TRI_df_marker_t const*) data)->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_df_marker_t*) data)->_size;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a header is visible in the current context
////////////////////////////////////////////////////////////////////////////////

static bool IsVisible (TRI_doc_mptr_t const* header,
                       const TRI_doc_operation_context_t* const context) {
  return (header != NULL && header->_validTo == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection revision id with the marker's tick value
////////////////////////////////////////////////////////////////////////////////

static void CollectionRevisionUpdate (TRI_document_collection_t* document,
                                      const TRI_df_marker_t* const marker) {
  TRI_col_info_t* info = &document->base.base._info;

  if (marker->_tick > info->_rid) {
    info->_rid = marker->_tick;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge marker in memory
////////////////////////////////////////////////////////////////////////////////

static int CreateDeletionMarker (TRI_doc_deletion_key_marker_t** result,
                                 TRI_voc_size_t* totalSize,
                                 char* keyBody,
                                 TRI_voc_size_t keyBodySize,
                                 const bool calcCrc) {
  TRI_doc_deletion_key_marker_t* marker;

  *result = NULL;
  *totalSize = sizeof(TRI_doc_deletion_key_marker_t) + keyBodySize + 1;

  marker = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, *totalSize * sizeof(char), false);
  if (marker == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
   
  TRI_InitMarker(&marker->base, TRI_DOC_MARKER_KEY_DELETION, *totalSize, TRI_NewTickVocBase());
  
  marker->_sid = 0;
  marker->_offsetKey = sizeof(TRI_doc_deletion_key_marker_t);

  // copy the key into the marker
  memcpy(((char*) marker) + marker->_offsetKey, keyBody, keyBodySize + 1);
  
  if (calcCrc) {
    TRI_CrcMarker(&marker->base, *totalSize);
  }
  
  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge marker in memory, based on another
/// existing marker
////////////////////////////////////////////////////////////////////////////////

static int CloneDocumentMarker (TRI_df_marker_t const* original,
                                TRI_doc_document_key_marker_t** result,
                                TRI_voc_size_t* totalSize,
                                const TRI_df_marker_type_e markerType,
                                TRI_shaped_json_t const* shaped,
                                const bool calcCrc) {

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
    TRI_ASSERT_DEBUG(baseLength > sizeof(TRI_doc_document_key_marker_t));
  }
  else if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    // edge marker
    TRI_doc_edge_key_marker_t const* o = (TRI_doc_edge_key_marker_t const*) original;
    
    baseLength = o->base._offsetJson;
    TRI_ASSERT_DEBUG(baseLength > sizeof(TRI_doc_edge_key_marker_t));
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

  // copy non-changed data (e.g. key(s)) from old marker into new marker
  memcpy(marker, original, baseLength);
  
  // set the marker type, size, revision id etc.   
  tick = TRI_NewTickVocBase();
  TRI_InitMarker(&marker->base, markerType, *totalSize, tick);
  marker->_rid   = tick;
  marker->_shape = shaped->_sid;

  // copy shaped json into the marker
  memcpy(((char*) marker) + baseLength, (char*) shaped->_data.data, shaped->_data.length);
  
  // no need to adjust _offsetKey, _offsetJson etc. as we copied it from the old marker
  

#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_ASSERT_DEBUG(marker->_offsetKey  == ((TRI_doc_document_key_marker_t const*) original)->_offsetKey);
  TRI_ASSERT_DEBUG(marker->_offsetJson == ((TRI_doc_document_key_marker_t const*) original)->_offsetJson);

  if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* o = (TRI_doc_edge_key_marker_t const*) original;
    TRI_doc_edge_key_marker_t const* c = (TRI_doc_edge_key_marker_t const*) marker;

    TRI_ASSERT_DEBUG(c->_toCid == o->_toCid);
    TRI_ASSERT_DEBUG(c->_fromCid == o->_fromCid);
    TRI_ASSERT_DEBUG(c->_offsetToKey == o->_offsetToKey);
    TRI_ASSERT_DEBUG(c->_offsetFromKey == o->_offsetFromKey);
  }
#endif
  
  if (calcCrc) {
    // calculate crc checksum
    TRI_CrcMarker(&marker->base, *totalSize);
  }

  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge marker in memory
////////////////////////////////////////////////////////////////////////////////

static int CreateDocumentMarker (TRI_primary_collection_t* primary,
                                 TRI_doc_document_key_marker_t** result,
                                 TRI_voc_size_t* totalSize,
                                 char** keyBody, 
                                 const TRI_df_marker_type_e markerType,
                                 TRI_voc_key_t key,
                                 TRI_shaped_json_t const* shaped, 
                                 void const* data,
                                 const bool calcCrc) {
  TRI_doc_document_key_marker_t* marker;
  TRI_key_generator_t* keyGenerator;
  char* position;
  char keyBuffer[TRI_VOC_KEY_MAX_LENGTH + 1]; 
  TRI_voc_size_t keyBodySize;
  TRI_voc_tick_t tick;
  size_t markerSize;
  size_t keySize;
  size_t fromSize;
  size_t toSize;
  int res;

  *result = NULL;
  tick = TRI_NewTickVocBase();

  // generate the key
  keyGenerator = (TRI_key_generator_t*) primary->_keyGenerator;
  TRI_ASSERT_DEBUG(keyGenerator != NULL);
  
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
  TRI_InitMarker(&marker->base, markerType, *totalSize, tick);
  marker->_rid   = tick; 
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
  
  if (calcCrc) {
    // calculate crc checksum
    TRI_CrcMarker(&marker->base, *totalSize);
  }

  *result = marker;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates and initially populates a document master pointer
////////////////////////////////////////////////////////////////////////////////

static int CreateHeader (TRI_document_collection_t* document,
                         TRI_doc_document_key_marker_t* marker,
                         TRI_voc_fid_t fid,
                         TRI_doc_mptr_t** result) {
  TRI_doc_mptr_t* header;
  TRI_voc_tick_t tick;

  // get a new header pointer
  header = document->_headers->request(document->_headers);

  if (header == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  tick = marker->base._tick;

  header->_rid       = tick;
  header->_fid       = fid;
  header->_validFrom = tick;     // document creation time
  header->_validTo   = 0;        // document deletion time, 0 means "infinitely valid"
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

  if (base->_maximumMarkerSize < size) {
    base->_maximumMarkerSize = size;
  }

  while (base->_state == TRI_COL_STATE_WRITE) {
    n = base->_journals._length;

    for (i = 0;  i < n;  ++i) {
      // select datafile
      datafile = base->_journals._buffer[i];

      // try to reserve space
      res = TRI_ReserveElementDatafile(datafile, size, result);

      // in case of full datafile, try next
      if (res == TRI_ERROR_NO_ERROR) {
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

        return datafile;
      }
      else if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
        // some other error
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

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
/// @brief writes data to the journal and updates the barriers
////////////////////////////////////////////////////////////////////////////////

static int WriteElement (TRI_document_collection_t* document,
                         TRI_datafile_t* journal,
                         TRI_df_marker_t const* marker,
                         TRI_voc_size_t markerSize,
                         void const* keyBody,
                         TRI_voc_size_t keyBodySize,
                         void const* body,
                         TRI_voc_size_t bodySize,
                         TRI_df_marker_t* result) {
  int res;

  res = TRI_WriteElementDatafile(journal,
                                 result,
                                 marker,
                                 markerSize,
                                 keyBody,
                                 keyBodySize,
                                 body,
                                 bodySize,
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  CollectionRevisionUpdate(document, marker);

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
/// @brief inserts a document into indexes and datafile
///
/// when this function is called, the write-lock on the indexes & documents for
/// the collection must be held
////////////////////////////////////////////////////////////////////////////////

static int InsertDocument (TRI_document_collection_t* document,
                           TRI_doc_document_key_marker_t* marker,
                           TRI_doc_mptr_t* header,
                           TRI_voc_size_t totalSize,
                           bool forceSync,
                           TRI_doc_mptr_t* mptr) {

  TRI_datafile_t* journal;
  TRI_df_marker_t* result;
  int res;

  TRI_ASSERT_DEBUG(header != NULL);

  // .............................................................................
  // insert into indexes
  // .............................................................................

  // insert into primary index first
  res = InsertPrimaryIndex(document, header);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = InsertSecondaryIndexes(document, header);

  if (res != TRI_ERROR_NO_ERROR) {
    // insertion into secondary indexes failed
    DeletePrimaryIndex(document, header);

    return res;
  }

  // insert into indexes has succeeded

  
  // .............................................................................
  // insert into datafile
  // .............................................................................
  
  // find and select a journal
  journal = SelectJournal(document, totalSize, &result);

  if (journal == NULL) {
    res = TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // now write marker and blob
    res = WriteElement(document, journal, &marker->base, totalSize, NULL, 0, NULL, 0, result);

    if (res == TRI_ERROR_NO_ERROR) {
      // writing the element into the datafile has succeeded
      TRI_doc_datafile_info_t* dfi;
      size_t i, n;

      // update the header with the correct fid and the positions in the datafile
      header->_fid    = journal->_fid;
      header->_data   = ((char*) result);
      header->_key    = ((char*) result) + marker->_offsetKey;  

      *mptr = *header;

      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(&document->base, journal->_fid);

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += LengthDataMasterPointer(mptr);
      }

      // .............................................................................
      // post process insert
      // .............................................................................

      n = document->_allIndexes._length;

      for (i = 0;  i < n;  ++i) {
        TRI_index_t* idx;

        idx = document->_allIndexes._buffer[i];

        if (idx->postInsert != NULL) {
          idx->postInsert(idx, header);
        }
      }

      // TODO: does the sync need to be inside the lock?? 
      if (forceSync) {
        WaitSync(document, journal, ((char const*) result) + totalSize);
      }
    }
    else {
      // writing the element into the datafile has failed
      LOG_ERROR("cannot write element into datafile: '%s'", TRI_last_error());
    }
  }


  // something has failed.... now delete from the indexes again

  if (res != TRI_ERROR_NO_ERROR) {
    // some error has occurred
    DeleteSecondaryIndexes(document, header);
    DeletePrimaryIndex(document, header);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document from the indexes and datafile
///
/// when this function is called, the write-lock on the indexes & documents for
/// the collection must be held
////////////////////////////////////////////////////////////////////////////////

static int DeleteDocument (TRI_doc_operation_context_t* context,
                           TRI_doc_deletion_key_marker_t* marker,
                           const TRI_voc_size_t totalSize,
                           const bool forceSync) {
  TRI_primary_collection_t* primary;
  TRI_document_collection_t* document;
  TRI_doc_mptr_t* header;
  TRI_df_marker_t* result;
  TRI_datafile_t* journal;
  TRI_doc_datafile_info_t* dfi;
  size_t i, n;
  int res;

  primary = context->_collection;
  document = (TRI_document_collection_t*) primary;

  // get the existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, ((char*) marker) + marker->_offsetKey);

  if (! IsVisible(header, context)) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // .............................................................................
  // check the revision
  // .............................................................................

  res = TRI_RevisionCheck(context, header->_rid);
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
 
  
  // find and select a journal
  journal = SelectJournal(document, totalSize, &result);

  if (journal == NULL) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  TRI_ASSERT_DEBUG(result != NULL);

  // and write marker and blob
  res = WriteElement(document, journal, &marker->base, totalSize, NULL, 0, NULL, 0, result);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot delete element");

    return res;
  }
  
  // delete from indexes
  res = DeleteSecondaryIndexes(document, header);
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("deleting document from indexes failed");

    // TODO: though delete should never fail, if it does, the collection is left
    // in an inconsistent state:
    // there will be the deletion marker in the datafile, but the index will still contain the document

    return res;
  }
  
  res = DeletePrimaryIndex(document, header);
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("deleting document from indexes failed");
    
    // TODO: though delete should never fail, if it does, the collection is left
    // in an inconsistent state:
    // there will be the deletion marker in the datafile, but the index will still contain the document

    return res;
  }


  // update the datafile info
  dfi = TRI_FindDatafileInfoPrimaryCollection(primary, header->_fid);
  if (dfi != NULL) {
    size_t length = LengthDataMasterPointer(header);

    dfi->_numberAlive -= 1;
    dfi->_sizeAlive -= length;

    dfi->_numberDead += 1;
    dfi->_sizeDead += length;
  }

  if (header->_fid != journal->_fid) {
    // only need to look up datafile if it is not the same
    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, journal->_fid);
  }

  if (dfi != NULL) {
    dfi->_numberDeletion += 1;
  }


  // .............................................................................
  // post process delete
  // .............................................................................

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->postRemove != NULL) {
      idx->postRemove(idx, header);
    }
  }
  
  // and release the header pointer
  document->_headers->release(document->_headers, header);

  // .............................................................................
  // wait for sync
  // .............................................................................

  if (context->_sync) {
    WaitSync(document, journal, ((char const*) result) + totalSize);
  }

  return TRI_ERROR_NO_ERROR;
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

  newHeader->_rid  = marker->_rid;
  newHeader->_fid  = fid;
  newHeader->_data = marker;
  newHeader->_key  = ((char*) marker) + marker->_offsetKey;

  newHeader->_validFrom = oldHeader->_validFrom;
  newHeader->_validTo   = oldHeader->_validTo; // TODO: fix for trx
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static int UpdateDocument (TRI_document_collection_t* document,
                           TRI_doc_mptr_t* oldHeader,
                           TRI_doc_document_key_marker_t const* marker,
                           const TRI_voc_size_t totalSize,
                           const bool forceSync,
                           TRI_doc_mptr_t* mptr) {
  
  TRI_datafile_t* journal;
  TRI_doc_mptr_t* newHeader;
  TRI_doc_mptr_t oldData;
  TRI_df_marker_t* result;
  int res;
 
  // save the old data, remember
  oldData = *oldHeader;
  
  // .............................................................................
  // update indexes
  // .............................................................................

  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)

  res = DeleteSecondaryIndexes(document, oldHeader);

  if (res != TRI_ERROR_NO_ERROR) {
    // re-enter the document in case of failure, ignore errors during rollback
    int resRollback;

    resRollback = InsertSecondaryIndexes(document, oldHeader);

    if (resRollback != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("encountered error '%s' during rollback of update", TRI_errno_string(resRollback));
    }

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
  res = InsertSecondaryIndexes(document, newHeader);

  if (res != TRI_ERROR_NO_ERROR) {
    // rollback
    int resRollback;

    resRollback = DeleteSecondaryIndexes(document, newHeader);

    if (resRollback != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("encountered error '%s' during rollback of update", TRI_errno_string(resRollback));
    }
    
    // copy back old header data
    *oldHeader = oldData;

    resRollback = InsertSecondaryIndexes(document, oldHeader);
    
    if (resRollback != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("encountered error '%s' during rollback of update", TRI_errno_string(resRollback));
    }

    return res;
  }

  
  // .............................................................................
  // write datafile
  // .............................................................................

  
  // find and select a journal
  journal = SelectJournal(document, totalSize, &result);
  
  if (journal == NULL) {
    res = TRI_ERROR_ARANGO_NO_JOURNAL;
  }


  if (res == TRI_ERROR_NO_ERROR) {
    res = WriteElement(document, journal, &marker->base, totalSize, NULL, 0, NULL, 0, result);

    if (res == TRI_ERROR_NO_ERROR) {
      TRI_doc_datafile_info_t* dfi;
      TRI_primary_collection_t* primary;
      size_t i, n;
      
      
      // update the header with the correct fid and the positions in the datafile
      newHeader->_fid    = journal->_fid;
      newHeader->_data   = ((char*) result);
      newHeader->_key    = ((char*) result) + marker->_offsetKey;  

      primary = (TRI_primary_collection_t*) document;

      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, oldData._fid);

      if (dfi != NULL) {
        size_t length = LengthDataMasterPointer(&oldData);

        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= length;
        dfi->_numberDead += 1;
        dfi->_sizeDead += length;
      }

      if (oldData._fid != journal->_fid) {
        // only select new journal if it different from the old
        dfi = TRI_FindDatafileInfoPrimaryCollection(primary, journal->_fid);
      }

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += LengthDataMasterPointer(newHeader);
      }
  
      // .............................................................................
      // post process update
      // .............................................................................

      n = document->_allIndexes._length;

      for (i = 0;  i < n;  ++i) {
        TRI_index_t* idx;

        idx = document->_allIndexes._buffer[i];

        if (idx->postUpdate != NULL) {
          idx->postUpdate(idx, newHeader, oldHeader, &oldData);
        }
      }

      // wait for sync
      if (forceSync) {
        WaitSync(document, journal, ((char const*) result) + totalSize);
      }

      // write new header into result  
      *mptr = *((TRI_doc_mptr_t*) newHeader);

      TRI_ASSERT_DEBUG(res == TRI_ERROR_NO_ERROR);
    }
    else {
      // writing the element into the datafile has failed
      LOG_ERROR("cannot write element into datafile: '%s'", TRI_last_error());
    }
  }


  if (res != TRI_ERROR_NO_ERROR) {
    // rollback index insertion
    int resRollback;

    resRollback = DeleteSecondaryIndexes(document, newHeader);
    if (resRollback != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("encountered error '%s' during rollback of update", TRI_errno_string(resRollback));
    }
    
    // copy back old header data
    *oldHeader = oldData;

    resRollback = InsertSecondaryIndexes(document, oldHeader);
    if (resRollback != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("encountered error '%s' during rollback of update", TRI_errno_string(resRollback));
    }
  }
    

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
            TRI_TypeNameIndex(idx),
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
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoDatafile (TRI_primary_collection_t* primary,
                                       TRI_datafile_t* datafile) {
  TRI_doc_datafile_info_t* dfi;

  dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

  printf("DATAFILE '%s'\n", datafile->_filename);

  if (dfi == NULL) {
    printf(" no info\n\n");
    return;
  }

  printf("  number alive: %ld\n", (long) dfi->_numberAlive);
  printf("  size alive:   %ld\n", (long) dfi->_sizeAlive);
  printf("  number dead:  %ld\n", (long) dfi->_numberDead);
  printf("  size dead:    %ld\n", (long) dfi->_sizeDead);
  printf("  deletion:     %ld\n\n", (long) dfi->_numberDeletion);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoPrimaryCollection (TRI_primary_collection_t* collection) {
  TRI_datafile_t* datafile;
  size_t n;
  size_t i;

  // journals
  n = collection->base._journals._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->base._journals._buffer[i];
    DebugDatafileInfoDatafile(collection, datafile);
  }

  // compactor journals
  n = collection->base._compactors._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->base._compactors._buffer[i];
    DebugDatafileInfoDatafile(collection, datafile);
  }

  // datafiles
  n = collection->base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    datafile = collection->base._datafiles._buffer[i];
    DebugDatafileInfoDatafile(collection, datafile);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugHeaderDocumentCollection (TRI_document_collection_t* collection) {
  TRI_primary_collection_t* primary;
  void** end;
  void** ptr;

  primary = &collection->base;

  ptr = primary->_primaryIndex._table;
  end = ptr + primary->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_doc_mptr_t* d;

      d = *ptr;

      printf("fid %llu, key %s, rid %llu, validFrom: %llu validTo %llu\n",
             (unsigned long long) d->_fid,
             (char*) d->_key,
             (unsigned long long) d->_rid,
             (unsigned long long) d->_validFrom,
             (unsigned long long) d->_validTo);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document into the collection
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

static int InsertShapedJson (TRI_doc_operation_context_t* context,
                             const TRI_voc_key_t key,
                             TRI_doc_mptr_t* mptr,
                             TRI_df_marker_type_e markerType,
                             TRI_shaped_json_t const* shaped,
                             void const* data,
                             const bool lock,
                             const bool forceSync) {

  TRI_primary_collection_t* primary;
  TRI_document_collection_t* document;
  TRI_doc_document_key_marker_t* marker;
  TRI_doc_mptr_t* header;
  char* keyBody;
  TRI_voc_size_t totalSize;
  int res;

  primary = context->_collection;

  TRI_ASSERT_DEBUG(primary != NULL);
  TRI_ASSERT_DEBUG(shaped != NULL);

  // first create a new marker in memory
  // this does not require any locks

  // TODO: make calcCrc flag (last parameter) dynamic, based on the collection properties
  res = CreateDocumentMarker(primary, &marker, &totalSize, &keyBody, markerType, key, shaped, data, true);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
          
  TRI_ASSERT_DEBUG(marker != NULL);
  TRI_ASSERT_DEBUG(keyBody != NULL);
  TRI_ASSERT_DEBUG(totalSize > 0);

  document = (TRI_document_collection_t*) primary;
  
  if (lock) {
    // WRITE-LOCK START
    primary->beginWrite(primary);
  }

  header = NULL;
  res = CreateHeader(document, marker, 0, &header);

  if (res == TRI_ERROR_NO_ERROR) {
    res = InsertDocument(document, marker, header, totalSize, forceSync, mptr);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // something has failed... free the header
    document->_headers->release(document->_headers, header);
  }
    
  if (lock) {
    primary->endWrite(primary);
    // WRITE-LOCK END
  }
        
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

static int ReadShapedJson (TRI_doc_operation_context_t* context,
                           const TRI_voc_key_t key,
                           TRI_doc_mptr_t* mptr) {
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t const* header;

  primary = context->_collection;

  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  if (! IsVisible(header, context)) {
    // make an empty result
    memset(mptr, 0, sizeof(TRI_doc_mptr_t));

    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // we found a document, now copy it over
  *mptr = *((TRI_doc_mptr_t*) header);

  return TRI_ERROR_NO_ERROR;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static int UpdateShapedJson (TRI_doc_operation_context_t* context,
                             const TRI_voc_key_t key,
                             TRI_doc_mptr_t* mptr,
                             TRI_shaped_json_t const* shaped,
                             const bool lock,
                             const bool forceSync) {
  TRI_primary_collection_t* primary;
  TRI_doc_document_key_marker_t* marker;
  TRI_doc_mptr_t* header;
  TRI_voc_size_t totalSize;
  int res;
  
  TRI_ASSERT_DEBUG(mptr != NULL);
  
  // initialise the result
  mptr->_key = NULL;
  mptr->_data = NULL;
  
  marker = NULL;  

  primary = context->_collection;

  if (lock) {
    primary->beginWrite(primary);
  }

  TRI_ASSERT_DEBUG(key != NULL);

  // get the header pointer of the previous revision
  header = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

  if (IsVisible(header, context)) {
    // document found, now check revision
    res = TRI_RevisionCheck(context, header->_rid);
  }
  else {
    // document not found
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }


  if (res == TRI_ERROR_NO_ERROR) {
    TRI_df_marker_t const* original;

    original = header->_data;

    // TODO: make calcCrc flag (last parameter) dynamic, based on the collection properties
    res = CloneDocumentMarker(original, &marker, &totalSize, original->_type, shaped, true);

    if (res == TRI_ERROR_NO_ERROR) {
      res = UpdateDocument((TRI_document_collection_t*) primary, header, marker, totalSize, forceSync, mptr);
    }
  }

  if (lock) {
    primary->endWrite(primary);
  }

  if (marker != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static int DeleteShapedJson (TRI_doc_operation_context_t* context,
                             const TRI_voc_key_t key,
                             const bool lock,
                             const bool forceSync) {
  TRI_primary_collection_t* primary;
  TRI_doc_deletion_key_marker_t* marker;
  TRI_voc_size_t totalSize;
  int res;
  
  TRI_ASSERT_DEBUG(key != NULL);

  // TODO: make calcCrc dynamic
  marker = NULL;
  res = CreateDeletionMarker(&marker, &totalSize, key, strlen(key), true);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT_DEBUG(marker != NULL);

  primary = context->_collection;
  
  if (lock) {
    primary->beginWrite(primary);
  }

  res = DeleteDocument(context, marker, totalSize, forceSync);

  if (lock) {
    primary->endWrite(primary);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, marker);

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
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator (TRI_df_marker_t const* marker, void* data, TRI_datafile_t* datafile, bool journal) {
  TRI_document_collection_t* collection = data;
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t const* found;
  TRI_doc_datafile_info_t* dfi;
  TRI_key_generator_t* keyGenerator;
  TRI_voc_key_t key = NULL;

  primary = &collection->base;
  keyGenerator = primary->_keyGenerator;

  CollectionRevisionUpdate(collection, marker);

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* d = (TRI_doc_document_key_marker_t const*) marker;
    size_t markerSize;

    if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {

      LOG_TRACE("document: fid %llu, key %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
                (unsigned long long) datafile->_fid,
                ((char*) d + d->_offsetKey),
                (unsigned long long) d->_rid,
                (unsigned long) d->_offsetJson,
                (unsigned long) d->_offsetKey);

      markerSize = sizeof(TRI_doc_document_key_marker_t);
      key = ((char*) d) + d->_offsetKey;
    }
    else {

#ifdef TRI_ENABLE_LOGGER
      TRI_doc_edge_key_marker_t const* e = (TRI_doc_edge_key_marker_t const*) marker;

      LOG_TRACE("edge: fid %llu, key %s, fromKey %s, toKey %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
                (unsigned long long) datafile->_fid,
                ((char*) d + d->_offsetKey),
                ((char*) e + e->_offsetFromKey),
                ((char*) e + e->_offsetToKey),
                (unsigned long long) d->_rid,
                (unsigned long) d->_offsetJson,
                (unsigned long) d->_offsetKey);
#endif
      markerSize = sizeof(TRI_doc_edge_key_marker_t);
      key = ((char*) d) + d->_offsetKey;
    }

    if (primary->base._maximumMarkerSize < markerSize) {
      primary->base._maximumMarkerSize = markerSize;
    }

    if (keyGenerator->track != NULL) {
      keyGenerator->track(keyGenerator, key);
    }

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

    // it is a new entry
    if (found == NULL) {
      TRI_doc_mptr_t* header;
      int res;

      // get a header
      res = CreateHeader(collection, (TRI_doc_document_key_marker_t*) marker, datafile->_fid, &header);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("out of memory");
        TRI_set_errno(res);

        return false;
      }

      TRI_ASSERT_DEBUG(header != NULL);

      // insert into primary index
      res = InsertPrimaryIndex(collection, header);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");

        collection->_headers->release(collection->_headers, header);
        return false;
      }

      res = InsertSecondaryIndexes(collection, header);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");

        DeletePrimaryIndex(collection, header);

        collection->_headers->release(collection->_headers, header);
        return false;
      }
      
      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += LengthDataMasterPointer(header);
      }
    }

    // it is an update, but only if found has a smaller revision identifier
    else if (found->_rid < d->_rid || (found->_rid == d->_rid && found->_fid <= datafile->_fid)) {
      TRI_doc_mptr_t* newHeader;
      TRI_doc_mptr_t oldData;
      int res;
      
      // save the old data
      oldData = *found;

      // delete old entries
      DeleteSecondaryIndexes(collection, found);

      // TODO: this will be identical for non-transactional collections only
      newHeader = CONST_CAST(found);

      // update the header info
      UpdateHeader(datafile->_fid, marker, newHeader, found);
      newHeader->_validTo = 0;
      

      // update secondary indexes
      res = InsertSecondaryIndexes(collection, found);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");

        // revert the changes
        DeleteSecondaryIndexes(collection, found);

        // copy the old data back into the header
        memcpy(newHeader, &oldData, sizeof(TRI_doc_mptr_t));

        // and re-insert the old header
        InsertSecondaryIndexes(collection, found);
        
        return false;
      }

      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, found->_fid);

      if (dfi != NULL) {
        size_t length = LengthDataMasterPointer(found);

/*
        issue #411: if we decrease here, the counts might get negative!
        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= length;
*/

        dfi->_numberDead += 1;
        dfi->_sizeDead += length;
      }

      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += LengthDataMasterPointer(newHeader);
      }
    }

    // it is a delete
    else if (found->_validTo != 0) {
      // TODO: fix for trx: check if delete was committed or not
      LOG_TRACE("skipping already deleted document: %s", key);
    }

    // it is a stale update
    else {
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberDead += 1;
        dfi->_sizeDead += LengthDataMasterPointer(found);
      }
    }
  }
  // deletion
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* d;

    d = (TRI_doc_deletion_key_marker_t const*) marker;
    key = ((char*) d) + d->_offsetKey;

    LOG_TRACE("deletion: fid %llu, key %s, rid %llu, deletion %llu",
              (unsigned long long) datafile->_fid,
              (char*) key,
              (unsigned long long) d->_rid,
              (unsigned long long) marker->_tick);

    if (keyGenerator->track != NULL) {
      keyGenerator->track(keyGenerator, key);
    }

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);

    // it is a new entry, so we missed the create
    if (found == NULL) {
      TRI_doc_mptr_t* header;
      int res;

      header = collection->_headers->request(collection->_headers);
      if (header == NULL) {
        LOG_ERROR("out of memory");
        return false;
      }

      header->_rid       = d->_rid;
      header->_validFrom = marker->_tick;
      header->_validTo   = marker->_tick; // TODO: fix for trx
      header->_data      = marker;
      header->_key       = key;

      // insert into indexes
      res = InsertPrimaryIndex(collection, header);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");

        collection->_headers->release(collection->_headers, header);
        return false;
      }

      res = InsertSecondaryIndexes(collection, header);
      
      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");

        DeletePrimaryIndex(collection, header);

        collection->_headers->release(collection->_headers, header);
        return false;
      }

      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberDeletion += 1;
      }
    }

    // it is a real delete
    else if (found->_validTo == 0) {
      TRI_doc_mptr_t* newHeader;
      
      newHeader = CONST_CAST(found);

      // mark element as deleted
      newHeader->_validFrom = marker->_tick;
      newHeader->_validTo   = marker->_tick; // TODO: fix for trx
      newHeader->_data      = marker;
      newHeader->_key       = key;

      // update the datafile info
      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, found->_fid);

      if (dfi != NULL) {
        size_t length = LengthDataMasterPointer(found);

        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= length;

        dfi->_numberDead += 1;
        dfi->_sizeDead += length;
      }

      dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberDeletion += 1;
      }
    }

    // it is a double delete
    else {
      LOG_TRACE("skipping deletion of already deleted document: %s", (char*) key);
    }
  }
  else {
    LOG_TRACE("skipping marker type %lu", (unsigned long) marker->_type);
  }

  return true;
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

  // load json description of the index
  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  // json must be a index description
  if (json == NULL) {
    LOG_ERROR("cannot read index definition from '%s': %s", filename, error);
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
    iid = iis->_value._number;
    TRI_UpdateTickVocBase(iid);
  }
  else {
    LOG_ERROR("ignoring index, index identifier could not be located");
    return false;
  }

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

  collection->_headers = TRI_CreateSimpleHeaders(sizeof(TRI_doc_mptr_t));
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
  collection->base.beginRead  = BeginRead;
  collection->base.endRead    = EndRead;

  collection->base.beginWrite = BeginWrite;
  collection->base.endWrite   = EndWrite;

  collection->base.insert     = InsertShapedJson;
  collection->base.read       = ReadShapedJson;
  collection->base.update     = UpdateShapedJson;
  collection->base.destroy    = DeleteShapedJson;

  collection->cleanupIndexes  = CleanupIndexes;

  return true;
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
    LOG_ERROR("cannot create document collection");
    return NULL;
  }

  TRI_ASSERT_DEBUG(keyGenerator != NULL);


  // first create the document collection
  document = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_document_collection_t), false);

  if (document == NULL) {
    TRI_FreeKeyGenerator(keyGenerator);
    LOG_ERROR("cannot create document collection");
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

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t* vocbase, char const* path) {
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

  TRI_ASSERT_DEBUG(keyGenerator != NULL);
  document->base._keyGenerator = keyGenerator;


  shapeCollection = TRI_CollectionVocShaper(shaper);
  if (shapeCollection != NULL) {
    shapeCollection->base._info._waitForSync = (vocbase->_forceSyncShapes || collection->_info._waitForSync);
  }


  // read all documents and fill indexes
  TRI_IterateCollection(collection, OpenIterator, collection);

  if (collection->_info._maximalSize < collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD) {
    LOG_WARNING("maximal size is %lu, but maximal marker size is %lu plus overhead %lu: adjusting maximal size to %lu",
                (unsigned long) collection->_info._maximalSize,
                (unsigned long) collection->_maximumMarkerSize,
                (unsigned long) TRI_JOURNAL_OVERHEAD,
                (unsigned long) (collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD));

    collection->_info._maximalSize = collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD;
  }

  TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);

  // output information about datafiles and journals
  if (TRI_IsTraceLogging(__FILE__)) {
    DebugDatafileInfoPrimaryCollection(&document->base);
    DebugHeaderDocumentCollection(document);
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
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header) {
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t* found;

  TRI_ASSERT_DEBUG(document != NULL);
  TRI_ASSERT_DEBUG(header != NULL);
  TRI_ASSERT_DEBUG(header->_key != NULL);
  
  if (header->_validTo != 0) {
    // don't insert in case the document is deleted
    return TRI_ERROR_NO_ERROR;
  }

  primary = &document->base;

  // add a new header
  found = TRI_InsertKeyAssociativePointer(&primary->_primaryIndex, header->_key, (void*) header, false);

  // TODO: if TRI_InsertKeyAssociativePointer fails with OOM, it returns NULL.
  // in case the call succeeds but does not find any previous value, it also returns NULL
  // this function here will continue happily in both cases.
  // These two cases must be distinguishable in order to notify the caller about an error

  if (found == NULL) {
    // success
    return TRI_ERROR_NO_ERROR;
  }

  // we found a previous revision in the index
  if (found->_validTo == 0) {
    // the found revision is still alive
    LOG_TRACE("document '%s' already existed with revision %llu while creating revision %llu",
              header->_key,
              (unsigned long long) found->_rid,
              (unsigned long long) header->_rid);

    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  // a deleted document was found in the index. now insert again and overwrite
  // this should be an exceptional case
  found = TRI_InsertKeyAssociativePointer(&primary->_primaryIndex, header->_key, (void*) header, true);

  // overwriting does not change the size of the index and should always succeed
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int InsertSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header) {
  size_t i, n;
  int result;

  result = TRI_ERROR_NO_ERROR;
  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = document->_allIndexes._buffer[i];
    res = idx->insert(idx, header);

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
                               TRI_doc_mptr_t const* header) {
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

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int DeleteSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header) {
  size_t i, n;
  int result;

  n = document->_allIndexes._length;
  result = TRI_ERROR_NO_ERROR;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = document->_allIndexes._buffer[i];
    res = idx->remove(idx, header);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static int FillIndex (TRI_document_collection_t* document, TRI_index_t* idx) {
  TRI_doc_mptr_t const* mptr;
  TRI_primary_collection_t* primary;
  TRI_doc_operation_context_t context;
  uint64_t inserted;
  void** end;
  void** ptr;
  int res;

  primary = &document->base;

  TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_LAST_WRITE, false);

  // update index
  ptr = primary->_primaryIndex._table;
  end = ptr + primary->_primaryIndex._nrAlloc;

  inserted = 0;

  for (;  ptr < end;  ++ptr) {
    if (IsVisible(*ptr, &context)) {
      mptr = *ptr;

      res = idx->insert(idx, mptr);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("failed to insert document '%llu/%s' for index %llu",
                    (unsigned long long) primary->base._info._cid,
                    (char*) mptr->_key,
                    (unsigned long long) idx->_iid);

        return res;
      }

      ++inserted;

      if (inserted % 10000 == 0) {
        LOG_DEBUG("indexed %llu documents of collection %llu", (unsigned long long) inserted, (unsigned long long) primary->base._info._cid);
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

      if (found != NULL) {
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
                                                           size_t size,
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
    if (primary->_capConstraint->_size == size) {
      return &primary->_capConstraint->base;
    }
    else {
      TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
      return NULL;
    }
  }

  // create a new index
  idx = TRI_CreateCapConstraint(primary, size);

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
  TRI_json_t* num;
  TRI_index_t* idx;
  size_t size;

  num = TRI_LookupArrayJson(definition, "size");

  if (num == NULL || num->_type != TRI_JSON_NUMBER) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' missing", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  if (num->_value._number < 1.0) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' %f must be at least 1", (unsigned long long) iid, num->_value._number);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  size = (size_t) num->_value._number;

  idx = CreateCapConstraintDocumentCollection(document, size, iid, NULL);

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
                                                        size_t size,
                                                        bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateCapConstraintDocumentCollection(document, size, 0, created);

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
                                                      bool constraint,
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
    idx = TRI_LookupGeoIndex1DocumentCollection(document, loc, geoJson, constraint, ignoreNull);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, lat, lon, constraint, ignoreNull);
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
    idx = TRI_CreateGeo1Index(primary, location, loc, geoJson, constraint, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeo2Index(primary, latitude, lat, longitude, lon, constraint, ignoreNull);

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
  bool constraint;
  bool ignoreNull;
  char const* typeStr;
  size_t fieldCount;

  typeStr = TRI_LookupArrayJson(definition, "type")->_value._string.data;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract constraint
  constraint = false;
  bv = TRI_LookupArrayJson(definition, "constraint");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    constraint = bv->_value._boolean;
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
                                        constraint,
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
                                        constraint,
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
                                                    bool constraint,
                                                    bool ignoreNull) {
  size_t n;
  size_t i;

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO1_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_location != 0 && geo->_location == location && geo->_geoJson == geoJson && geo->_constraint == constraint) {
        if (! constraint || geo->base._ignoreNull == ignoreNull) {
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
                                                    bool constraint,
                                                    bool ignoreNull) {
  size_t n;
  size_t i;

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO2_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_latitude != 0 && geo->_longitude != 0 && geo->_latitude == latitude && geo->_longitude == longitude && geo->_constraint == constraint) {
        if (! constraint || geo->base._ignoreNull == ignoreNull) {
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
                                                    bool constraint,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateGeoIndexDocumentCollection(document, location, NULL, NULL, geoJson, constraint, ignoreNull, 0, created);

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
                                                    bool constraint,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;
  TRI_primary_collection_t* primary;

  primary = &document->base;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  idx = CreateGeoIndexDocumentCollection(document, NULL, latitude, longitude, false, constraint, ignoreNull, 0, created);

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

TRI_vector_t TRI_SelectByExample (TRI_doc_operation_context_t* context,
                                  size_t length,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values) {
  TRI_shaper_t* shaper;
  TRI_primary_collection_t* primary;
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** end;
  TRI_vector_t filtered;

  primary = context->_collection;

  // use filtered to hold copies of the master pointer
  TRI_InitVector(&filtered, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t));

  // do a full scan
  shaper = primary->_shaper;

  ptr = (TRI_doc_mptr_t const**) (primary->_primaryIndex._table);
  end = (TRI_doc_mptr_t const**) ptr + primary->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (IsVisible(*ptr, context)) {
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

int TRI_DeleteDocumentDocumentCollection (TRI_doc_operation_context_t* context,
                                          TRI_doc_mptr_t* doc) {
  // no extra locking here as the collection is already locked
  return DeleteShapedJson(context, doc->_key, false, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
