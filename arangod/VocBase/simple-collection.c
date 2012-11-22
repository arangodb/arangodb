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

static TRI_doc_mptr_t UpdateDocument (TRI_sim_collection_t* collection,
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
                                      bool allowRollback);

static int DeleteDocument (TRI_sim_collection_t* collection,
                           TRI_doc_deletion_marker_t* marker,
                           TRI_voc_rid_t rid,
                           TRI_voc_rid_t* oldRid,
                           TRI_doc_update_policy_e policy,
                           bool release);

static int DeleteShapedJson (TRI_doc_collection_t* doc,
                             TRI_voc_did_t did,
                             TRI_voc_rid_t rid,
                             TRI_voc_rid_t* oldRid,
                             TRI_doc_update_policy_e policy,
                             bool release);

static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key);

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element);

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element);

static int InsertPrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc);
static int  UpdatePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old);
static int RemovePrimary (TRI_index_t* idx, TRI_doc_mptr_t const* doc);
static TRI_json_t* JsonPrimary (TRI_index_t* idx, TRI_doc_collection_t const* collection);

static int CapConstraintFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t);

static int BitarrayIndexFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t);

static int GeoIndexFromJson (TRI_sim_collection_t* sim,
                             TRI_json_t* definition,
                             TRI_idx_iid_t iid);

static int HashIndexFromJson (TRI_sim_collection_t* sim,
                              TRI_json_t* definition,
                              TRI_idx_iid_t iid);

static int SkiplistIndexFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid);

static int PriorityQueueFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid);

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

  if (sim->base.base._maximumMarkerSize < size) {
    sim->base.base._maximumMarkerSize = size;
  }

  while (sim->base.base._state == TRI_COL_STATE_WRITE) {
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
      else if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
        TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
        return NULL;
      }
    }

    TRI_INC_SYNCHRONISER_WAITER_VOC_BASE(sim->base.base._vocbase);
    TRI_WAIT_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
    TRI_DEC_SYNCHRONISER_WAITER_VOC_BASE(sim->base.base._vocbase);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
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

