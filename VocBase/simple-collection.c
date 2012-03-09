////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "simple-collection.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "ShapedJson/shape-accessor.h"
#include "VocBase/index.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t* header);

static bool UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_doc_mptr_t const* update);

static bool DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_voc_tick_t);

static TRI_index_t* CreateGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                                 char const* location,
                                                 char const* latitude,
                                                 char const* longitude,
                                                 bool geoJson,
                                                 TRI_idx_iid_t iid);

static TRI_index_t* CreateHashIndexSimCollection (TRI_sim_collection_t* collection,
                                                  const TRI_vector_t* attributes,
                                                  TRI_idx_iid_t iid,
                                                  bool unique);
                                                 
static TRI_index_t* CreateSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                      const TRI_vector_t* attributes,
                                                      TRI_idx_iid_t iid,
                                                      bool unique);
                                                 
static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key);

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element);

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element);

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

static TRI_datafile_t* SelectJournal (TRI_sim_collection_t* collection,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  bool ok;
  size_t i;
  size_t n;

  TRI_LockCondition(&collection->_journalsCondition);

  while (true) {
    n = collection->base.base._journals._length;

    for (i = 0;  i < n;  ++i) {

      // select datafile
      datafile = collection->base.base._journals._buffer[i];

      // try to reserve space
      ok = TRI_ReserveElementDatafile(datafile, size, result);

      // in case of full datafile, try next
      if (ok) {
        TRI_UnlockCondition(&collection->_journalsCondition);
        return datafile;
      }
      else if (! ok && TRI_errno() != TRI_VOC_ERROR_DATAFILE_FULL) {
        TRI_UnlockCondition(&collection->_journalsCondition);
        return NULL;
      }
    }

    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for synchronisation
////////////////////////////////////////////////////////////////////////////////

static void WaitSync (TRI_sim_collection_t* collection,
                      TRI_datafile_t* journal,
                      char const* position) {
  TRI_collection_t* base;
  bool done;

  base = &collection->base.base;

  // no condition at all
  if (0 == base->_syncAfterObjects && 0 == base->_syncAfterBytes && 0 == base->_syncAfterTime) {
    return;
  }

  TRI_LockCondition(&collection->_journalsCondition);

  // wait until the sync condition is fullfilled
  while (true) {
    done = true;

    // check for error
    if (journal->_state == TRI_DF_STATE_WRITE_ERROR) {
      break;
    }

    // always sync
    if (1 == base->_syncAfterObjects) {
      if (journal->_synced < position) {
        done = false;
      }
    }

    // at most that many outstanding objects
    else if (1 < base->_syncAfterObjects) {
      if (journal->_nWritten - journal->_nSynced < base->_syncAfterObjects) {
        done = false;
      }
    }

    // at most that many outstanding bytes
    if (0 < base->_syncAfterBytes) {
      if (journal->_written - journal->_synced < base->_syncAfterBytes) {
        done = false;
      }
    }

    // at most that many seconds
    if (0 < base->_syncAfterTime && journal->_synced < journal->_written) {
      double t = TRI_microtime();

      if (journal->_lastSynced < t - base->_syncAfterTime) {
        done = false;
      }
    }

    // stop waiting of we reached our limits
    if (done) {
      break;
    }

    // we have to wait a bit longer
    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes data to the journal and updates the barriers
////////////////////////////////////////////////////////////////////////////////

static bool WriteElement (TRI_sim_collection_t* collection,
                          TRI_datafile_t* journal,
                          TRI_df_marker_t* marker,
                          TRI_voc_size_t markerSize,
                          void const* body,
                          TRI_voc_size_t bodySize,
                          TRI_df_marker_t* result) {
  bool ok;

  ok = TRI_WriteElementDatafile(journal,
                                result,
                                marker, markerSize,
                                body, bodySize,
                                false);

  if (! ok) {
    return false;
  }

  TRI_LockCondition(&collection->_journalsCondition);
  journal->_written = ((char*) result) + marker->_size;
  journal->_nWritten++;
  TRI_UnlockCondition(&collection->_journalsCondition);

  return true;
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
/// @brief creates a new header
////////////////////////////////////////////////////////////////////////////////

static void CreateHeader (TRI_doc_collection_t* c,
                          TRI_datafile_t* datafile,
                          TRI_df_marker_t const* m,
                          size_t markerSize,
                          TRI_doc_mptr_t* header,
                          void const* additional) {
  TRI_doc_document_marker_t const* marker;

  marker = (TRI_doc_document_marker_t const*) m;

  header->_did = marker->_did;
  header->_rid = marker->_rid;
  header->_fid = datafile->_fid;
  header->_deletion = 0;
  header->_data = marker;
  header->_document._sid = marker->_shape;
  header->_document._data.length = marker->base._size - markerSize;
  header->_document._data.data = ((char*) marker) + markerSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* CreateDocument (TRI_sim_collection_t* collection,
                                       TRI_doc_document_marker_t* marker,
                                       size_t markerSize,
                                       void const* body,
                                       TRI_voc_size_t bodySize,
                                       TRI_df_marker_t** result,
                                       void const* additional,
                                       bool release) {

  TRI_datafile_t* journal;
  TRI_doc_mptr_t* header;
  TRI_voc_size_t total;
  TRI_doc_datafile_info_t* dfi;
  bool ok;

  // get a new header pointer
  header = collection->_headers->request(collection->_headers);

  // generate a new tick
  marker->_rid = marker->_did = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = markerSize + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return NULL;
  }

  // verify the header pointer
  header = collection->_headers->verify(collection->_headers, header);

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, markerSize, body, bodySize);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, markerSize, body, bodySize, *result);

  // generate create header
  if (ok) {

    // fill the header
    collection->base.createHeader(&collection->base, journal, *result, markerSize, header, 0);

    // update the datafile info
    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);

    dfi->_numberAlive += 1;
    dfi->_sizeAlive += header->_document._data.length;

    // update immediate indexes
    CreateImmediateIndexes(collection, header);

    // release lock
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    // wait for sync
    WaitSync(collection, journal, ((char const*) *result) + markerSize + bodySize);

    // and return
    return header;
  }
  else {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    LOG_ERROR("cannot write element: %s", TRI_last_error());
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing header
////////////////////////////////////////////////////////////////////////////////

static void UpdateHeader (TRI_doc_collection_t* c,
                          TRI_datafile_t* datafile,
                          TRI_df_marker_t const* m,
                          size_t markerSize,
                          TRI_doc_mptr_t const* header,
                          TRI_doc_mptr_t* update) {
  TRI_doc_document_marker_t const* marker;

  marker = (TRI_doc_document_marker_t const*) m;
  *update = *header;

  update->_rid = marker->_rid;
  update->_fid = datafile->_fid;
  update->_data = marker;
  update->_document._sid = marker->_shape;
  update->_document._data.length = marker->base._size - markerSize;
  update->_document._data.data = ((char*) marker) + markerSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* UpdateDocument (TRI_sim_collection_t* collection,
                                             TRI_doc_mptr_t const* header,
                                             TRI_doc_document_marker_t* marker,
                                             size_t markerSize,
                                             void const* body,
                                             TRI_voc_size_t bodySize,
                                             TRI_voc_rid_t rid,
                                             TRI_doc_update_policy_e policy,
                                             TRI_df_marker_t** result,
                                             bool release) {
  TRI_datafile_t* journal;
  TRI_voc_size_t total;
  bool ok;

  // check the revision
  switch (policy) {
    case TRI_DOC_UPDATE_ERROR:
      if (rid != 0) {
        if (rid != header->_rid) {
          TRI_set_errno(TRI_VOC_ERROR_CONFLICT);
          return NULL;
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);
      return NULL;

    case TRI_DOC_UPDATE_ILLEGAL:
      TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);
      return NULL;
  }

  // generate a new tick
  marker->_rid = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = markerSize + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return NULL;
  }

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, markerSize, body, bodySize);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, markerSize, body, bodySize, *result);

  // update the header
  if (ok) {
    TRI_doc_mptr_t update;
    TRI_doc_datafile_info_t* dfi;

    // update the header
    collection->base.updateHeader(&collection->base, journal, *result, markerSize, header, &update);

    // update the datafile info
    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, header->_fid);

    dfi->_numberAlive -= 1;
    dfi->_sizeAlive -= header->_document._data.length;

    dfi->_numberDead += 1;
    dfi->_sizeDead += header->_document._data.length;

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);

    dfi->_numberAlive += 1;
    dfi->_sizeAlive += update._document._data.length;

    // update immediate indexes
    UpdateImmediateIndexes(collection, header, &update);

    // release lock
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    // wait for sync
    WaitSync(collection, journal, ((char const*) *result) + markerSize + bodySize);

    // and return
    return header;
  }
  else {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    LOG_ERROR("cannot write element");
    return NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element and removes it from the index
////////////////////////////////////////////////////////////////////////////////

static bool DeleteDocument (TRI_sim_collection_t* collection,
                            TRI_doc_deletion_marker_t* marker,
                            TRI_voc_rid_t rid,
                            TRI_doc_update_policy_e policy,
                            bool release) {
  TRI_datafile_t* journal;
  TRI_df_marker_t* result;
  TRI_doc_mptr_t const* header;
  TRI_voc_size_t total;
  bool ok;

  // get an existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &marker->_did);

  if (header == NULL || header->_deletion != 0) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return false;
  }

  // check the revision
  switch (policy) {
    case TRI_DOC_UPDATE_ERROR:
      if (rid != 0) {
        if (rid != header->_rid) {
          TRI_set_errno(TRI_VOC_ERROR_CONFLICT);
          return NULL;
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);
      return NULL;

    case TRI_DOC_UPDATE_ILLEGAL:
      TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);
      return NULL;
  }

  // generate a new tick
  marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_deletion_marker_t);
  journal = SelectJournal(collection, total, &result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0, result);

  // update the header
  if (ok) {
    TRI_doc_datafile_info_t* dfi;

    // update the datafile info
    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, header->_fid);

    dfi->_numberAlive -= 1;
    dfi->_sizeAlive -= header->_document._data.length;

    dfi->_numberDead += 1;
    dfi->_sizeDead += header->_document._data.length;

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);

    dfi->_numberDeletion += 1;

    // update immediate indexes
    DeleteImmediateIndexes(collection, header, marker->base._tick);

    // release lock
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    // wait for sync
    WaitSync(collection, journal, ((char const*) result) + sizeof(TRI_doc_deletion_marker_t));
  }
  else {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    LOG_ERROR("cannot delete element");
  }

  return ok;
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
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoDatafile (TRI_doc_collection_t* collection,
                                       TRI_datafile_t* datafile) {
  TRI_doc_datafile_info_t* dfi;

  dfi = TRI_FindDatafileInfoDocCollection(collection, datafile->_fid);

  printf("DATAFILE '%s'\n", datafile->_filename);

  if (dfi == NULL) {
    printf(" no info\n\n");
    return;
  }

  printf("  number alive: %ld\n", (long) dfi->_numberAlive);
  printf("  size alive:   %ld\n", (long) dfi->_sizeAlive);
  printf("  number dead:  %ld\n", (long) dfi->_numberDead);
  printf("  size dead:    %ld\n", (long) dfi->_sizeDead);
  printf("  deletion:     %ld\n\n", ( long) dfi->_numberDeletion);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoDocCollection (TRI_doc_collection_t* collection) {
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

static void DebugHeaderSimCollection (TRI_sim_collection_t* collection) {
  void** end;
  void** ptr;

  // update index
  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      TRI_doc_mptr_t* d;

      d = *ptr;

      printf("fid %lu, did %lu, rid %lu, eid %lu, del %lu\n",
             (unsigned long) d->_fid,
             (unsigned long) d->_did,
             (unsigned long) d->_rid,
             (unsigned long) d->_eid,
             (unsigned long) d->_deletion);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* CreateShapedJson (TRI_doc_collection_t* document,
                                               TRI_df_marker_type_e type,
                                               TRI_shaped_json_t const* json,
                                               void const* data,
                                               bool release) {
  TRI_df_marker_t* result;
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;

  if (type == TRI_DOC_MARKER_DOCUMENT) {
    TRI_doc_document_marker_t marker;

    memset(&marker, 0, sizeof(marker));

    marker.base._size = sizeof(marker) + json->_data.length;
    marker.base._type = type;

    marker._sid = 0;
    marker._shape = json->_sid;

    return CreateDocument(collection,
                          &marker, sizeof(marker),
                          json->_data.data, json->_data.length,
                          &result,
                          data,
                          release);
  }
  else if (type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_edge_marker_t marker;
    TRI_sim_edge_t const* edge;

    edge = data;

    memset(&marker, 0, sizeof(marker));

    marker.base.base._size = sizeof(marker) + json->_data.length;
    marker.base.base._type = type;

    marker.base._sid = 0;
    marker.base._shape = json->_sid;

    marker._fromCid = edge->_fromCid;
    marker._fromDid = edge->_fromDid;
    marker._toCid = edge->_toCid;
    marker._toDid = edge->_toDid;

    return CreateDocument(collection,
                          &marker.base, sizeof(marker),
                          json->_data.data, json->_data.length,
                          &result,
                          data,
                          release);
  }
  else {
    LOG_FATAL("unknown marker type %lu", (unsigned long) type);
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* ReadShapedJson (TRI_doc_collection_t* document,
                                             TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_mptr_t const* header;

  collection = (TRI_sim_collection_t*) document;

  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    return NULL;
  }
  else {
    return header;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* UpdateShapedJson (TRI_doc_collection_t* document,
                                               TRI_shaped_json_t const* json,
                                               TRI_voc_did_t did,
                                               TRI_voc_rid_t rid,
                                               TRI_doc_update_policy_e policy,
                                               bool release) {
  TRI_df_marker_t const* original;
  TRI_df_marker_t* result;
  TRI_doc_mptr_t const* header;
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;

  // get an existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return NULL;
  }

  original = header->_data;

  // the original is an document
  if (original->_type == TRI_DOC_MARKER_DOCUMENT) {
    TRI_doc_document_marker_t marker;

    // create an update
    memset(&marker, 0, sizeof(marker));

    marker.base._size = sizeof(marker) + json->_data.length;
    marker.base._type = original->_type;

    marker._did = did;
    marker._sid = 0;
    marker._shape = json->_sid;

    return UpdateDocument(collection,
                          header,
                          &marker, sizeof(marker),
                          json->_data.data, json->_data.length,
                          rid,
                          policy,
                          &result,
                          release);
  }

  // the original is an edge
  else if (original->_type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_edge_marker_t marker;
    TRI_doc_edge_marker_t const* originalEdge;

    originalEdge = header->_data;

    // create an update
    memset(&marker, 0, sizeof(marker));

    marker.base.base._size = sizeof(marker) + json->_data.length;
    marker.base.base._type = original->_type;

    marker.base._did = did;
    marker.base._sid = 0;
    marker.base._shape = json->_sid;

    marker._fromCid = originalEdge->_fromCid;
    marker._fromDid = originalEdge->_fromDid;
    marker._toCid = originalEdge->_toCid;
    marker._toDid = originalEdge->_toDid;

    return UpdateDocument(collection,
                          header,
                          &marker.base, sizeof(marker),
                          json->_data.data, json->_data.length,
                          rid,
                          policy,
                          &result,
                          release);
  }

  // do not know
  else {
    LOG_FATAL("unknown marker type %lu", (unsigned long) original->_type);
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static bool DeleteShapedJson (TRI_doc_collection_t* document,
                              TRI_voc_did_t did,
                              TRI_voc_rid_t rid,
                              TRI_doc_update_policy_e policy,
                              bool release) {
  TRI_sim_collection_t* collection;
  TRI_doc_deletion_marker_t marker;

  collection = (TRI_sim_collection_t*) document;

  memset(&marker, 0, sizeof(marker));

  marker.base._size = sizeof(marker);
  marker.base._type = TRI_DOC_MARKER_DELETION;

  marker._did = did;
  marker._sid = 0;

  return DeleteDocument(collection, &marker, rid, policy, release);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

static bool BeginRead (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_ReadLockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static bool EndRead (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static bool BeginWrite (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_WriteLockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static bool EndWrite (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a document collection
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t SizeSimCollection (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;
  TRI_voc_size_t result;

  collection = (TRI_sim_collection_t*) document;

  TRI_ReadLockReadWriteLock(&collection->_lock);
  result = collection->_primaryIndex._nrUsed;
  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE COLLECTION
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
  TRI_sim_collection_t* collection = data;
  TRI_doc_mptr_t const* found;
  TRI_doc_datafile_info_t* dfi;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_DOCUMENT || marker->_type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_document_marker_t const* d = (TRI_doc_document_marker_t const*) marker;
    size_t markerSize;

    if (marker->_type == TRI_DOC_MARKER_DOCUMENT) {
      LOG_TRACE("document: fid %lu, did %lu, rid %lu",
                (unsigned long) datafile->_fid,
                (unsigned long) d->_did,
                (unsigned long) d->_rid);

      markerSize = sizeof(TRI_doc_document_marker_t);
    }
    else if (marker->_type == TRI_DOC_MARKER_EDGE) {
      TRI_doc_edge_marker_t const* e = (TRI_doc_edge_marker_t const*) marker;

      LOG_TRACE("edge: fid %lu, did %lu, rid %lu, edge (%lu,%lu) -> (%lu,%lu)",
                (unsigned long) datafile->_fid,
                (unsigned long) d->_did,
                (unsigned long) d->_rid,
                (unsigned long) e->_fromCid,
                (unsigned long) e->_fromDid,
                (unsigned long) e->_toCid,
                (unsigned long) e->_toDid);

      markerSize = sizeof(TRI_doc_edge_marker_t);
    }
    else {
      LOG_FATAL("unknown marker type %lu", (unsigned long) marker->_type);
      exit(EXIT_FAILURE);
    }

    found = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    // it is a new entry
    if (found == NULL) {
      TRI_doc_mptr_t* header;

      header = collection->_headers->request(collection->_headers);
      header = collection->_headers->verify(collection->_headers, header);

      // fill the header
      collection->base.createHeader(&collection->base, datafile, marker, markerSize, header, 0);

      // update the datafile info
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      dfi->_numberAlive += 1;
      dfi->_sizeAlive += header->_document._data.length;

      // update immediate indexes
      CreateImmediateIndexes(collection, header);
    }

    // it is an delete
    else if (found->_deletion != 0) {
      LOG_TRACE("skipping already deleted document: %lu", (unsigned long) d->_did);
    }

    // it is an update, but only if found has a smaller revision identifier
    else if (found->_rid < d->_rid || (found->_rid == d->_rid && found->_fid <= datafile->_fid)) {
      TRI_doc_mptr_t update;

      // update the header info
      collection->base.updateHeader(&collection->base, datafile, marker, markerSize, found, &update);

      // update the datafile info
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, found->_fid);

      dfi->_numberAlive -= 1;
      dfi->_sizeAlive -= found->_document._data.length;

      dfi->_numberDead += 1;
      dfi->_sizeDead += found->_document._data.length;

      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      dfi->_numberAlive += 1;
      dfi->_sizeAlive += update._document._data.length;

      // update immediate indexes
      UpdateImmediateIndexes(collection, found, &update);
    }

    // it is a stale update
    else {
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      dfi->_numberDead += 1;
      dfi->_sizeDead += found->_document._data.length;
    }
  }

  // deletion
  else if (marker->_type == TRI_DOC_MARKER_DELETION) {
    TRI_doc_deletion_marker_t const* d;

    d = (TRI_doc_deletion_marker_t const*) marker;

    LOG_TRACE("deletion: fid %lu, did %lu, rid %lu, deletion %lu",
              (unsigned long) datafile->_fid,
              (unsigned long) d->_did,
              (unsigned long) d->_rid,
              (unsigned long) marker->_tick);

    found = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    // it is a new entry, so we missed the create
    if (found == NULL) {
      TRI_doc_mptr_t* header;

      header = collection->_headers->request(collection->_headers);
      header = collection->_headers->verify(collection->_headers, header);

      header->_did = d->_did;
      header->_rid = d->_rid;
      header->_deletion = marker->_tick;
      header->_data = 0;
      header->_document._data.length = 0;
      header->_document._data.data = 0;

      // update immediate indexes
      CreateImmediateIndexes(collection, header);

      // update the datafile info
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      dfi->_numberDeletion += 1;
    }

    // it is a real delete
    else if (found->_deletion == 0) {
      union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;

      // mark element as deleted
      change.c = found;
      change.v->_deletion = marker->_tick;

      // update the datafile info
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, found->_fid);

      dfi->_numberAlive -= 1;
      dfi->_sizeAlive -= found->_document._data.length;

      dfi->_numberDead += 1;
      dfi->_sizeDead += found->_document._data.length;

      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      dfi->_numberDeletion += 1;
    }

    // it is a double delete
    else {
      LOG_TRACE("skipping deletion of already deleted document: %lu", (unsigned long) d->_did);
    }
  }
  else {
    LOG_TRACE("skipping marker %lu", (unsigned long) marker->_type);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIndex (char const* filename, void* data) {
  TRI_idx_iid_t iid;
  TRI_index_t* idx;
  TRI_json_t* fieldCount;
  TRI_json_t* fieldStr;
  TRI_json_t* gjs;
  TRI_json_t* iis;
  TRI_json_t* json;
  TRI_json_t* type;
  TRI_sim_collection_t* doc;
  TRI_vector_t attributes;
  bool uniqueIndex;
  char const* typeStr;
  char fieldChar[30];
  char* error;
  int intCount;

  // load json description of the index
  json = TRI_JsonFile(filename, &error);

  // simple collection of the index
  doc = (TRI_sim_collection_t*) data;

  // json must be a index description
  if (json == NULL) {
    LOG_ERROR("cannot read index definition from '%s': %s", filename, error);
    return false;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot read index definition from '%s': expecting an array", filename);

    TRI_FreeJson(json);
    return false;
  }

  // extract the type
  type = TRI_LookupArrayJson(json, "type");

  if (type->_type != TRI_JSON_STRING) {
    LOG_ERROR("cannot read index definition from '%s': expecting a string for type", filename);

    TRI_FreeJson(json);
    return false;
  }

  typeStr = type->_value._string.data;

  // extract the index identifier
  iis = TRI_LookupArrayJson(json, "iid");

  if (iis != NULL && iis->_type == TRI_JSON_NUMBER) {
    iid = iis->_value._number;
  }
  else {
    LOG_ERROR("ignore hash-index, index identifier could not be located");
    return false;
  }  

  // ...........................................................................
  // GEO INDEX
  // ...........................................................................

  if (TRI_EqualString(typeStr, "geo")) {
    TRI_json_t* lat;
    TRI_json_t* loc;
    TRI_json_t* lon;
    bool geoJson;

    loc = TRI_LookupArrayJson(json, "location");
    lat = TRI_LookupArrayJson(json, "latitude");
    lon = TRI_LookupArrayJson(json, "longitude");
    gjs = TRI_LookupArrayJson(json, "geoJson");
    iid = 0;
    geoJson = false;
    
    if (gjs != NULL && gjs->_type == TRI_JSON_BOOLEAN) {
      geoJson = gjs->_value._boolean;
    }

    if (loc != NULL && loc->_type == TRI_JSON_STRING) {
      CreateGeoIndexSimCollection(doc, loc->_value._string.data, NULL, NULL, geoJson, iid);
    }
    else if (lat != NULL && lat->_type == TRI_JSON_STRING && lon != NULL && lon ->_type == TRI_JSON_STRING) {
      CreateGeoIndexSimCollection(doc, NULL, lat->_value._string.data, lon->_value._string.data, false, iid);
    }
    else {
      LOG_ERROR("ignore geo-index %lu, need either 'location' or 'latitude' and 'longitude'",
                (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }

    TRI_FreeJson(json);
    return true;
  }
  
  // ...........................................................................
  // HASH INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash")) {
  
    // Determine if the hash index is unique or non-unique
    gjs = TRI_LookupArrayJson(json, "unique");
    uniqueIndex = false;

    if (gjs != NULL && gjs->_type == TRI_JSON_BOOLEAN) {
      uniqueIndex = gjs->_value._boolean;
    }
    else {
      LOG_ERROR("ignore hash-index %lu, could not determine if unique or non-unique",
                (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }  
    
    // Extract the list of fields
    fieldCount = 0;
    fieldCount = TRI_LookupArrayJson(json, "fieldCount");
    intCount = 0;

    if ( (fieldCount != NULL) && (fieldCount->_type == TRI_JSON_NUMBER) ) {
      intCount = fieldCount->_value._number;
    }

    if (intCount < 1) {
      LOG_ERROR("ignore hash-index %lu, field count missing", (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }

    // Initialise the vector in which we store the fields on which the hashing
    // will be based.
    TRI_InitVector(&attributes, sizeof(char*));
    
    // find fields
    for (int j = 0;  j < intCount;  ++j) {
      sprintf(fieldChar, "field_%i", j);

      fieldStr = TRI_LookupArrayJson(json, fieldChar);

      if (fieldStr->_type != TRI_JSON_STRING) {
        LOG_ERROR("ignore hash-index %lu, invalid field name for hash index",
                  (unsigned long) iid);

        TRI_DestroyVector(&attributes);
        TRI_FreeJson(json);
        return false;
      }  

      TRI_PushBackVector(&attributes, &(fieldStr->_value._string.data));
    }  

    // create the index
    idx = CreateHashIndexSimCollection (doc, &attributes, iid, uniqueIndex); 

    TRI_DestroyVector(&attributes);
    TRI_FreeJson(json);

    if (idx == NULL) {
      LOG_ERROR("cannot create hash index %lu", (unsigned long) iid);
      return false;
    }

    return true;
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "skiplist")) {
    
    // Determine if the skiplist index is unique or non-unique
    gjs = TRI_LookupArrayJson(json, "unique");
    uniqueIndex = false;

    if (gjs != NULL && gjs->_type == TRI_JSON_BOOLEAN) {
      uniqueIndex = gjs->_value._boolean;
    }
    else {
      LOG_ERROR("ignore skiplist-index %lu, could not determine if unique or non-unique",
                (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }  
    
    // Extract the list of fields
    fieldCount = 0;
    fieldCount = TRI_LookupArrayJson(json, "fieldCount");
    intCount = 0;

    if ( (fieldCount != NULL) && (fieldCount->_type == TRI_JSON_NUMBER) ) {
      intCount = fieldCount->_value._number;
    }

    if (intCount < 1) {
      LOG_ERROR("ignore skiplist-index %lu, field count missing", (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }
    
    // Initialise the vector in which we store the fields on which the hashing
    // will be based.
    TRI_InitVector(&attributes, sizeof(char*));

    // find fields
    for (int j = 0; j < intCount; ++j) {
      sprintf(fieldChar, "field_%i", j);

      fieldStr = TRI_LookupArrayJson(json, fieldChar);

      if (fieldStr->_type != TRI_JSON_STRING) {
        LOG_ERROR("ignore skiplist-index %lu, invalid field name for hash index",
                  (unsigned long) iid);

        TRI_DestroyVector(&attributes);
        TRI_FreeJson(json);
        return false;
      }  

      TRI_PushBackVector(&attributes, &(fieldStr->_value._string.data));
    }  
    
    // create the index
    idx = CreateSkiplistIndexSimCollection (doc, &attributes, iid, uniqueIndex); 

    TRI_DestroyVector(&attributes);
    TRI_FreeJson(json);

    if (idx == NULL) {
      LOG_ERROR("cannot create hash index %lu", (unsigned long) iid);
      return false;
    }

    return true;
  }
  
  // .........................................................................
  // ups, unknown index type
  // .........................................................................

  else {
    LOG_ERROR("ignoring unknown index type '%s' for index %lu", 
              typeStr,
              (unsigned long) iid);

    TRI_FreeJson(json);
    return false;
  }
}


//////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdge (TRI_multi_pointer_t* array, void const* data) {
  TRI_edge_header_t const* h;
  uint64_t hash[3];

  h = data;

  hash[0] = h->_direction;
  hash[1] = h->_cid;
  hash[2] = h->_did;

  return TRI_FnvHashPointer(hash, sizeof(hash));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdge (TRI_multi_pointer_t* array, void const* left, void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;

  l = left;
  r = right;

  return l->_direction == r->_direction && l->_cid == r->_cid && l->_did == r->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge (TRI_multi_pointer_t* array, void const* left, void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;

  l = left;
  r = right;

  return l->_mptr == r->_mptr && l->_direction == r->_direction && l->_cid == r->_cid && l->_did == r->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static void InitSimCollection (TRI_sim_collection_t* collection,
                               TRI_shaper_t* shaper) {
  TRI_InitDocCollection(&collection->base, shaper);

  TRI_InitReadWriteLock(&collection->_lock);

  collection->_headers = TRI_CreateSimpleHeaders(sizeof(TRI_doc_mptr_t));

  TRI_InitAssociativePointer(&collection->_primaryIndex,
                             HashKeyHeader,
                             HashElementDocument,
                             IsEqualKeyDocument,
                             0);

  TRI_InitMultiPointer(&collection->_edgesIndex,
                       HashElementEdge,
                       HashElementEdge,
                       IsEqualKeyEdge,
                       IsEqualElementEdge);

  TRI_InitCondition(&collection->_journalsCondition);

  TRI_InitVectorPointer(&collection->_indexes);

  collection->base.createHeader = CreateHeader;
  collection->base.updateHeader = UpdateHeader;

  collection->base.beginRead = BeginRead;
  collection->base.endRead = EndRead;

  collection->base.beginWrite = BeginWrite;
  collection->base.endWrite = EndWrite;

  collection->base.create = CreateShapedJson;
  collection->base.read = ReadShapedJson;
  collection->base.update = UpdateShapedJson;
  collection->base.destroy = DeleteShapedJson;

  collection->base.size = SizeSimCollection;
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

TRI_sim_collection_t* TRI_CreateSimCollection (char const* path, TRI_col_parameter_t* parameter) {
  TRI_col_info_t info;
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;

  memset(&info, 0, sizeof(info));
  info._version = TRI_COL_VERSION;
  info._type = parameter->_type;
  info._cid = TRI_NewTickVocBase();
  TRI_CopyString(info._name, parameter->_name, sizeof(info._name));
  info._maximalSize = parameter->_maximalSize;
  info._size = sizeof(TRI_col_info_t);

  // first create the document collection
  doc = TRI_Allocate(sizeof(TRI_sim_collection_t));
  if (!doc) {
    LOG_ERROR("cannot create document");
    return NULL;
  }

  collection = TRI_CreateCollection(&doc->base.base, path, &info);

  if (collection == NULL) {
    LOG_ERROR("cannot create document collection");

    TRI_Free(doc);
    return NULL;
  }

  // then the shape collection
  shaper = TRI_CreateVocShaper(collection->_directory, "SHAPES");

  if (shaper == NULL) {
    LOG_ERROR("cannot create shapes collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    TRI_Free(doc);
    return NULL;
  }

  // create document collection and shaper
  InitSimCollection(doc, shaper);

  return doc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimCollection (TRI_sim_collection_t* collection) {
  TRI_DestroyCondition(&collection->_journalsCondition);

  TRI_DestroyAssociativePointer(&collection->_primaryIndex);

  TRI_FreeSimpleHeaders(collection->_headers);

  TRI_DestroyReadWriteLock(&collection->_lock);

  if (collection->base._shaper != NULL) {
    TRI_FreeVocShaper(collection->base._shaper);
  }

  TRI_DestroyDocCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimCollection (TRI_sim_collection_t* collection) {
  TRI_DestroySimCollection(collection);
  TRI_Free(collection);
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

TRI_datafile_t* TRI_CreateJournalSimCollection (TRI_sim_collection_t* collection) {
  return TRI_CreateJournalDocCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalSimCollection (TRI_sim_collection_t* collection,
                                    size_t position) {
  return TRI_CloseJournalDocCollection(&collection->base, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_sim_collection_t* TRI_OpenSimCollection (char const* path) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;
  char* shapes;

  // first open the document collection
  doc = TRI_Allocate(sizeof(TRI_sim_collection_t));
  if (!doc) {
    return NULL;
  }

  collection = TRI_OpenCollection(&doc->base.base, path);

  if (collection == NULL) {
    LOG_ERROR("cannot open document collection");

    TRI_Free(doc);
    return NULL;
  }

  // then the shape collection
  shapes = TRI_Concatenate2File(collection->_directory, "SHAPES");
  if (!shapes) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    TRI_Free(doc);
    return NULL;
  }

  shaper = TRI_OpenVocShaper(shapes);
  TRI_FreeString(shapes);

  if (shaper == NULL) {
    LOG_ERROR("cannot open shapes collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    return NULL;
  }

  // create document collection and shaper
  InitSimCollection(doc, shaper);

  // read all documents and fill indexes
  TRI_IterateCollection(collection, OpenIterator, collection);
  TRI_IterateIndexCollection(collection, OpenIndex, collection);

  // output infomations about datafiles and journals
  if (TRI_IsTraceLogging(__FILE__)) {
    DebugDatafileInfoDocCollection(&doc->base);
    DebugHeaderSimCollection(doc);
  }

  return doc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseSimCollection (TRI_sim_collection_t* collection) {
  bool ok;

  ok = TRI_CloseCollection(&collection->base.base);

  if (! ok) {
    return false;
  }

  ok = TRI_CloseVocShaper(collection->base._shaper);

  if (! ok) {
    return false;
  }

  collection->base._shaper = NULL;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t* header) {
  TRI_df_marker_t const* marker;
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  bool ok;
  bool result;

  // add a new header
  found = TRI_InsertKeyAssociativePointer(&collection->_primaryIndex, &header->_did, header, false);

  if (found != NULL) {
    LOG_ERROR("document %lu already existed with revision %lu while creating revision %lu",
              (unsigned long) header->_did,
              (unsigned long) found->_rid,
              (unsigned long) header->_rid);

    collection->_headers->release(collection->_headers, header);
    return false;
  }

  // return in case of a delete
  if (header->_deletion != 0) {
    return true;
  }

  // check the document type
  marker = header->_data;

  // add edges
  if (marker->_type == TRI_DOC_MARKER_EDGE) {
    TRI_edge_header_t* entry;
    TRI_doc_edge_marker_t const* edge;

    edge = header->_data;

    // IN
    entry = TRI_Allocate(sizeof(TRI_edge_header_t));
    if (!entry) {
      // TODO: what to do in this case?
      return false;
    }
    entry->_mptr = header;
    entry->_direction = TRI_EDGE_IN;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;
    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    // OUT
    entry = TRI_Allocate(sizeof(TRI_edge_header_t));
    if (!entry) {
      // TODO: what to do in this case?
      return false;
    }
    entry->_mptr = header;
    entry->_direction = TRI_EDGE_OUT;
    entry->_cid = edge->_fromCid;
    entry->_did = edge->_fromDid;
    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    // ANY
    entry = TRI_Allocate(sizeof(TRI_edge_header_t));
    if (!entry) {
      // TODO: what to do in this case?
      return false;
    }
    entry->_mptr = header;
    entry->_direction = TRI_EDGE_ANY;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;
    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    if (edge->_toCid != edge->_fromCid || edge->_toDid != edge->_fromDid) {
      entry = TRI_Allocate(sizeof(TRI_edge_header_t));
      if (!entry) {
        // TODO: what to do in this case?
        return false;
      }
      entry->_mptr = header;
      entry->_direction = TRI_EDGE_ANY;
      entry->_cid = edge->_fromCid;
      entry->_did = edge->_fromDid;
      TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);
    }
  }

  // update all the other indices
  n = collection->_indexes._length;
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    ok = idx->insert(idx, header);

    // i < n ? idx : NULL;
    if (! ok) {
      result = false;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_doc_mptr_t const* update) {

  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_shaped_json_t old;
  bool ok;
  bool result;
  size_t i;
  size_t n;

  // get the old document
  old = header->_document;

  // update all fields, the document identifier stays the same
  change.c = header;

  change.v->_rid = update->_rid;
  change.v->_eid = update->_eid;
  change.v->_fid = update->_fid;
  change.v->_deletion = update->_deletion;

  change.v->_data = update->_data;
  change.v->_document = update->_document;

  // update all other indexes
  n = collection->_indexes._length;
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    ok = idx->update(idx, header, &old);

    if (! ok) {
      result = false;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_voc_tick_t deletion) {
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_df_marker_t const* marker;
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  bool result;
  bool ok;

  // set the deletion flag
  change.c = header;
  change.v->_deletion = deletion;

  // remove from main index
  found = TRI_RemoveKeyAssociativePointer(&collection->_primaryIndex, &header->_did);

  if (found == NULL) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return false;
  }

  // check the document type
  marker = header->_data;

  // add edges
  if (marker->_type == TRI_DOC_MARKER_EDGE) {
    TRI_edge_header_t entry;
    TRI_edge_header_t* old;
    TRI_doc_edge_marker_t const* edge;

    edge = header->_data;

    memset(&entry, 0, sizeof(entry));
    entry._mptr = header;

    // IN
    entry._direction = TRI_EDGE_IN;
    entry._cid = edge->_toCid;
    entry._did = edge->_toDid;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(old);
    }

    // OUT
    entry._direction = TRI_EDGE_OUT;
    entry._cid = edge->_fromCid;
    entry._did = edge->_fromDid;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(old);
    }

    // ANY
    entry._direction = TRI_EDGE_ANY;
    entry._cid = edge->_toCid;
    entry._did = edge->_toDid;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(old);
    }

    if (edge->_toCid != edge->_fromCid || edge->_toDid != edge->_fromDid) {
      entry._direction = TRI_EDGE_ANY;
      entry._cid = edge->_fromCid;
      entry._did = edge->_fromDid;
      old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

      if (old != NULL) {
        TRI_Free(old);
      }
    }
  }

  // remove all other indexes
  n = collection->_indexes._length;
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    ok = idx->remove(idx, header);

    if (! ok) {
      result = false;
    }
  }

  // and release the header pointer
  collection->_headers->release(collection->_headers, change.v);

  // that's it
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static bool FillIndex (TRI_sim_collection_t* collection,
                       TRI_index_t* idx) {
  size_t n;
  size_t scanned;
  void** end;
  void** ptr;

  // update index
  n = collection->_primaryIndex._nrUsed;
  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  scanned = 0;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      ++scanned;

      if (! idx->insert(idx, *ptr)) {
        LOG_TRACE("failed to insert document '%lu:%lu'",
                  (unsigned long) collection->base.base._cid,
                  (unsigned long) ((TRI_doc_mptr_t const*) ptr)->_did);

        return false;
      }

      if (scanned % 10000 == 0) {
        LOG_INFO("indexed %ld of %ld documents", scanned, n);
      }
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
/// @brief returns a description of all indexes
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesSimCollection (TRI_sim_collection_t* collection) {
  TRI_vector_pointer_t* vector;
  size_t n;
  size_t i;

  vector = TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!vector) {
    return NULL;
  }
  TRI_InitVectorPointer(vector);

  // .............................................................................
  // within read-lock the collection
  // .............................................................................

  TRI_ReadLockReadWriteLock(&collection->_lock);

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    TRI_json_t* json;

    idx = collection->_indexes._buffer[i];

    json = idx->json(idx, &collection->base);

    if (json != NULL) {
      TRI_PushBackVectorPointer(vector, json);
    }
  }

  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without read-lock
  // .............................................................................

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of anindex
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_IndexSimCollection (TRI_sim_collection_t* collection, TRI_idx_iid_t iid) {
  TRI_index_t* idx = NULL;
  size_t n;
  size_t i;

  // .............................................................................
  // within read-lock the collection
  // .............................................................................

  TRI_ReadLockReadWriteLock(&collection->_lock);

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    idx = collection->_indexes._buffer[i];
    
    if (idx->_iid == iid) {
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without read-lock
  // .............................................................................

  return i < n ? idx : NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexSimCollection (TRI_sim_collection_t* collection, TRI_idx_iid_t iid) {
  TRI_vector_pointer_t* vector;
  TRI_index_t* found;
  size_t n;
  size_t i;

  vector = TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!vector) {
    return false;
  }

  found = NULL;
  TRI_InitVectorPointer(vector);

  // .............................................................................
  // within write-lock
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    if (idx->_iid == iid) {
      found = TRI_RemoveVectorPointer(&collection->_indexes, i);
      break;
    }
  }

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  if (found != NULL) {
    return TRI_RemoveIndex(&collection->base, found);
  }
  else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the document id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_did_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_voc_did_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the document header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_mptr_t const* e = element;

  return TRI_FnvHashPointer(&e->_did, sizeof(TRI_voc_did_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document id and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_did_t const* k = key;
  TRI_doc_mptr_t const* e = element;

  return *k == e->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesSimCollection (TRI_sim_collection_t* edges,
                                                   TRI_edge_direction_e direction,
                                                   TRI_voc_cid_t cid,
                                                   TRI_voc_did_t did) {
  union { TRI_doc_mptr_t* v; TRI_doc_mptr_t const* c; } cnv;
  TRI_vector_pointer_t result;
  TRI_edge_header_t entry;
  TRI_vector_pointer_t found;
  size_t i;

  TRI_InitVectorPointer(&result);

  entry._direction = direction;
  entry._cid = cid;
  entry._did = did;

  found = TRI_LookupByKeyMultiPointer(&edges->_edgesIndex, &entry);

  for (i = 0;  i < found._length;  ++i) {
    cnv.c = ((TRI_edge_header_t*) found._buffer[i])->_mptr;

    TRI_PushBackVectorPointer(&result, cnv.v);
  }

  TRI_DestroyVectorPointer(&found);

  return result;
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

static TRI_index_t* CreateGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                                 char const* location,
                                                 char const* latitude,
                                                 char const* longitude,
                                                 bool geoJson,
                                                 TRI_idx_iid_t iid) {
  TRI_index_t* idx;
  TRI_shape_pid_t lat;
  TRI_shape_pid_t loc;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = NULL;

  shaper = collection->base._shaper;

  if (location != NULL) {
    loc = shaper->findAttributePathByName(shaper, location);
  }

  if (latitude != NULL) {
    lat = shaper->findAttributePathByName(shaper, latitude);
  }

  if (longitude != NULL) {
    lon = shaper->findAttributePathByName(shaper, longitude);
  }

  // check, if we know the index
  if (location != NULL) {
    idx = TRI_LookupGeoIndexSimCollection(collection, loc, geoJson);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2SimCollection(collection, lat, lon);
  }
  else {
    TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);

    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");

    return NULL;
  }

  if (idx != NULL) {
    LOG_TRACE("geo-index already created for location '%s'", location);

    return idx;
  }

  // create a new index
  if (location != NULL) {
    idx = TRI_CreateGeoIndex(&collection->base, loc, geoJson);

    LOG_TRACE("created geo-index for location '%s': %d",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeoIndex2(&collection->base, lat, lon);

    LOG_TRACE("created geo-index for location '%s': %d, %d",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  FillIndex(collection, idx);

  // and store index
  TRI_PushBackVectorPointer(&collection->_indexes, idx);

  return idx;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateHashIndexSimCollection (TRI_sim_collection_t* collection,
                                                  const TRI_vector_t* attributes,
                                                  TRI_idx_iid_t iid,
                                                  bool unique) {
  TRI_index_t* idx;
  TRI_shaper_t* shaper;
  TRI_vector_t shapes;
  bool ok;

  idx     = NULL;
  shaper = collection->base._shaper;
  
  TRI_InitVector(&shapes, sizeof(TRI_shape_pid_t));

  // ...........................................................................
  // Determine the shape ids for the attributes
  // ...........................................................................

  for (size_t j = 0; j < attributes->_length; ++j) {
    char* shapeString;
    TRI_shape_pid_t shape;

    shapeString = *((char**)(TRI_AtVector(attributes,j)));    
    shape       = shaper->findAttributePathByName(shaper, shapeString);   

    TRI_PushBackVector(&shapes, &shape);
  }
 
  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = TRI_LookupHashIndexSimCollection(collection, &shapes);
  
  if (idx != NULL) {
    TRI_DestroyVector(&shapes);
    LOG_TRACE("hash-index already created");

    return idx;
  }

  // ...........................................................................
  // Create the hash index
  // ...........................................................................

  idx = TRI_CreateHashIndex(&collection->base, &shapes, unique);

  // ...........................................................................
  // release memory allocated to vector
  // ...........................................................................

  TRI_DestroyVector(&shapes);
  
  // ...........................................................................
  // If index id given, use it otherwise use the default.
  // ...........................................................................

  if (iid) {
    idx->_iid = iid;
  }
  
  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................

  ok = FillIndex(collection, idx);

  if (! ok) {
    TRI_FreeHashIndex(idx);
    return NULL;
  }
  
  // ...........................................................................
  // store index and return
  // ...........................................................................

  TRI_PushBackVectorPointer(&collection->_indexes, idx);

  return idx;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skiplist index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                      const TRI_vector_t* attributes,
                                                      TRI_idx_iid_t iid,
                                                      bool unique) {
  TRI_index_t* idx     = NULL;
  TRI_shaper_t* shaper = collection->base._shaper;
  TRI_vector_t shapes;
  
  TRI_InitVector(&shapes, sizeof(TRI_shape_pid_t));


  // ...........................................................................
  // Determine the shape ids for the attributes
  // ...........................................................................
  for (size_t j = 0; j < attributes->_length; ++j) {
    char* shapeString     = *((char**)(TRI_AtVector(attributes,j)));    
    TRI_shape_pid_t shape = shaper->findAttributePathByName(shaper, shapeString);   
    TRI_PushBackVector(&shapes,&shape);
  }
  
  
  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................
  idx = TRI_LookupSkiplistIndexSimCollection(collection, &shapes);
  
  
  if (idx != NULL) {
    TRI_DestroyVector(&shapes);
    LOG_TRACE("skiplist-index already created");
    return idx;
  }


  // ...........................................................................
  // Create the skiplist index
  // ...........................................................................
  idx = TRI_CreateSkiplistIndex(&collection->base,&shapes, unique);
  

  // ...........................................................................
  // If index id given, use it otherwise use the default.
  // ...........................................................................
  if (iid) {
    idx->_iid = iid;
  }
  
  
  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................
  FillIndex(collection, idx);

  
  // ...........................................................................
  // store index
  // ...........................................................................
  TRI_PushBackVectorPointer(&collection->_indexes, idx);

  
  // ...........................................................................
  // release memory allocated to vector
  // ...........................................................................
  TRI_DestroyVector(&shapes);
  
  return idx;  
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
/// @brief finds a geo index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                              TRI_shape_pid_t location,
                                              bool geoJson) {
  size_t n;
  size_t i;

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_location != 0 && geo->_location == location && geo->_geoJson == geoJson) {
        return idx;
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                               TRI_shape_pid_t latitude,
                                               TRI_shape_pid_t longitude) {
  size_t n;
  size_t i;

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_GEO_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_latitude != 0 && geo->_longitude != 0 && geo->_latitude == latitude && geo->_longitude == longitude) {
        return idx;
      }
    }
  }

  return NULL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupHashIndexSimCollection (TRI_sim_collection_t* collection,
                                               const TRI_vector_t* shapes) {
  TRI_index_t* matchedIndex = NULL;                                                                                        
  
  // ...........................................................................
  // Note: This function does NOT differentiate between non-unique and unique
  //       hash indexes. The first hash index which matches the attributes
  //       (shapes parameter) will be returned.
  // ...........................................................................
  
  
  // ........................................................................... 
  // go through every index and see if we have a match 
  // ........................................................................... 
  
  for (size_t j = 0;  j < collection->_indexes._length;  ++j) {
    TRI_index_t* idx            = collection->_indexes._buffer[j];
    TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;
    bool found                  = true;

        
    // .........................................................................
    // check that the type of the index is in fact a hash index 
    // .........................................................................
        
    if (idx->_type != TRI_IDX_TYPE_HASH_INDEX) {
      continue;
    }

        
    // .........................................................................
    // check that the number of shapes (fields) in the hash index matches that
    // of the number of attributes
    // .........................................................................
        
    if (shapes->_length != hashIndex->_shapeList->_length) {
      continue;
    }
        
        
    // .........................................................................
    // Go through all the attributes and see if they match
    // .........................................................................

    for (size_t k = 0; k < shapes->_length; ++k) {
      TRI_shape_pid_t field = *((TRI_shape_pid_t*)(TRI_AtVector(hashIndex->_shapeList,k)));   
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(shapes,k)));
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
/// @brief finds a skiplist index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                   const TRI_vector_t* shapes) {
  TRI_index_t* matchedIndex = NULL;                                                                                        
  
  // ...........................................................................
  // Note: This function does NOT differentiate between non-unique and unique
  //       skiplist indexes. The first index which matches the attributes
  //       (shapes parameter) will be returned.
  // ...........................................................................
  
  
  // ........................................................................... 
  // go through every index and see if we have a match 
  // ........................................................................... 
  
  for (size_t j = 0;  j < collection->_indexes._length;  ++j) {
    TRI_index_t* idx                    = collection->_indexes._buffer[j];
    TRI_skiplist_index_t* skiplistIndex = (TRI_skiplist_index_t*) idx;
    bool found                          = true;

        
    // .........................................................................
    // check that the type of the index is in fact a skiplist index 
    // .........................................................................
        
    if (idx->_type != TRI_IDX_TYPE_SKIPLIST_INDEX) {
      continue;
    }

        
    // .........................................................................
    // check that the number of shapes (fields) in the index matches that
    // of the number of attributes
    // .........................................................................
        
    if (shapes->_length != skiplistIndex->_shapeList->_length) {
      continue;
    }
        
        
    // .........................................................................
    // Go through all the attributes and see if they match
    // .........................................................................

    for (size_t k = 0; k < shapes->_length; ++k) {
      TRI_shape_pid_t field = *((TRI_shape_pid_t*)(TRI_AtVector(skiplistIndex->_shapeList,k)));   
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(shapes,k)));
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
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                               char const* location,
                                               bool geoJson) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  idx = CreateGeoIndexSimCollection(collection, location, NULL, NULL, geoJson, 0);

  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return 0;
  }

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&collection->base, idx);

  return ok ? idx->_iid : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                                char const* latitude,
                                                char const* longitude) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  idx = CreateGeoIndexSimCollection(collection, NULL, latitude, longitude, false, 0);

  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return 0;
  }

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&collection->base, idx);

  return ok ? idx->_iid : 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureHashIndexSimCollection(TRI_sim_collection_t* collection,
                                               const TRI_vector_t* attributes,
                                               bool unique) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);
  
  // ............................................................................. 
  // Given the list of attributes (as strings) 
  // .............................................................................

  idx = CreateHashIndexSimCollection(collection, attributes, 0, unique);
  
  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return 0;
  }

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&collection->base, idx);

  if (ok) {
  }  
  else {
    assert(0);
  }
  
  return ok ? idx->_iid : 0;
}                                                

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureSkiplistIndexSimCollection(TRI_sim_collection_t* collection,
                                                   const TRI_vector_t* attributes,
                                                   bool unique) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................
  TRI_WriteLockReadWriteLock(&collection->_lock);

  
  // ............................................................................. 
  // Given the list of attributes (as strings) 
  // .............................................................................

  idx = CreateSkiplistIndexSimCollection(collection, attributes, 0, unique);
  
  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);
    return 0;
  }

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&collection->base, idx);

  if (ok) {
  }  
  else {
    assert(0);
  }
  
  return ok ? idx->_iid : 0;
}                                                

                                                
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
