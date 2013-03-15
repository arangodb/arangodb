////////////////////////////////////////////////////////////////////////////////
/// @brief compactor
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

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "compactor.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compactify interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const COMPACTOR_INTERVAL = (1 * 1000 * 1000);

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
/// @brief selects a journal, possibly waits until a journal appears
///
/// Note that the function grabs a lock. We have to release this lock, in order
/// to allow the gc to start when waiting for a journal to appear.
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectCompactor (TRI_document_collection_t* document,
                                        TRI_voc_size_t size,
                                        TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  int res;
  size_t i;
  size_t n;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  while (true) {
    n = document->base.base._compactors._length;

    if (n == 0) {
      // we don't have a compactor so we can exit immediately
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
      return NULL;
    }

    for (i = 0;  i < n;  ++i) {
      // select datafile
      datafile = document->base.base._compactors._buffer[i];

      // try to reserve space
      res = TRI_ReserveElementDatafile(datafile, size, result);

      // in case of full datafile, try next
      if (res == TRI_ERROR_NO_ERROR) {
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
        return datafile;
      }
      else if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
        TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
        return NULL;
      }
    }

    TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write document to file
////////////////////////////////////////////////////////////////////////////////

static int CopyDocument (TRI_document_collection_t* collection,
                         TRI_df_marker_t const* marker,
                         TRI_df_marker_t** result,
                         TRI_voc_fid_t* fid) {
  TRI_datafile_t* journal;
  TRI_voc_size_t total;

  // find and select a journal
  total = marker->_size;
  journal = SelectCompactor(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  *fid = journal->_fid;

  // and write marker and blob
  return TRI_WriteElementDatafile(journal,
                                  *result,
                                  marker,
                                  marker->_size,
                                  NULL,
                                  0,
                                  NULL,
                                  0,
                                  false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to delete datafile
///
/// Note that a datafile pointer is never freed. The state of the datafile
/// will be switch to TRI_DF_STATE_CLOSED - but the datafile pointer is
/// still valid.
////////////////////////////////////////////////////////////////////////////////

static void RemoveDatafileCallback (TRI_datafile_t* datafile, void* data) {
  TRI_collection_t* collection;
#ifdef TRI_ENABLE_LOGGER
  char* old;
#endif
  char* filename;
  char* name;
  char* number;
  bool ok;

  collection = data;

  number   = TRI_StringUInt64(datafile->_fid);
  name     = TRI_Concatenate3String("deleted-", number, ".db");
  filename = TRI_Concatenate2File(collection->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

#ifdef TRI_ENABLE_LOGGER
  old = datafile->_filename;
#endif

  ok = TRI_RenameDatafile(datafile, filename);

  if (! ok) {
    LOG_ERROR("cannot rename obsolete datafile '%s' to '%s': %s",
              old,
              filename,
              TRI_last_error());
  }

  LOG_DEBUG("finished compactifing datafile '%s'", datafile->_filename);

  ok = TRI_CloseDatafile(datafile);

  if (! ok) {
    LOG_ERROR("cannot close obsolete datafile '%s': %s",
              datafile->_filename,
              TRI_last_error());
  }
  else {
    if (collection->_vocbase->_removeOnCompacted) {
      int res;

      LOG_DEBUG("wiping compacted datafile from disk");

      res = TRI_UnlinkFile(filename);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("cannot wipe obsolete datafile '%s': %s",
                  datafile->_filename,
                  TRI_last_error());
      }
    }
  }

  TRI_FreeDatafile(datafile);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator
////////////////////////////////////////////////////////////////////////////////

static bool Compactifier (TRI_df_marker_t const* marker, void* data, TRI_datafile_t* datafile, bool journal) {
  TRI_df_marker_t* result;
  TRI_doc_datafile_info_t* dfi;
  TRI_doc_mptr_t const* found;
  TRI_doc_mptr_t* found2;
  TRI_document_collection_t* doc;
  TRI_primary_collection_t* primary;
  TRI_voc_fid_t fid;
  int res;

  doc = data;
  primary = &doc->base;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    bool deleted;

    TRI_voc_size_t markerSize = 0;
    TRI_voc_size_t keyBodySize = 0;
    TRI_doc_document_key_marker_t const* d = (TRI_doc_document_key_marker_t const*) marker;

    if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
      markerSize = sizeof(TRI_doc_document_key_marker_t);
    }
    else {
      markerSize = sizeof(TRI_doc_edge_key_marker_t);
    }

    keyBodySize = d->_offsetJson - d->_offsetKey;

    // check if the document is still active
    TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, ((char*) d + d->_offsetKey));
    deleted = found == NULL || found->_validTo != 0;

    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    if (deleted) {
      LOG_TRACE("found a stale document: %s", ((char*) d + d->_offsetKey));
      return true;
    }

    // write to compactor files
    res = CopyDocument(doc, marker, &result, &fid);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }

    // check if the document is still active
    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex,((char*) d + d->_offsetKey));
    deleted = found == NULL || found->_validTo != 0;

    // update datafile
    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, fid);

    if (deleted) {
      dfi->_numberDead += 1;
      dfi->_sizeDead += marker->_size - markerSize - keyBodySize;

      LOG_DEBUG("found a stale document after copying: %s", ((char*) d + d->_offsetKey));
      TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

      return true;
    }

    found2 = CONST_CAST(found);
    found2->_fid = fid;

    // let marker point to the new position
    found2->_data = result;

    // let _key point to the new key position
    found2->_key = ((char*) result) + (((TRI_doc_document_key_marker_t*) result)->_offsetKey);

    // update datafile info
    dfi->_numberAlive += 1;
    dfi->_sizeAlive += marker->_size - markerSize - keyBodySize;

    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
  }

  // deletion
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    TRI_doc_deletion_key_marker_t const* d = (TRI_doc_deletion_key_marker_t const*) marker;

    // write to compactor files
    res = CopyDocument(doc, marker, &result, &fid);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }

    // update datafile info
    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex,((char*) d + d->_offsetKey));

    if (found != NULL) {
      found2 = CONST_CAST(found);
      found2->_fid = fid;
      found2->_data = result;

      // let _key point to the new key position
      found2->_key = ((char*) result) + (((TRI_doc_deletion_key_marker_t*) result)->_offsetKey);
    }

    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, fid);
    dfi->_numberDeletion += 1;

    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for journal to sync