static TRI_doc_mptr_t CreateDocument (TRI_sim_collection_t* sim,
                                      TRI_doc_document_marker_t* marker,
                                      size_t markerSize,
                                      void const* body,
                                      TRI_voc_size_t bodySize,
                                      TRI_df_marker_t** result,
                                      void const* additional,
                                      TRI_voc_did_t did,
                                      TRI_voc_rid_t rid,
                                      bool release) {

  TRI_datafile_t* journal;
  TRI_doc_mptr_t* header;
  TRI_doc_mptr_t mptr;
  TRI_voc_size_t total;
  TRI_doc_datafile_info_t* dfi;
  int res;
  
  // .............................................................................
  // create header
  // .............................................................................

  // get a new header pointer
  header = sim->_headers->request(sim->_headers);

  if (did > 0 && rid > 0) {
    // use existing document id & revision id
    marker->_did = did;
    marker->_rid = marker->base._tick = rid;
    TRI_UpdateTickVocBase(did);
    TRI_UpdateTickVocBase(rid);
  }
  else {
    // generate a new tick
    marker->_rid = marker->_did = marker->base._tick = TRI_NewTickVocBase();
  }

  // find and select a journal
  total = markerSize + bodySize;
  journal = SelectJournal(sim, total, result);

  if (journal == NULL) {
    if (release) {
      sim->base.endWrite(&sim->base);
    }

    memset(&mptr, 0, sizeof(mptr));
    return mptr;
  }

  // .............................................................................
  // write document blob
  // .............................................................................

  // verify the header pointer
  header = sim->_headers->verify(sim->_headers, header);

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, markerSize, body, bodySize);

  // and write marker and blob
  res = WriteElement(sim, journal, &marker->base, markerSize, body, bodySize, *result);

  // .............................................................................
  // update indexes
  // .............................................................................

  // generate create header
  if (res == TRI_ERROR_NO_ERROR) {

    // fill the header
    sim->base.createHeader(&sim->base, journal, *result, markerSize, header, 0);

    // update the datafile info
    dfi = TRI_FindDatafileInfoDocCollection(&sim->base, journal->_fid);
    if (dfi != NULL) {
      dfi->_numberAlive += 1;
      dfi->_sizeAlive += header->_document._data.length;
    }

    // update immediate indexes
    res = CreateImmediateIndexes(sim, header);

    // check for constraint error, rollback if necessary
    if (res != TRI_ERROR_NO_ERROR) {
      int resRollback;

      LOG_DEBUG("encountered index violation during create, deleting newly created document");

      // rollback, ignore any additional errors
      resRollback = DeleteShapedJson(&sim->base, header->_did, header->_rid, 0, TRI_DOC_UPDATE_LAST_WRITE, false);

      if (resRollback != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("encountered error '%s' during rollback of create", TRI_last_error());
      }

      TRI_set_errno(res);
    }

    // .............................................................................
    // create result
    // .............................................................................

    if (res == TRI_ERROR_NO_ERROR) {
      mptr = *header;

      // check cap constraint
      if (sim->base._capConstraint != NULL) {
        while (sim->base._capConstraint->_size < sim->base._capConstraint->_array._array._nrUsed) {
          TRI_doc_mptr_t const* oldest;
          int remRes;

          oldest = TRI_PopFrontLinkedArray(&sim->base._capConstraint->_array);

          if (oldest == NULL) {
            LOG_WARNING("cap collection is empty, but collection '%ld' contains elements", 
                        (unsigned long) sim->base.base._cid);
            break;
          }

          LOG_DEBUG("removing document '%lu' because of cap constraint",
                    (unsigned long) oldest->_did);

          remRes = DeleteShapedJson(&sim->base, oldest->_did, 0, NULL, TRI_DOC_UPDATE_LAST_WRITE, false);

          if (remRes != TRI_ERROR_NO_ERROR) {
            LOG_WARNING("cannot cap collection: %s", TRI_last_error());
            break;
          }
        }
      }

      // release lock, header might be invalid after this
      if (release) {
        sim->base.endWrite(&sim->base);
      }

      // wait for sync
      WaitSync(sim, journal, ((char const*) *result) + markerSize + bodySize);

      // and return
      return mptr;
    }
    else {
      if (release) {
        sim->base.endWrite(&sim->base);
      }

      mptr._did = 0;
      return mptr;
    }

  }
  else {
    if (release) {
      sim->base.endWrite(&sim->base);
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
/// @brief roll backs an update
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t RollbackUpdate (TRI_sim_collection_t* sim,
                                      TRI_doc_mptr_t const* header,
                                      TRI_df_marker_t const* originalMarker,
                                      TRI_df_marker_t** result) {
  TRI_doc_document_marker_t* marker;
  TRI_doc_document_marker_t documentUpdate;
  TRI_doc_edge_marker_t edgeUpdate;
  char* data;
  size_t dataLength;
  size_t markerLength;

  if (originalMarker->_type == TRI_DOC_MARKER_DOCUMENT) {
    memcpy(&documentUpdate, originalMarker, sizeof(TRI_doc_document_marker_t));
    marker = &documentUpdate;
    markerLength = sizeof(TRI_doc_document_marker_t);
    data = ((char*) originalMarker) + sizeof(TRI_doc_document_marker_t);
    dataLength = originalMarker->_size - sizeof(TRI_doc_document_marker_t);
  }
  else if (originalMarker->_type == TRI_DOC_MARKER_EDGE) {
    memcpy(&edgeUpdate, originalMarker, sizeof(TRI_doc_edge_marker_t));
    marker = &edgeUpdate.base;
    markerLength = sizeof(TRI_doc_edge_marker_t);
    data = ((char*) originalMarker) + sizeof(TRI_doc_edge_marker_t);
    dataLength = originalMarker->_size - sizeof(TRI_doc_edge_marker_t);
  }
  else {
    TRI_doc_mptr_t mptr;
    TRI_set_errno(TRI_ERROR_INTERNAL);

    memset(&mptr, 0, sizeof(mptr));
    return mptr;
  }

  return UpdateDocument(sim,
                        header,
                        marker, markerLength,
                        data, dataLength,
                        header->_rid,
                        NULL,
                        TRI_DOC_UPDATE_LAST_WRITE,
                        result,
                        false,
                        false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t UpdateDocument (TRI_sim_collection_t* collection,
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
  TRI_voc_size_t total;
  int res;

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
          if (release) {
            collection->base.endWrite(&collection->base);
          }

          TRI_set_errno(TRI_ERROR_ARANGO_CONFLICT);
          memset(&mptr, 0, sizeof(mptr));
          return mptr;
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      TRI_set_errno(TRI_ERROR_NOT_IMPLEMENTED);
      mptr._did = 0;
      return mptr;

    case TRI_DOC_UPDATE_ILLEGAL:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      TRI_set_errno(TRI_ERROR_INTERNAL);
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
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);

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
    if (dfi != NULL) {
      dfi->_numberAlive -= 1;
      dfi->_sizeAlive -= header->_document._data.length;

      dfi->_numberDead += 1;
      dfi->_sizeDead += header->_document._data.length;
    }

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);
    if (dfi != NULL) {
      dfi->_numberAlive += 1;
      dfi->_sizeAlive += update._document._data.length;
    }

    // update immediate indexes
    res = UpdateImmediateIndexes(collection, header, &update);

    // check for constraint error
    if (allowRollback && res != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("encountered index violating during update, rolling back");

      resUpd = RollbackUpdate(collection, header, originalMarker, result);

      if (resUpd._did == 0) {
        LOG_ERROR("encountered error '%s' during rollback of update", TRI_last_error());
      }

      TRI_set_errno(res);
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

    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
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

          return TRI_set_errno(TRI_ERROR_ARANGO_CONFLICT);
        }
      }

      break;

    case TRI_DOC_UPDATE_LAST_WRITE:
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      return TRI_set_errno(TRI_ERROR_NOT_IMPLEMENTED);

    case TRI_DOC_UPDATE_ILLEGAL:
      if (release) {
        collection->base.endWrite(&collection->base);
      }

      return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // generate a new tick
  marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_deletion_marker_t);
  journal = SelectJournal(collection, total, &result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);

    if (release) {
      collection->base.endWrite(&collection->base);
    }

    return TRI_ERROR_ARANGO_NO_JOURNAL;
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
    if (dfi != NULL) {
      dfi->_numberAlive -= 1;
      dfi->_sizeAlive -= header->_document._data.length;

      dfi->_numberDead += 1;
      dfi->_sizeDead += header->_document._data.length;
    }

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, journal->_fid);
    if (dfi != NULL) {
      dfi->_numberDeletion += 1;
    }

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
  printf("  deletion:     %ld\n\n", (long) dfi->_numberDeletion);
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

