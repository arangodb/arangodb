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

static int CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                   TRI_doc_mptr_t* header);

static int UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                   TRI_doc_mptr_t const* header,
                                   TRI_doc_mptr_t const* update);

static int DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                   TRI_doc_mptr_t const* header,
                                   TRI_voc_tick_t);

static int DeleteDocument (TRI_sim_collection_t* collection,
                           TRI_doc_deletion_marker_t* marker,
                           TRI_voc_rid_t rid,
                           TRI_voc_rid_t* oldRid,
                           TRI_doc_update_policy_e policy,
                           bool release);

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

static int InsertPrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc);
static int  UpdatePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old);
static int RemovePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc);
static TRI_json_t* JsonPrimary (TRI_index_t* idx, TRI_doc_collection_t* collection);

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

static TRI_datafile_t* SelectJournal (TRI_sim_collection_t* sim,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  int res;
  size_t i;
  size_t n;

  TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

  while (true) {
    n = sim->base.base._journals._length;

    for (i = 0;  i < n;  ++i) {

      // select datafile
      datafile = sim->base.base._journals._buffer[i];

      // try to reserve space
      res = TRI_ReserveElementDatafile(datafile, size, result);

      // in case of full datafile, try next
      if (res == TRI_ERROR_NO_ERROR) {
        TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
        return datafile;
      }
      else if (res != TRI_ERROR_AVOCADO_DATAFILE_FULL) {
        TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
        return NULL;
      }
    }

    TRI_WAIT_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for synchronisation
///
/// Note that a datafile is never freed. If the datafile is closed the state
/// is set to TRI_DF_STATE_CLOSED - but the datafile pointer is still valid.
/// If a datafile is closed - then the data has been copied to some other
/// datafile and has been synced.
////////////////////////////////////////////////////////////////////////////////

static void WaitSync (TRI_sim_collection_t* sim,
                      TRI_datafile_t* journal,
                      char const* position) {
  TRI_collection_t* base;

  base = &sim->base.base;

  // no condition at all. Do NOT acquire a lock, in the worst
  // case we will miss a parameter change.

  if (! base->_waitForSync) {
    return;
  }

  TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

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
    TRI_WAIT_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes data to the journal and updates the barriers
////////////////////////////////////////////////////////////////////////////////

static int WriteElement (TRI_sim_collection_t* sim,
                         TRI_datafile_t* journal,
                         TRI_df_marker_t* marker,
                         TRI_voc_size_t markerSize,
                         void const* body,
                         TRI_voc_size_t bodySize,
                         TRI_df_marker_t* result) {
  int res;

  res = TRI_WriteElementDatafile(journal,
                                 result,
                                 marker, markerSize,
                                 body, bodySize,
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

  journal->_written = ((char*) result) + marker->_size;
  journal->_nWritten++;

  TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

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

static TRI_doc_mptr_t CreateDocument (TRI_sim_collection_t* collection,
                                      TRI_doc_document_marker_t* marker,
                                      size_t markerSize,
                                      void const* body,
                                      TRI_voc_size_t bodySize,
                                      TRI_df_marker_t** result,
                                      void const* additional,
                                      bool release) {

  TRI_datafile_t* journal;
  TRI_doc_mptr_t* header;
  TRI_doc_mptr_t mptr;
  TRI_voc_size_t total;
  TRI_doc_datafile_info_t* dfi;
  int res;
  int originalRes;

  // .............................................................................
  // create header
  // .............................................................................

  // get a new header pointer
  header = collection->_headers->request(collection->_headers);

  // generate a new tick
  marker->_rid = marker->_did = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = markerSize + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_AVOCADO_NO_JOURNAL);

    if (release) {
      collection->base.endWrite(&collection->base);
    }

    mptr._did = 0;
    return mptr;
  }

  // .............................................................................
  // write document blob
  // .............................................................................

  // verify the header pointer
  header = collection->_headers->verify(collection->_headers, header);

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, markerSize, body, bodySize);

  // and write marker and blob
  res = WriteElement(collection, journal, &marker->base, markerSize, body, bodySize, *result);

  // .............................................................................
  // update indexes
  // .............................................................................

  // generate create header
  if (res == TRI_ERROR_NO_ERROR) {

    // fill the header
    collection->base.createHeader(&collection->base, journal, *result, markerSize, header, 0);

    // update the datafile info
    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);

    dfi->_numberAlive += 1;
    dfi->_sizeAlive += header->_document._data.length;

    // update immediate indexes
    res = CreateImmediateIndexes(collection, header);
    originalRes = res;

    // check for constraint error
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_doc_deletion_marker_t markerDel;

      LOG_WARNING("encountered index violating during create, deleting newly created document");

      // .............................................................................
      // rollback
      // .............................................................................

      memset(&markerDel, 0, sizeof(markerDel));

      markerDel.base._size = sizeof(markerDel);
      markerDel.base._type = TRI_DOC_MARKER_DELETION;

      markerDel._did = header->_did;
      markerDel._sid = 0;

      // ignore any additional errors
      res = DeleteDocument(collection, &markerDel, header->_rid, NULL, TRI_DOC_UPDATE_LAST_WRITE, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("encountered error '%s' during rollback of create", TRI_last_error());
      }
    }

    // .............................................................................
    // create result
    // .............................................................................

    if (res == TRI_ERROR_NO_ERROR) {
      mptr = *header;

      // release lock, header might be invalid after this
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      // wait for sync
      WaitSync(collection, journal, ((char const*) *result) + markerSize + bodySize);

      // and return
      return mptr;
    }
    else {
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      TRI_set_errno(originalRes);

      mptr._did = 0;
      return mptr;
    }

  }
  else {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    LOG_ERROR("cannot write element: %s", TRI_last_error());
    mptr._did = 0;
    return mptr;
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

static TRI_doc_mptr_t const UpdateDocument (TRI_sim_collection_t* collection,
                                            TRI_doc_mptr_t const* header,
                                            TRI_doc_document_marker_t* marker,
                                            size_t markerSize,
                                            void const* body,
                                            TRI_voc_size_t bodySize,
                                            TRI_voc_rid_t rid,
                                            TRI_voc_rid_t* oldRid,
                                            TRI_doc_update_policy_e policy,
                                            TRI_df_marker_t** result,
                                            bool release,
                                            bool allowRollback) {
  TRI_doc_mptr_t mptr;
  TRI_datafile_t* journal;
  TRI_df_marker_t const* originalMarker;
  TRI_doc_mptr_t resUpd;
  TRI_shaped_json_t originalJson;
  TRI_voc_size_t total;
  int originalRes;
  int res;

  originalJson = header->_document;
  originalMarker = header->_data;

  // .............................................................................
  // check the revision
  // .............................................................................

  if (oldRid != NULL) {
    *oldRid = header->_rid;
  }

  switch (policy) {
    case TRI_DOC_UPDATE_ERROR:
      if (rid != 0) {
        if (rid != header->_rid) {
          TRI_set_errno(TRI_ERROR_AVOCADO_CONFLICT);

          if (release) {
            collection->base.endWrite(&collection->base);
          }

          mptr._did = 0;
          return mptr;
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      TRI_set_errno(TRI_ERROR_NOT_IMPLEMENTED);

      if (release) {
        collection->base.endWrite(&collection->base);
      }

      mptr._did = 0;
      return mptr;

    case TRI_DOC_UPDATE_ILLEGAL:
      TRI_set_errno(TRI_ERROR_INTERNAL);

      if (release) {
        collection->base.endWrite(&collection->base);
      }

      mptr._did = 0;
      return mptr;
  }

  // .............................................................................
  // update header
  // .............................................................................

  // generate a new tick
  marker->_rid = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = markerSize + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_AVOCADO_NO_JOURNAL);

    if (release) {
      collection->base.endWrite(&collection->base);
    }

    mptr._did = 0;
    return mptr;
  }

  // .............................................................................
  // write document blob
  // .............................................................................

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, markerSize, body, bodySize);

  // and write marker and blob
  res = WriteElement(collection, journal, &marker->base, markerSize, body, bodySize, *result);

  // .............................................................................
  // update indexes
  // .............................................................................

  // update the header
  if (res == TRI_ERROR_NO_ERROR) {
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
    res = UpdateImmediateIndexes(collection, header, &update);
    originalRes = res;

    // check for constraint error
    if (allowRollback && res != TRI_ERROR_NO_ERROR) {
      LOG_WARNING("encountered index violating during update, rolling back");

      // .............................................................................
      // rollback
      // .............................................................................

      if (originalMarker->_type == TRI_DOC_MARKER_DOCUMENT) {
        TRI_doc_document_marker_t markerUpd;
        
        // create an update
        memset(&markerUpd, 0, sizeof(markerUpd));
        
        markerUpd.base._size = sizeof(markerUpd) + originalJson._data.length;
        markerUpd.base._type = originalMarker->_type;
        
        markerUpd._did = header->_did;
        markerUpd._sid = 0;
        markerUpd._shape = originalJson._sid;
        
        resUpd = UpdateDocument(collection,
                                header,
                                &markerUpd, sizeof(markerUpd),
                                originalJson._data.data, originalJson._data.length,
                                header->_rid,
                                NULL,
                                TRI_DOC_UPDATE_LAST_WRITE,
                                result,
                                false,
                                false);
        
        if (resUpd._did == 0) {
          LOG_ERROR("encountered error '%s' during rollback of update", TRI_last_error());
        }
      }
      else if (originalMarker->_type == TRI_DOC_MARKER_EDGE) {
        TRI_doc_edge_marker_t markerUpd;
        TRI_doc_edge_marker_t const* originalEdge;
        
        originalEdge = (TRI_doc_edge_marker_t*) originalMarker;
        
        // create an update
        memset(&markerUpd, 0, sizeof(markerUpd));
        
        markerUpd.base.base._size = sizeof(markerUpd) + originalJson._data.length;
        markerUpd.base.base._type = originalMarker->_type;
        
        markerUpd.base._did = header->_did;
        markerUpd.base._sid = 0;
        markerUpd.base._shape = originalJson._sid;
        
        markerUpd._fromCid = originalEdge->_fromCid;
        markerUpd._fromDid = originalEdge->_fromDid;
        markerUpd._toCid = originalEdge->_toCid;
        markerUpd._toDid = originalEdge->_toDid;
        
        resUpd = UpdateDocument(collection,
                                header,
                                &markerUpd.base, sizeof(markerUpd),
                                originalJson._data.data, originalJson._data.length,
                                header->_rid,
                                NULL,
                                TRI_DOC_UPDATE_LAST_WRITE,
                                result,
                                false,
                                false);
        
        if (resUpd._did == 0) {
          LOG_ERROR("encountered error '%s' during rollback of update", TRI_last_error());
        }
      }
      else {
        LOG_ERROR("unknown document marker type '%lu' in document '%lu:%lu' revision '%lu'",
                  (unsigned long) originalMarker->_type,
                  (unsigned long) collection->base.base._cid,
                  (unsigned long) header->_did,
                  (unsigned long) header->_rid);
      }
    }

    // .............................................................................
    // create result
    // .............................................................................

    if (originalRes == TRI_ERROR_NO_ERROR) {
      mptr = *header;

      // release lock, header might be invalid after this
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      // wait for sync
      WaitSync(collection, journal, ((char const*) *result) + markerSize + bodySize);
      
      // and return
      return mptr;
    }
    else {
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      TRI_set_errno(originalRes);

      mptr._did = 0;
      return mptr;
    }

  }
  else {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    LOG_ERROR("cannot write element");
    mptr._did = 0;
    return mptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element and removes it from the index
////////////////////////////////////////////////////////////////////////////////

static int DeleteDocument (TRI_sim_collection_t* collection,
                           TRI_doc_deletion_marker_t* marker,
                           TRI_voc_rid_t rid,
                           TRI_voc_rid_t* oldRid,
                           TRI_doc_update_policy_e policy,
                           bool release) {
  TRI_datafile_t* journal;
  TRI_df_marker_t* result;
  TRI_doc_mptr_t const* header;
  TRI_voc_size_t total;
  int res;

  // get an existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &marker->_did);

  if (header == NULL || header->_deletion != 0) {
    if (release) {
      collection->base.endWrite(&collection->base);
    }

    return TRI_set_errno(TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND);
  }

  // check the revision
  if (oldRid != NULL) {
    *oldRid = header->_rid;
  }

  switch (policy) {
    case TRI_DOC_UPDATE_ERROR:
      if (rid != 0) {
        if (rid != header->_rid) {
          if (release) {
            collection->base.endWrite(&collection->base);
          }

          return TRI_ERROR_AVOCADO_CONFLICT;
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      return TRI_ERROR_NOT_IMPLEMENTED;

    case TRI_DOC_UPDATE_ILLEGAL:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      return TRI_ERROR_INTERNAL;
  }

  // generate a new tick
  marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_deletion_marker_t);
  journal = SelectJournal(collection, total, &result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_AVOCADO_NO_JOURNAL);

    if (release) {
      collection->base.endWrite(&collection->base);
    }

    return TRI_ERROR_AVOCADO_NO_JOURNAL;
  }

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0);

  // and write marker and blob
  res = WriteElement(collection, journal, &marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0, result);

  // update the header
  if (res == TRI_ERROR_NO_ERROR) {
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

static TRI_doc_mptr_t const CreateShapedJson (TRI_doc_collection_t* document,
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

static TRI_doc_mptr_t const ReadShapedJson (TRI_doc_collection_t* document,
                                            TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_mptr_t result;
  TRI_doc_mptr_t const* header;

  collection = (TRI_sim_collection_t*) document;

  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    result._did = 0;
    return result;
  }
  else {
    return *header;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const UpdateShapedJson (TRI_doc_collection_t* document,
                                              TRI_shaped_json_t const* json,
                                              TRI_voc_did_t did,
                                              TRI_voc_rid_t rid,
                                              TRI_voc_rid_t* oldRid,
                                              TRI_doc_update_policy_e policy,
                                              bool release) {
  TRI_df_marker_t const* original;
  TRI_df_marker_t* result;
  TRI_doc_mptr_t mptr;
  TRI_doc_mptr_t const* header;
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;

  // get an existing header pointer
  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    if (release) {
      document->endWrite(&collection->base);
    }

    TRI_set_errno(TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND);
    mptr._did = 0;
    return mptr;
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
                          oldRid,
                          policy,
                          &result,
                          release,
                          true);
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
                          oldRid,
                          policy,
                          &result,
                          release,
                          true);
  }

  // do not know
  else {
    if (release) {
      document->endWrite(&collection->base);
    }

    LOG_FATAL("unknown marker type %lu", (unsigned long) original->_type);
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static int DeleteShapedJson (TRI_doc_collection_t* document,
                             TRI_voc_did_t did,
                             TRI_voc_rid_t rid,
                             TRI_voc_rid_t* oldRid,
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

  return DeleteDocument(collection, &marker, rid, oldRid, policy, release);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginRead (TRI_doc_collection_t* doc) {
  TRI_sim_collection_t* sim;

  sim = (TRI_sim_collection_t*) doc;
  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndRead (TRI_doc_collection_t* doc) {
  TRI_sim_collection_t* sim;

  sim = (TRI_sim_collection_t*) doc;
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginWrite (TRI_doc_collection_t* doc) {
  TRI_sim_collection_t* sim;

  sim = (TRI_sim_collection_t*) doc;
  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndWrite (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* sim;

  sim = (TRI_sim_collection_t*) document;
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a document collection
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t SizeSimCollection (TRI_doc_collection_t* doc) {
  TRI_sim_collection_t* sim;
  TRI_voc_size_t result;

  sim = (TRI_sim_collection_t*) doc;

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  result = sim->_primaryIndex._nrUsed;
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

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

static bool OpenIndexIterator (char const* filename, void* data) {
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
  int j;

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
  // HASH INDEX OR SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash") || TRI_EqualString(typeStr, "skiplist")) {
  
    // Determine if the hash index is unique or non-unique
    gjs = TRI_LookupArrayJson(json, "unique");
    uniqueIndex = false;

    if (gjs != NULL && gjs->_type == TRI_JSON_BOOLEAN) {
      uniqueIndex = gjs->_value._boolean;
    }
    else {
      LOG_ERROR("ignore %s-index %lu, could not determine if unique or non-unique",
                typeStr,
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
      LOG_ERROR("ignore %s-index %lu, field count missing", typeStr, (unsigned long) iid);

      TRI_FreeJson(json);
      return false;
    }

    // Initialise the vector in which we store the fields on which the hashing
    // will be based.
    TRI_InitVector(&attributes, sizeof(char*));
    
    // find fields
    for (j = 0;  j < intCount;  ++j) {
      sprintf(fieldChar, "field_%i", j);

      fieldStr = TRI_LookupArrayJson(json, fieldChar);

      if (fieldStr->_type != TRI_JSON_STRING) {
        LOG_ERROR("ignore %s-index %lu, invalid field name for hash index",
                  (unsigned long) iid);

        TRI_DestroyVector(&attributes);
        TRI_FreeJson(json);
        return false;
      }  

      TRI_PushBackVector(&attributes, &(fieldStr->_value._string.data));
    }  

    // create the index
    if (TRI_EqualString(typeStr, "hash")) {
      idx = CreateHashIndexSimCollection (doc, &attributes, iid, uniqueIndex); 
    }
    else if (TRI_EqualString(typeStr, "skiplist")) {
      idx = CreateSkiplistIndexSimCollection (doc, &attributes, iid, uniqueIndex); 
    }
    else {
      LOG_ERROR("internal error for hash type '%s'", typeStr);
      idx = NULL;
    }

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

static bool InitSimCollection (TRI_sim_collection_t* collection,
                               TRI_shaper_t* shaper) {
  TRI_index_t* primary;
  char* id;

  TRI_InitDocCollection(&collection->base, shaper);

  TRI_InitReadWriteLock(&collection->_lock);

  collection->_headers = TRI_CreateSimpleHeaders(sizeof(TRI_doc_mptr_t));

  if (collection->_headers == NULL) {
    TRI_DestroyDocCollection(&collection->base);
    TRI_DestroyReadWriteLock(&collection->_lock);
    return false;
  }

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

  // create primary index
  primary = TRI_Allocate(sizeof(TRI_index_t));
  id = TRI_DuplicateString("_id");

  if (primary == NULL || id == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return false;
  }

  TRI_InitVectorString(&primary->_fields);
  TRI_PushBackVectorString(&primary->_fields, id);

  primary->_iid = 0;
  primary->_type = TRI_IDX_TYPE_PRIMARY_INDEX;
  primary->_unique = true;

  primary->insert = InsertPrimary;
  primary->remove = RemovePrimary;
  primary->update = UpdatePrimary;
  primary->json = JsonPrimary;

  TRI_PushBackVectorPointer(&collection->_indexes, primary);

  // setup methods
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

TRI_sim_collection_t* TRI_CreateSimCollection (char const* path,
                                               TRI_col_parameter_t* parameter,
                                               TRI_voc_cid_t cid) {
  TRI_col_info_t info;
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;

  memset(&info, 0, sizeof(info));
  info._version = TRI_COL_VERSION;
  info._type = parameter->_type;
  info._cid = cid == 0 ? TRI_NewTickVocBase() : cid;
  TRI_CopyString(info._name, parameter->_name, sizeof(info._name));
  info._maximalSize = parameter->_maximalSize;
  info._size = sizeof(TRI_col_info_t);

  // first create the document collection
  doc = TRI_Allocate(sizeof(TRI_sim_collection_t));

  if (doc == NULL) {
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
    TRI_FreeCollection(collection); // will free doc

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
  TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);

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

int TRI_CloseSimCollection (TRI_sim_collection_t* collection) {
  int res;

  res = TRI_CloseCollection(&collection->base.base);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_CloseVocShaper(collection->base._shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static int CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                   TRI_doc_mptr_t* header) {
  TRI_df_marker_t const* marker;
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  int result;
  bool constraint;

  // .............................................................................
  // update primary index
  // .............................................................................

  // add a new header
  found = TRI_InsertKeyAssociativePointer(&collection->_primaryIndex, &header->_did, header, false);

  if (found != NULL) {
    LOG_ERROR("document %lu already existed with revision %lu while creating revision %lu",
              (unsigned long) header->_did,
              (unsigned long) found->_rid,
              (unsigned long) header->_rid);

    collection->_headers->release(collection->_headers, header);
    return TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  // return in case of a delete
  if (header->_deletion != 0) {
    return TRI_ERROR_NO_ERROR;
  }

  // .............................................................................
  // update edges index
  // .............................................................................

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
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_IN;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;

    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    // OUT
    entry = TRI_Allocate(sizeof(TRI_edge_header_t));

    if (!entry) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_OUT;
    entry->_cid = edge->_fromCid;
    entry->_did = edge->_fromDid;

    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    // ANY
    entry = TRI_Allocate(sizeof(TRI_edge_header_t));

    if (!entry) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_ANY;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;

    TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);

    if (edge->_toCid != edge->_fromCid || edge->_toDid != edge->_fromDid) {
      entry = TRI_Allocate(sizeof(TRI_edge_header_t));

      if (!entry) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      entry->_mptr = header;
      entry->_direction = TRI_EDGE_ANY;
      entry->_cid = edge->_fromCid;
      entry->_did = edge->_fromDid;

      TRI_InsertElementMultiPointer(&collection->_edgesIndex, entry, true);
    }
  }

  // .............................................................................
  // update all the other indices
  // .............................................................................

  n = collection->_indexes._length;
  result = TRI_ERROR_NO_ERROR;
  constraint = false;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = collection->_indexes._buffer[i];
    res = idx->insert(idx, header);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }

    // "prefer" unique constraint violated
    if (res == TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED) {
      constraint = true;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      result = res;
    }
  }

  if (constraint) {
    return TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static int UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                   TRI_doc_mptr_t const* header,
                                   TRI_doc_mptr_t const* update) {

  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_shaped_json_t old;
  bool constraint;
  bool result;
  size_t i;
  size_t n;

  // get the old document
  old = header->_document;

  // .............................................................................
  // update primary index
  // .............................................................................

  // update all fields, the document identifier stays the same
  change.c = header;

  change.v->_rid = update->_rid;
  change.v->_eid = update->_eid;
  change.v->_fid = update->_fid;
  change.v->_deletion = update->_deletion;

  change.v->_data = update->_data;
  change.v->_document = update->_document;

  // .............................................................................
  // update all the other indices
  // .............................................................................

  n = collection->_indexes._length;
  result = TRI_ERROR_NO_ERROR;
  constraint = false;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = collection->_indexes._buffer[i];
    res = idx->update(idx, header, &old);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }

    // "prefer" unique constraint violated
    if (res == TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED) {
      constraint = true;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      result = res;
    }
  }

  if (constraint) {
    return TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static int DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_voc_tick_t deletion) {
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_df_marker_t const* marker;
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  bool result;

  // set the deletion flag
  change.c = header;
  change.v->_deletion = deletion;

  // .............................................................................
  // remove from main index
  // .............................................................................

  found = TRI_RemoveKeyAssociativePointer(&collection->_primaryIndex, &header->_did);

  if (found == NULL) {
    return TRI_set_errno(TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND);
  }

  // .............................................................................
  // update edges index
  // .............................................................................

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

  // .............................................................................
  // remove from all other indexes
  // .............................................................................

  n = collection->_indexes._length;
  result = TRI_ERROR_NO_ERROR;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = collection->_indexes._buffer[i];
    res = idx->remove(idx, header);

    if (res != TRI_ERROR_NO_ERROR) {
      result = res;
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
  TRI_doc_mptr_t const* mptr;
  size_t n;
  size_t scanned;
  void** end;
  void** ptr;
  int res;

  // update index
  n = collection->_primaryIndex._nrUsed;
  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  scanned = 0;

  for (;  ptr < end;  ++ptr) {
    if (*ptr != NULL) {
      mptr = *ptr;

      ++scanned;

      if (mptr->_deletion == 0) {
        res = idx->insert(idx, *ptr);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("failed to insert document '%lu:%lu'",
                      (unsigned long) collection->base.base._cid,
                      (unsigned long) mptr->_did);

          return false;
        }
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

TRI_vector_pointer_t* TRI_IndexesSimCollection (TRI_sim_collection_t* sim) {
  TRI_vector_pointer_t* vector;
  size_t n;
  size_t i;

  vector = TRI_Allocate(sizeof(TRI_vector_pointer_t));
  TRI_InitVectorPointer(vector);

  // .............................................................................
  // inside read-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  n = sim->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    TRI_json_t* json;

    idx = sim->_indexes._buffer[i];

    json = idx->json(idx, &sim->base);

    if (json != NULL) {
      TRI_PushBackVectorPointer(vector, json);
    }
  }

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside read-lock
  // .............................................................................

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of anindex
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_IndexSimCollection (TRI_sim_collection_t* sim, TRI_idx_iid_t iid) {
  TRI_index_t* idx = NULL;
  size_t n;
  size_t i;

  // .............................................................................
  // inside read-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  n = sim->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    idx = sim->_indexes._buffer[i];
    
    if (idx->_iid == iid) {
      break;
    }
  }

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside read-lock
  // .............................................................................

  return i < n ? idx : NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexSimCollection (TRI_sim_collection_t* sim, TRI_idx_iid_t iid) {
  TRI_index_t* found;
  size_t n;
  size_t i;

  if (iid == 0) {
    return true;
  }

  found = NULL;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  n = sim->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = sim->_indexes._buffer[i];

    if (idx->_iid == iid) {
      found = TRI_RemoveVectorPointer(&sim->_indexes, i);
      break;
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != NULL) {
    return TRI_RemoveIndexFile(&sim->base, found);
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
/// @brief insert methods does nothing
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update methods does nothing
////////////////////////////////////////////////////////////////////////////////

static int  UpdatePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove methods does nothing
////////////////////////////////////////////////////////////////////////////////

static int RemovePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonPrimary (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  TRI_json_t* json;

  json = TRI_CreateArrayJson();

  TRI_Insert3ArrayJson(json, "iid", TRI_CreateNumberJson(0));
  TRI_Insert3ArrayJson(json, "type", TRI_CreateStringCopyJson("primary"));
  TRI_Insert3ArrayJson(json, "fieldCount", TRI_CreateNumberJson(1));
  TRI_Insert3ArrayJson(json, "field_0", TRI_CreateStringCopyJson("_id"));

  return json;
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

static TRI_index_t* CreateGeoIndexSimCollection (TRI_sim_collection_t* sim,
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
  bool ok;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = NULL;

  shaper = sim->base._shaper;

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
    idx = TRI_LookupGeoIndexSimCollection(sim, loc, geoJson);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2SimCollection(sim, lat, lon);
  }
  else {
    TRI_set_errno(TRI_ERROR_INTERNAL);

    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");

    return NULL;
  }

  if (idx != NULL) {
    LOG_TRACE("geo-index already created for location '%s'", location);

    return idx;
  }

  // create a new index
  if (location != NULL) {
    idx = TRI_CreateGeoIndex(&sim->base, location, loc, geoJson);

    LOG_TRACE("created geo-index for location '%s': %d",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeoIndex2(&sim->base, latitude, lat, longitude, lon);

    LOG_TRACE("created geo-index for location '%s': %d, %d",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  ok = FillIndex(sim, idx);

  if (! ok) {
    TRI_FreeGeoIndex(idx);
    return NULL;
  }
  
  // and store index
  TRI_PushBackVectorPointer(&sim->_indexes, idx);

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
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  bool ok;
  size_t j;

  idx     = NULL;
  shaper = collection->base._shaper;
  
  TRI_InitVector(&paths, sizeof(TRI_shape_pid_t));
  TRI_InitVectorPointer(&fields);

  // ...........................................................................
  // Determine the shape ids for the attributes
  // ...........................................................................

  for (j = 0; j < attributes->_length; ++j) {
    char* path;
    TRI_shape_pid_t pid;

    path = *((char**)(TRI_AtVector(attributes,j)));    
    pid  = shaper->findAttributePathByName(shaper, path);   

    TRI_PushBackVectorPointer(&fields, path);
    TRI_PushBackVector(&paths, &pid);
  }
 
  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = TRI_LookupHashIndexSimCollection(collection, &paths);
  
  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("hash-index already created");

    return idx;
  }

  // ...........................................................................
  // Create the hash index
  // ...........................................................................

  idx = TRI_CreateHashIndex(&collection->base, &fields, &paths, unique);

  // ...........................................................................
  // release memory allocated to vector
  // ...........................................................................

  TRI_DestroyVector(&paths);
  
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
  TRI_vector_t paths;
  TRI_vector_pointer_t fields;
  bool ok;
  size_t j;
  
  TRI_InitVector(&paths, sizeof(TRI_shape_pid_t));
  TRI_InitVectorPointer(&fields);

  // ...........................................................................
  // Determine the shape ids for the attributes
  // ...........................................................................

  for (j = 0;  j < attributes->_length;  ++j) {
    char* path = *((char**)(TRI_AtVector(attributes,j)));    
    TRI_shape_pid_t shape = shaper->findAttributePathByName(shaper, path);   

    TRI_PushBackVector(&paths, &shape);
    TRI_PushBackVectorPointer(&fields, path);
  }
  
  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = TRI_LookupSkiplistIndexSimCollection(collection, &paths);
  
  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);

    LOG_TRACE("skiplist-index already created");
    return idx;
  }

  // ...........................................................................
  // Create the skiplist index
  // ...........................................................................

  idx = TRI_CreateSkiplistIndex(&collection->base, &fields, &paths, unique);

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
    TRI_FreeSkiplistIndex(idx);
    return NULL;
  }
  
  // ...........................................................................
  // store index
  // ...........................................................................

  TRI_PushBackVectorPointer(&collection->_indexes, idx);
  
  // ...........................................................................
  // release memory allocated to vector
  // ...........................................................................

  TRI_DestroyVector(&paths);
  
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
                                               const TRI_vector_t* paths) {
  TRI_index_t* matchedIndex = NULL;                                                                                        
  size_t j, k;
  
  // ...........................................................................
  // Note: This function does NOT differentiate between non-unique and unique
  //       hash indexes. The first hash index which matches the attributes
  //       (paths parameter) will be returned.
  // ...........................................................................
  
  
  // ........................................................................... 
  // go through every index and see if we have a match 
  // ........................................................................... 
  
  for (j = 0;  j < collection->_indexes._length;  ++j) {
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
    // check that the number of paths (fields) in the hash index matches that
    // of the number of attributes
    // .........................................................................
        
    if (paths->_length != hashIndex->_paths._length) {
      continue;
    }
        
    // .........................................................................
    // Go through all the attributes and see if they match
    // .........................................................................

    for (k = 0; k < paths->_length; ++k) {
      TRI_shape_pid_t field = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths,k)));   
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
/// @brief finds a skiplist index (unique or non-unique)
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                   const TRI_vector_t* paths) {
  TRI_index_t* matchedIndex = NULL;                                                                                        
  size_t j, k;
  
  // ...........................................................................
  // Note: This function does NOT differentiate between non-unique and unique
  //       skiplist indexes. The first index which matches the attributes
  //       (paths parameter) will be returned.
  // ...........................................................................
  
  
  // ........................................................................... 
  // go through every index and see if we have a match 
  // ........................................................................... 
  
  for (j = 0;  j < collection->_indexes._length;  ++j) {
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
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................
        
    if (paths->_length != skiplistIndex->_paths._length) {
      continue;
    }
        
    // .........................................................................
    // Go through all the attributes and see if they match
    // .........................................................................

    for (k = 0; k < paths->_length; ++k) {
      TRI_shape_pid_t field = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,k)));   
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
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndexSimCollection (TRI_sim_collection_t* sim,
                                               char const* location,
                                               bool geoJson) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  idx = CreateGeoIndexSimCollection(sim, location, NULL, NULL, geoJson, 0);

  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return 0;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&sim->base, idx);

  return ok ? idx->_iid : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* sim,
                                                char const* latitude,
                                                char const* longitude) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  idx = CreateGeoIndexSimCollection(sim, NULL, latitude, longitude, false, 0);

  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return 0;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&sim->base, idx);

  return ok ? idx->_iid : 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureHashIndexSimCollection(TRI_sim_collection_t* sim,
                                               const TRI_vector_t* attributes,
                                               bool unique) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  // ............................................................................. 
  // Given the list of attributes (as strings) 
  // .............................................................................

  idx = CreateHashIndexSimCollection(sim, attributes, 0, unique);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return 0;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&sim->base, idx);

  return ok ? idx->_iid : 0;
}                                                

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureSkiplistIndexSimCollection(TRI_sim_collection_t* sim,
                                                   const TRI_vector_t* attributes,
                                                   bool unique) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  // ............................................................................. 
  // Given the list of attributes (as strings) 
  // .............................................................................

  idx = CreateSkiplistIndexSimCollection(sim, attributes, 0, unique);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return 0;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  ok = TRI_SaveIndex(&sim->base, idx);
  
  return ok ? idx->_iid : 0;
}                                                

                                                
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