////////////////////////////////////////////////////////////////////////////////

static void WaitCompactSync (TRI_document_collection_t* collection, TRI_datafile_t* datafile) {
  TRI_LockCondition(&collection->_journalsCondition);

  while (datafile->_synced < datafile->_written) {
    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compactify a datafile
////////////////////////////////////////////////////////////////////////////////

static void CompactifyDatafile (TRI_document_collection_t* document, TRI_voc_fid_t fid) {
  TRI_datafile_t* df;
  TRI_primary_collection_t* primary;
  bool ok;
  size_t n;
  size_t i;

  primary = &document->base;

  // locate the datafile
  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(primary);

  n = primary->base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = primary->base._datafiles._buffer[i];

    if (df->_fid == fid) {
      break;
    }
  }

  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

  if (i == n) {
    return;
  }

  // now compactify the datafile
  LOG_DEBUG("starting to compactify datafile '%s'", df->_filename);

  ok = TRI_IterateDatafile(df, Compactifier, document, false);

  if (! ok) {
    LOG_WARNING("failed to compactify the datafile '%s'", df->_filename);
    return;
  }

  // wait for the journals to sync
  WaitCompactSync(document, df);

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(primary);

  n = primary->base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = primary->base._datafiles._buffer[i];

    if (df->_fid == fid) {
      TRI_RemoveVectorPointer(&primary->base._datafiles, i);
      break;
    }
  }

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

  if (i == n) {
    LOG_WARNING("failed to locate the datafile '%lu'", (unsigned long) df->_fid);
    return;
  }

  // add a deletion marker to the result set container
  TRI_CreateBarrierDatafile(&primary->_barrierList, df, RemoveDatafileCallback, &primary->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static bool CompactifyDocumentCollection (TRI_document_collection_t* document) {
  TRI_primary_collection_t* primary;
  TRI_vector_t vector;
  size_t i, n;

  primary = &document->base;

  // if we cannot acquire the read lock instantly, we will exit directly.
  // otherwise we'll risk a multi-thread deadlock between synchroniser,
  // compactor and data-modification threads (e.g. POST /_api/document)
  if (! TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(primary)) {
    return false;
  }

  n = primary->base._datafiles._length;
  if (n == 0) {
    // nothing to compact
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
    return false;
  }

  // copy datafile information
  TRI_InitVector(&vector, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t));

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df;
    TRI_doc_datafile_info_t* dfi;

    df = primary->base._datafiles._buffer[i];
    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, df->_fid);

    if (dfi->_numberDead > 0) {
      // only use those datafiles that contain dead objects
      TRI_PushBackVector(&vector, dfi);

      // we stop at the first datafile.
      // this is better than going over all datafiles in a collection in one go
      // because the compactor is single-threaded, and collecting all datafiles
      // might take a long time (it might even be that there is a request to
      // delete the collection in the middle of compaction, but the compactor
      // will not pick this up as it is read-locking the collection status)
      break;
    }
  }

  // can now continue without the lock
  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);


  if (vector._length == 0) {
    // cleanup local variables
    TRI_DestroyVector(&vector);
    return false;
  }

  // handle datafiles with dead objects
  n = vector._length;
  for (i = 0;  i < n;  ++i) {
    TRI_doc_datafile_info_t* dfi;

    dfi = TRI_AtVector(&vector, i);

    assert(dfi->_numberDead > 0);

    LOG_DEBUG("datafile = %lu, alive = %lu / %lu, dead = %lu / %lu, deletions = %lu",
              (unsigned long) dfi->_fid,
              (unsigned long) dfi->_numberAlive,
              (unsigned long) dfi->_sizeAlive,
              (unsigned long) dfi->_numberDead,
              (unsigned long) dfi->_sizeDead,
              (unsigned long) dfi->_numberDeletion);

    CompactifyDatafile(document, dfi->_fid);
  }

  // cleanup local variables
  TRI_DestroyVector(&vector);

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
/// @brief compactor event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactorVocBase (void* data) {
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;

  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    size_t i, n;
    TRI_col_type_e type;
    // keep initial _state value as vocbase->_state might change during compaction loop
    int state = vocbase->_state;
    bool worked;

    // copy all collections
    TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);

    TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* collection;
      TRI_primary_collection_t* primary;

      collection = collections._buffer[i];

      if (! TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(collection)) {
        // if we can't acquire the read lock instantly, we continue directly
        // we don't want to stall here for too long
        continue;
      }

      primary = collection->_collection;

      if (primary == NULL) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      worked = false;
      type = primary->base._info._type;

      // for document collection, compactify datafiles
      if (TRI_IS_DOCUMENT_COLLECTION(type)) {
        if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
          TRI_barrier_t* ce = TRI_CreateBarrierCompaction(&primary->_barrierList);

          if (ce == NULL) {
            // out of memory
            LOG_WARNING("out of memory when trying to create a barrier element");
          }
          else {
            worked = CompactifyDocumentCollection((TRI_document_collection_t*) primary);

            TRI_FreeBarrier(ce);
          }
        }
      }

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      if (worked) {
        // signal the cleanup thread that we worked and that it can now wake up
        TRI_LockCondition(&vocbase->_cleanupCondition);
        TRI_SignalCondition(&vocbase->_cleanupCondition);
        TRI_UnlockCondition(&vocbase->_cleanupCondition);
      }
    }

    if (vocbase->_state == 1) {
      // only sleep while server is still running
      usleep(COMPACTOR_INTERVAL);
    }

    if (state == 2) {
      // server shutdown
      break;
    }
  }

  TRI_DestroyVectorPointer(&collections);

  LOG_TRACE("shutting down compactor thread");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