static TRI_doc_mptr_t CreateShapedJson (TRI_doc_collection_t* document,
                                        TRI_df_marker_type_e type,
                                        TRI_shaped_json_t const* json,
                                        void const* data,
                                        TRI_voc_did_t did,
                                        TRI_voc_rid_t rid,
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
                          did,
                          rid,
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
                          did,
                          rid,
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

static TRI_doc_mptr_t ReadShapedJson (TRI_doc_collection_t* document,
                                      TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_mptr_t result;
  TRI_doc_mptr_t const* header;

  collection = (TRI_sim_collection_t*) document;

  header = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    memset(&result, 0, sizeof(result));
    return result;
  }
  else {
    return *header;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t UpdateShapedJson (TRI_doc_collection_t* document,
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

    TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    memset(&mptr, 0, sizeof(mptr));
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

static int DeleteShapedJson (TRI_doc_collection_t* doc,
                             TRI_voc_did_t did,
                             TRI_voc_rid_t rid,
                             TRI_voc_rid_t* oldRid,
                             TRI_doc_update_policy_e policy,
                             bool release) {
  TRI_sim_collection_t* sim;
  TRI_doc_deletion_marker_t marker;

  sim = (TRI_sim_collection_t*) doc;

  memset(&marker, 0, sizeof(marker));

  marker.base._size = sizeof(marker);
  marker.base._type = TRI_DOC_MARKER_DELETION;

  marker._did = did;
  marker._sid = 0;

  return DeleteDocument(sim, &marker, rid, oldRid, policy, release);
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
  TRI_doc_mptr_t const* mptr;
  TRI_sim_collection_t* sim;
  TRI_voc_size_t result;
  void** end;
  void** ptr;

  sim = (TRI_sim_collection_t*) doc;

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  ptr = sim->_primaryIndex._table;
  end = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc;
  result = 0;

  for (;  ptr < end;  ++ptr) {
    if (*ptr != NULL) {
      mptr = *ptr;

      if (mptr->_deletion == 0) {
        ++result;
      }
    }
  }

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

    if (collection->base.base._maximumMarkerSize < markerSize) {
      collection->base.base._maximumMarkerSize = markerSize;
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

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += header->_document._data.length;
      }

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

      if (dfi != NULL) {
        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= found->_document._data.length;

        dfi->_numberDead += 1;
        dfi->_sizeDead += found->_document._data.length;
      }

      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberAlive += 1;
        dfi->_sizeAlive += update._document._data.length;
      }

      // update immediate indexes
      UpdateImmediateIndexes(collection, found, &update);
    }

    // it is a stale update
    else {
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberDead += 1;
        dfi->_sizeDead += found->_document._data.length;
      }
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

      if (dfi != NULL) {
        dfi->_numberDeletion += 1;
      }
    }

    // it is a real delete
    else if (found->_deletion == 0) {
      union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;

      // mark element as deleted
      change.c = found;
      change.v->_deletion = marker->_tick;

      // update the datafile info
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, found->_fid);

      if (dfi != NULL) {
        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= found->_document._data.length;

        dfi->_numberDead += 1;
        dfi->_sizeDead += found->_document._data.length;
      }
      dfi = TRI_FindDatafileInfoDocCollection(&collection->base, datafile->_fid);

      if (dfi != NULL) {
        dfi->_numberDeletion += 1;
      }
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
  TRI_json_t* iis;
  TRI_json_t* json;
  TRI_json_t* type;
  TRI_sim_collection_t* sim;
  char const* typeStr;
  char* error;
  int res;

  // load json description of the index
  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  // simple collection of the index
  sim = (TRI_sim_collection_t*) data;

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

  // ...........................................................................
  // CAP CONSTRAINT
  // ...........................................................................

  if (TRI_EqualString(typeStr, "cap")) {
    res = CapConstraintFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  
  // ...........................................................................
  // BITARRAY INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "bitarray")) {
    res = BitarrayIndexFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }
  
  // ...........................................................................
  // GEO INDEX (list or attribute)
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "geo1") || TRI_EqualString(typeStr, "geo2")) {
    res = GeoIndexFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }
  
  // ...........................................................................
  // HASH INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash")) {
    res = HashIndexFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "skiplist")) {
    res = SkiplistIndexFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // ...........................................................................
  // PRIORITY QUEUE
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "priorityqueue")) {
    res = PriorityQueueFromJson(sim, json, iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return res == TRI_ERROR_NO_ERROR;
  }

  // .........................................................................
  // oops, unknown index type
  // .........................................................................

  else {
    LOG_ERROR("ignoring unknown index type '%s' for index %lu", 
              typeStr,
              (unsigned long) iid);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
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
  
  // create primary index
  primary = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_t), false);
  if (primary == NULL) {
    return false;
  }

  id = TRI_DuplicateString("_id");

  TRI_InitDocCollection(&collection->base, shaper);

  TRI_InitReadWriteLock(&collection->_lock);

  collection->_headers = TRI_CreateSimpleHeaders(sizeof(TRI_doc_mptr_t));

  if (collection->_headers == NULL) {
    TRI_DestroyDocCollection(&collection->base);
    TRI_DestroyReadWriteLock(&collection->_lock);
    return false;
  }

  TRI_InitAssociativePointer(&collection->_primaryIndex,
                             TRI_UNKNOWN_MEM_ZONE, 
                             HashKeyHeader,
                             HashElementDocument,
                             IsEqualKeyDocument,
                             0);

  TRI_InitMultiPointer(&collection->_edgesIndex,
                       TRI_UNKNOWN_MEM_ZONE, 
                       HashElementEdge,
                       HashElementEdge,
                       IsEqualKeyEdge,
                       IsEqualElementEdge);

  TRI_InitCondition(&collection->_journalsCondition);

  TRI_InitVectorPointer(&collection->_indexes, TRI_UNKNOWN_MEM_ZONE);

  TRI_InitVectorString(&primary->_fields, TRI_UNKNOWN_MEM_ZONE);
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

TRI_sim_collection_t* TRI_CreateSimCollection (TRI_vocbase_t* vocbase,
                                               char const* path,
                                               TRI_col_parameter_t* parameter,
                                               TRI_voc_cid_t cid) {
  TRI_col_info_t info;
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;

  memset(&info, 0, sizeof(info));
  info._version = TRI_COL_VERSION;
  info._type = parameter->_type;

  if (cid > 0) {
    TRI_UpdateTickVocBase(cid);
  }
  else {
    cid = TRI_NewTickVocBase();
  }
  info._cid = cid;
  TRI_CopyString(info._name, parameter->_name, sizeof(info._name));
  info._waitForSync = parameter->_waitForSync;
  info._maximalSize = parameter->_maximalSize;

  // first create the document collection
  doc = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sim_collection_t), false);

  if (doc == NULL) {
    LOG_ERROR("cannot create document");
    return NULL;
  }

  collection = TRI_CreateCollection(vocbase, &doc->base.base, path, &info);

  if (collection == NULL) {
    LOG_ERROR("cannot create document collection");

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, doc);
    return NULL;
  }

  // then the shape collection
  shaper = TRI_CreateVocShaper(vocbase, collection->_directory, "SHAPES");

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
  size_t i;
  size_t n;

  TRI_DestroyCondition(&collection->_journalsCondition);

  TRI_DestroyAssociativePointer(&collection->_primaryIndex);

  // free all elements in the edges index
  n = collection->_edgesIndex._nrAlloc;
  for (i = 0; i < n; ++i) {
    void* element = collection->_edgesIndex._table[i];
    if (element) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
    }
  }
  TRI_DestroyMultiPointer(&collection->_edgesIndex);

  TRI_FreeSimpleHeaders(collection->_headers);

  TRI_DestroyReadWriteLock(&collection->_lock);
 
  // free memory allocated for index field names
  n = collection->_indexes._length;
  for (i = 0 ; i < n ; ++i) {
    TRI_index_t* idx = (TRI_index_t*) collection->_indexes._buffer[i];
  
    TRI_FreeIndex(idx);
  }
  // free index vector
  TRI_DestroyVectorPointer(&collection->_indexes);

  TRI_DestroyDocCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimCollection (TRI_sim_collection_t* collection) {
  TRI_DestroySimCollection(collection);
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

TRI_sim_collection_t* TRI_OpenSimCollection (TRI_vocbase_t* vocbase, char const* path) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;
  char* shapes;

  // first open the document collection
  doc = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sim_collection_t), false);
  if (!doc) {
    return NULL;
  }

  collection = TRI_OpenCollection(vocbase, &doc->base.base, path);

  if (collection == NULL) {
    LOG_ERROR("cannot open document collection");

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, doc);
    return NULL;
  }

  // then the shape collection
  shapes = TRI_Concatenate2File(collection->_directory, "SHAPES");
  if (!shapes) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, doc);
    return NULL;
  }

  shaper = TRI_OpenVocShaper(vocbase, shapes);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, shapes);

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

  if (collection->_maximalSize < collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD) {
    LOG_WARNING("maximal size is %lu, but maximal marker size is %lu plus overhead %lu: adjusting maximal size to %lu",
                (unsigned long) collection->_maximalSize,
                (unsigned long) collection->_maximumMarkerSize,
                (unsigned long) TRI_JOURNAL_OVERHEAD,
                (unsigned long) (collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD));

    collection->_maximalSize = collection->_maximumMarkerSize + TRI_JOURNAL_OVERHEAD;
  }


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
    LOG_ERROR("ignoring index %lu, 'fields' must be a list", (unsigned long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return NULL;
  }

  *fieldCount = fld->_value._objects._length;

  for (j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* sub = TRI_AtVector(&fld->_value._objects, j);
      
    if (sub->_type != TRI_JSON_STRING) {
      LOG_ERROR("ignoring index %lu, 'fields' must be a list of attribute paths", (unsigned long) iid);

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
    LOG_ERROR("ignoring index %lu, 'fields' must be a list", (unsigned long) iid);

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
      LOG_ERROR("ignoring index %lu, 'fields' must be a list of key value pairs", (unsigned long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }    

    
    // .........................................................................   
    // Extract the key
    // .........................................................................   
  
    key = TRI_AtVector(&keyValue->_value._objects, 0);
    
    if (key == NULL || key->_type != TRI_JSON_STRING) {
      LOG_ERROR("ignoring index %lu, key in 'fields' pair must be an attribute (string)", (unsigned long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }
    
    
    // .........................................................................   
    // Extract the value
    // .........................................................................   

    value = TRI_AtVector(&keyValue->_value._objects, 1);
    
    if (value == NULL || value->_type != TRI_JSON_LIST) {
      LOG_ERROR("ignoring index %lu, value in 'fields' pair must be a list ([...])", (unsigned long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }
    
  }

  return keyValues;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static int CreateImmediateIndexes (TRI_sim_collection_t* sim,
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
  found = TRI_InsertKeyAssociativePointer(&sim->_primaryIndex, &header->_did, header, false);

  if (found != NULL) {
    LOG_ERROR("document %lu already existed with revision %lu while creating revision %lu",
              (unsigned long) header->_did,
              (unsigned long) found->_rid,
              (unsigned long) header->_rid);

    sim->_headers->release(sim->_headers, header);
    return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
  }

  // return in case of a deleted document
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
    entry = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
    /* TODO FIXME: memory allocation might fail */

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_IN;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;

    TRI_InsertElementMultiPointer(&sim->_edgesIndex, entry, true);

    // OUT
    entry = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
    /* TODO FIXME: memory allocation might fail */

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_OUT;
    entry->_cid = edge->_fromCid;
    entry->_did = edge->_fromDid;

    TRI_InsertElementMultiPointer(&sim->_edgesIndex, entry, true);

    // ANY
    entry = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
    /* TODO FIXME: memory allocation might fail */

    entry->_mptr = header;
    entry->_direction = TRI_EDGE_ANY;
    entry->_cid = edge->_toCid;
    entry->_did = edge->_toDid;

    TRI_InsertElementMultiPointer(&sim->_edgesIndex, entry, true);

    if (edge->_toCid != edge->_fromCid || edge->_toDid != edge->_fromDid) {
      entry = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), true);
      /* TODO FIXME: memory allocation might fail */

      entry->_mptr = header;
      entry->_direction = TRI_EDGE_ANY;
      entry->_cid = edge->_fromCid;
      entry->_did = edge->_fromDid;

      TRI_InsertElementMultiPointer(&sim->_edgesIndex, entry, true);
    }
  }

  // .............................................................................
  // update all the other indices
  // .............................................................................

  n = sim->_indexes._length;
  result = TRI_ERROR_NO_ERROR;
  constraint = false;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;
    int res;

    idx = sim->_indexes._buffer[i];
    res = idx->insert(idx, header);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }

    // "prefer" unique constraint violated
    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
      constraint = true;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      result = res;
    }
  }

  if (constraint) {
    return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
  }

  if (result != TRI_ERROR_NO_ERROR) {
    return TRI_set_errno(result);
  }

  return TRI_ERROR_NO_ERROR;
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
  int result;
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
    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
      constraint = true;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      result = res;
    }
  }

  if (constraint) {
    return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
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
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
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
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
    }

    // OUT
    entry._direction = TRI_EDGE_OUT;
    entry._cid = edge->_fromCid;
    entry._did = edge->_fromDid;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
    }

    // ANY
    entry._direction = TRI_EDGE_ANY;
    entry._cid = edge->_toCid;
    entry._did = edge->_toDid;
    old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

    if (old != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
    }

    if (edge->_toCid != edge->_fromCid || edge->_toDid != edge->_fromDid) {
      entry._direction = TRI_EDGE_ANY;
      entry._cid = edge->_fromCid;
      entry._did = edge->_fromDid;
      old = TRI_RemoveElementMultiPointer(&collection->_edgesIndex, &entry);

      if (old != NULL) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
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

static int FillIndex (TRI_sim_collection_t* collection, TRI_index_t* idx) {
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
          LOG_WARNING("failed to insert document '%lu:%lu' for index '%lu'",
                      (unsigned long) collection->base.base._cid,
                      (unsigned long) mptr->_did,
                      (unsigned long) idx->_iid);

          return res;
        }
      }

      if (scanned % 10000 == 0) {
        LOG_INFO("indexed %lu of %lu documents", (unsigned long) scanned, (unsigned long) n);
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief finds a path based, unique or non-unique index
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* LookupPathIndexSimCollection (TRI_sim_collection_t* collection,
                                                  TRI_vector_t const* paths,
                                                  TRI_idx_type_e type,
                                                  bool unique) {
  TRI_index_t* matchedIndex = NULL;                                                                                        
  TRI_vector_t* indexPaths;
  size_t j;
  size_t k;

  // ...........................................................................
  // go through every index and see if we have a match 
  // ...........................................................................
  
  for (j = 0;  j < collection->_indexes._length;  ++j) {
    TRI_index_t* idx = collection->_indexes._buffer[j];
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

static int BitarrayBasedIndexFromJson (TRI_sim_collection_t* sim,
                                       TRI_json_t* definition,
                                       TRI_idx_iid_t iid,
                                       TRI_index_t* (*creator)(TRI_sim_collection_t*,
                                                               const TRI_vector_pointer_t*,
                                                               const TRI_vector_pointer_t*,
                                                               TRI_idx_iid_t,
                                                               bool,
                                                               bool*)) {
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
    LOG_ERROR("ignoring index %lu, need at least one attribute path and one list of values",(unsigned long) iid);
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
    LOG_ERROR("ignoring index %lu, could not determine if unique or non-unique", (unsigned long) iid);
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
    LOG_ERROR("ignoring index %lu, could not determine if index supports undefined values", (unsigned long) iid);
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
  
  idx = creator(sim, &attributes, &values, iid, supportUndef, &created);


  // ...........................................................................
  // cleanup
  // ...........................................................................
  
  TRI_DestroyVectorPointer(&attributes);
  TRI_DestroyVectorPointer(&values);
  

  // ...........................................................................
  // Check if the creation or lookup succeeded
  // ...........................................................................
  
  if (idx == NULL) {
    LOG_ERROR("cannot create bitarray index %lu", (unsigned long) iid);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief restores a path based index (template)
////////////////////////////////////////////////////////////////////////////////

static int PathBasedIndexFromJson (TRI_sim_collection_t* sim,
                                   TRI_json_t* definition,
                                   TRI_idx_iid_t iid,
                                   TRI_index_t* (*creator)(TRI_sim_collection_t*,
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
    LOG_ERROR("ignoring index %lu, need at least von attribute path",(unsigned long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the hash index is unique or non-unique
  unique = false;
  bv = TRI_LookupArrayJson(definition, "unique");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    unique = bv->_value._boolean;
  }
  else {
    LOG_ERROR("ignoring index %lu, could not determine if unique or non-unique", (unsigned long) iid);
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
  idx = creator(sim, &attributes, iid, unique, NULL);

  // cleanup
  TRI_DestroyVectorPointer(&attributes);

  if (idx == NULL) {
    LOG_ERROR("cannot create hash index %lu", (unsigned long) iid);
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
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesSimCollection (TRI_sim_collection_t* sim,
                                                const bool lock) {
  TRI_vector_pointer_t* vector;
  size_t n;
  size_t i;

  vector = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);
  if (!vector) {
    return NULL;
  }

  TRI_InitVectorPointer(vector, TRI_UNKNOWN_MEM_ZONE);

  // .............................................................................
  // inside read-lock
  // .............................................................................

  if (lock) {
    TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  }

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

  if (lock) {
    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  }

  // .............................................................................
  // outside read-lock
  // .............................................................................

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexSimCollection (TRI_sim_collection_t* sim, TRI_idx_iid_t iid) {
  TRI_index_t* found;
  bool removeResult;
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

      if (found != NULL) {
        found->removeIndex(found, &sim->base);
      }

      break;
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != NULL) {
    removeResult = TRI_RemoveIndexFile(&sim->base, found);
    TRI_FreeIndex(found);
    return removeResult;
  }
  else {
    return false;
  }
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

static TRI_json_t* JsonPrimary (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;

  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "_id"));

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 0));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "primary"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

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

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  entry._direction = direction;
  entry._cid = cid;
  entry._did = did;

  found = TRI_LookupByKeyMultiPointer(TRI_UNKNOWN_MEM_ZONE, &edges->_edgesIndex, &entry);

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

static TRI_index_t* CreateCapConstraintSimCollection (TRI_sim_collection_t* sim,
                                                      size_t size,
                                                      TRI_idx_iid_t iid,
                                                      bool* created) {
  TRI_index_t* idx;
  int res;

  if (created != NULL) {
    *created = false;
  }

  // check if we already know a cap constraint
  if (sim->base._capConstraint != NULL) {
    if (sim->base._capConstraint->_size == size) {
      return &sim->base._capConstraint->base;
    }
    else {
      TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
      return NULL;
    }
  }

  // create a new index
  idx = TRI_CreateCapConstraint(&sim->base, size);

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(sim, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeCapConstraint(idx);
    return NULL;
  }
  
  // and store index
  TRI_PushBackVectorPointer(&sim->_indexes, idx);
  sim->base._capConstraint = (TRI_cap_constraint_t*) idx;

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int CapConstraintFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  TRI_json_t* num;
  TRI_index_t* idx;
  size_t size;

  num = TRI_LookupArrayJson(definition, "size");

  if (num == NULL || num->_type != TRI_JSON_NUMBER) {
    LOG_ERROR("ignoring cap constraint %lu, 'size' missing", (unsigned long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  if (num->_value._number < 1.0) {
    LOG_ERROR("ignoring cap constraint %lu, 'size' %f must be at least 1", (unsigned long) iid, num->_value._number);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  size = (size_t) num->_value._number; 

  idx = CreateCapConstraintSimCollection(sim, size, iid, NULL);

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

TRI_index_t* TRI_EnsureCapConstraintSimCollection (TRI_sim_collection_t* sim,
                                                   size_t size,
                                                   bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  idx = CreateCapConstraintSimCollection(sim, size, 0, created);

  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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
                                                 bool constraint,
                                                 bool ignoreNull,
                                                 TRI_idx_iid_t iid,
                                                 bool* created) {
  TRI_index_t* idx;
  TRI_shape_pid_t lat;
  TRI_shape_pid_t loc;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;
  int res;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = NULL;

  shaper = sim->base._shaper;

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
    idx = TRI_LookupGeoIndex1SimCollection(sim, loc, geoJson, constraint, ignoreNull);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2SimCollection(sim, lat, lon, constraint, ignoreNull);
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
    idx = TRI_CreateGeo1Index(&sim->base, location, loc, geoJson, constraint, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeo2Index(&sim->base, latitude, lat, longitude, lon, constraint, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld, %ld",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  res = FillIndex(sim, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);
    return NULL;
  }
  
  // and store index
  TRI_PushBackVectorPointer(&sim->_indexes, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int GeoIndexFromJson (TRI_sim_collection_t* sim,
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

      idx = CreateGeoIndexSimCollection(sim,
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
      LOG_ERROR("ignoring %s-index %lu, 'fields' must be a list with 1 entries",
                typeStr, (unsigned long) iid);
        
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

      idx = CreateGeoIndexSimCollection(sim,
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
      LOG_ERROR("ignoring %s-index %lu, 'fields' must be a list with 2 entries",
                typeStr, (unsigned long) iid);

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  else {
    assert(false);
  }
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

TRI_index_t* TRI_LookupGeoIndex1SimCollection (TRI_sim_collection_t* collection,
                                              TRI_shape_pid_t location,
                                               bool geoJson,
                                               bool constraint,
                                               bool ignoreNull) {
  size_t n;
  size_t i;

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

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

TRI_index_t* TRI_LookupGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                               TRI_shape_pid_t latitude,
                                               TRI_shape_pid_t longitude,
                                               bool constraint,
                                               bool ignoreNull) {
  size_t n;
  size_t i;

  n = collection->_indexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = collection->_indexes._buffer[i];

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

TRI_index_t* TRI_EnsureGeoIndex1SimCollection (TRI_sim_collection_t* sim,
                                               char const* location,
                                               bool geoJson,
                                               bool constraint,
                                               bool ignoreNull,
                                               bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  idx = CreateGeoIndexSimCollection(sim, location, NULL, NULL, geoJson, constraint, ignoreNull, 0, created);

  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* sim,
                                               char const* latitude,
                                               char const* longitude,
                                               bool constraint,
                                               bool ignoreNull,
                                               bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  idx = CreateGeoIndexSimCollection(sim, NULL, latitude, longitude, false, constraint, ignoreNull, 0, created);

  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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

static TRI_index_t* CreateHashIndexSimCollection (TRI_sim_collection_t* collection,
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
                                     collection->base._shaper,
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

  idx = LookupPathIndexSimCollection(collection, &paths, TRI_IDX_TYPE_HASH_INDEX, unique);
  
  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("hash-index already created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // create the hash index
  idx = TRI_CreateHashIndex(&collection->base, &fields, &paths, unique);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);
  
  // if index id given, use it otherwise use the default.
  if (iid) {
    idx->_iid = iid;
  }
  
  // initialises the index with all existing documents
  res = FillIndex(collection, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);
    return NULL;
  }
  
  // store index and return
  TRI_PushBackVectorPointer(&collection->_indexes, idx);

  if (created != NULL) {
    *created = true;
  }

  return idx;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int HashIndexFromJson (TRI_sim_collection_t* sim,
                              TRI_json_t* definition,
                              TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(sim, definition, iid, CreateHashIndexSimCollection);
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

TRI_index_t* TRI_LookupHashIndexSimCollection (TRI_sim_collection_t* sim,
                                               TRI_vector_pointer_t const* attributes,
                                               bool unique) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  // determine the sorted shape ids for the attributes
  res = TRI_PidNamesByAttributeNames(attributes, 
                                     sim->base._shaper,
                                     &paths,
                                     &fields,
                                     true);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  idx = LookupPathIndexSimCollection(sim, &paths, TRI_IDX_TYPE_HASH_INDEX, unique);
  
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

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

TRI_index_t* TRI_EnsureHashIndexSimCollection (TRI_sim_collection_t* sim,
                                               TRI_vector_pointer_t const* attributes,
                                               bool unique,
                                               bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  // given the list of attributes (as strings) 
  idx = CreateHashIndexSimCollection(sim, attributes, 0, unique, created);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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

static TRI_index_t* CreateSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                      TRI_vector_pointer_t const* attributes,
                                                      TRI_idx_iid_t iid,
                                                      bool unique,
                                                      bool* created) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = TRI_PidNamesByAttributeNames(attributes, 
                                     collection->base._shaper,
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

  idx = LookupPathIndexSimCollection(collection, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique);
  
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
  idx = TRI_CreateSkiplistIndex(&collection->base, &fields, &paths, unique);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);
  
  // If index id given, use it otherwise use the default.
  if (iid) {
    idx->_iid = iid;
  }
  
  // initialises the index with all existing documents
  res = FillIndex(collection, idx);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);
    return NULL;
  }
  
  // store index and return
  TRI_PushBackVectorPointer(&collection->_indexes, idx);
  
  if (created != NULL) {
    *created = true;
  }
  
  return idx;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(sim, definition, iid, CreateSkiplistIndexSimCollection);
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

TRI_index_t* TRI_LookupSkiplistIndexSimCollection (TRI_sim_collection_t* sim,
                                                   TRI_vector_pointer_t const* attributes,
                                                   bool unique) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;
  
  // determine the unsorted shape ids for the attributes
  res = TRI_PidNamesByAttributeNames(attributes, 
                                     sim->base._shaper,
                                     &paths,
                                     &fields,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  idx = LookupPathIndexSimCollection(sim, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique);
  
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

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

TRI_index_t* TRI_EnsureSkiplistIndexSimCollection (TRI_sim_collection_t* sim,
                                                   TRI_vector_pointer_t const* attributes,
                                                   bool unique,
                                                   bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  idx = CreateSkiplistIndexSimCollection(sim, attributes, 0, unique, created);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);
  
    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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

static TRI_index_t* CreatePriorityQueueIndexSimCollection (TRI_sim_collection_t* collection,
                                                           TRI_vector_pointer_t const* attributes,
                                                           TRI_idx_iid_t iid,
                                                           bool unique,
                                                           bool* created) {
  TRI_index_t* idx     = NULL;
  TRI_shaper_t* shaper = collection->base._shaper;
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

  idx = TRI_LookupPriorityQueueIndexSimCollection(collection, &paths);
  
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

  idx = TRI_CreatePriorityQueueIndex(&collection->base, &fields, &paths, unique);

  // ...........................................................................
  // If index id given, use it otherwise use the default.
  // ...........................................................................

  if (iid) {
    idx->_iid = iid;
  }
  
  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................

  res = FillIndex(collection, idx);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreePriorityQueueIndex(idx);
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

  if (created != NULL) {
    *created = true;
  }
  
  return idx;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int PriorityQueueFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return PathBasedIndexFromJson(sim, definition, iid, CreatePriorityQueueIndexSimCollection);
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

TRI_index_t* TRI_LookupPriorityQueueIndexSimCollection (TRI_sim_collection_t* collection,
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

TRI_index_t* TRI_EnsurePriorityQueueIndexSimCollection(TRI_sim_collection_t* sim,
                                                       TRI_vector_pointer_t const* attributes,
                                                       bool unique,
                                                       bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  // Given the list of attributes (as strings) 
  idx = CreatePriorityQueueIndexSimCollection(sim, attributes, 0, unique, created);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);

    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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

static TRI_index_t* CreateBitarrayIndexSimCollection (TRI_sim_collection_t* collection,
                                                      const TRI_vector_pointer_t* attributes,
                                                      const TRI_vector_pointer_t* values,
                                                      TRI_idx_iid_t iid,
                                                      bool supportUndef,
                                                      bool* created) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = TRI_PidNamesByAttributeNames(attributes, 
                                     collection->base._shaper,
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

  idx = LookupPathIndexSimCollection(collection, &paths, TRI_IDX_TYPE_BITARRAY_INDEX, false);
  
  if (idx != NULL) {
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
  
  idx = TRI_CreateBitarrayIndex(&collection->base, &fields, &paths, (TRI_vector_pointer_t*)(values), supportUndef);

  
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
  
  res = FillIndex(collection, idx);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeBitarrayIndex(idx);
    return NULL;
  }
  

  // ...........................................................................
  // store index within the collection and return
  // ...........................................................................
  
  TRI_PushBackVectorPointer(&collection->_indexes, idx);
  
  if (created != NULL) {
    *created = true;
  }
  
  return idx;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int BitarrayIndexFromJson (TRI_sim_collection_t* sim,
                                  TRI_json_t* definition,
                                  TRI_idx_iid_t iid) {
  return BitarrayBasedIndexFromJson(sim, definition, iid, CreateBitarrayIndexSimCollection);
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

TRI_index_t* TRI_LookupBitarrayIndexSimCollection (TRI_sim_collection_t* sim,
                                                   const TRI_vector_pointer_t* attributes) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int result;
  
  // ...........................................................................
  // determine the unsorted shape ids for the attributes
  // ...........................................................................
  
  result = TRI_PidNamesByAttributeNames(attributes, sim->base._shaper, &paths,
                                     &fields, false);

  if (result != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  
  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  
  // .............................................................................
  // attempt to go through the indexes within the collection and see if we can
  // locate the index
  // .............................................................................
  
  idx = LookupPathIndexSimCollection(sim, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, false);
  
  
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

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

TRI_index_t* TRI_EnsureBitarrayIndexSimCollection (TRI_sim_collection_t* sim,
                                                   const TRI_vector_pointer_t* attributes,
                                                   const TRI_vector_pointer_t* values,
                                                   bool supportUndef,
                                                   bool* created) {
  TRI_index_t* idx;
  int res;

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
  
  idx = CreateBitarrayIndexSimCollection(sim, attributes, values, 0, supportUndef, created);
  
  if (idx == NULL) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);
    return NULL;
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (created) {
    res = TRI_SaveIndex(&sim->base, idx);
  
    return res == TRI_ERROR_NO_ERROR ? idx : NULL;
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
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
  TRI_shaped_json_t const* document;
  TRI_shaped_json_t* example;
  TRI_shaped_json_t result;
  TRI_shape_t const* shape;
  bool ok;
  size_t i;

  document = &doc->_document;

  for (i = 0;  i < len;  ++i) {
    example = values[i];

    ok = TRI_ExtractShapedJsonVocShaper(shaper,
                                        document,
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

TRI_vector_t TRI_SelectByExample (TRI_sim_collection_t* sim,
                                  size_t length,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values) {
  TRI_shaper_t* shaper;
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** end;
  TRI_vector_t filtered;

  // use filtered to hold copies of the master pointer
  TRI_InitVector(&filtered, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t));

  // do a full scan
  shaper = sim->base._shaper;

  ptr = (TRI_doc_mptr_t const**) (sim->_primaryIndex._table);
  end = (TRI_doc_mptr_t const**) (sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc);

  for (;  ptr < end;  ++ptr) {
    if (*ptr && (*ptr)->_deletion == 0) {
      if (IsExampleMatch(shaper, *ptr, length, pids, values)) {
        TRI_PushBackVector(&filtered, *ptr);
      }
    }
  }

  return filtered;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
