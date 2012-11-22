////////////////////////////////////////////////////////////////////////////////
/// @brief compactor
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

#include "compactor.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/simple-collection.h"
#include "VocBase/shadow-data.h"

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

static int const COMPACTOR_INTERVAL = 1 * 1000 * 1000;

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

static TRI_datafile_t* SelectCompactor (TRI_sim_collection_t* sim,
                                        TRI_voc_size_t size,
                                        TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  int res;
  size_t i;
  size_t n;

  TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

  while (true) {
    n = sim->base.base._compactors._length;

    for (i = 0;  i < n;  ++i) {

      // select datafile
      datafile = sim->base.base._compactors._buffer[i];

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

    TRI_WAIT_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write document to file
////////////////////////////////////////////////////////////////////////////////

static int CopyDocument (TRI_sim_collection_t* collection,
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
    return false;
  }

  *fid = journal->_fid;

  // and write marker and blob
  return TRI_WriteElementDatafile(journal,
                                  *result,
                                  marker, marker->_size,
                                  NULL, 0,
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
  char* old;
  char* filename;
  char* name;
  char* number;
  bool ok;
  int res;

  collection = data;

  number = TRI_StringUInt32(datafile->_fid);
  name = TRI_Concatenate3String("deleted-", number, ".db");
  filename = TRI_Concatenate2File(collection->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

  old = datafile->_filename;
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
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } cnv;
  TRI_df_marker_t* result;
  TRI_doc_datafile_info_t* dfi;
  TRI_doc_mptr_t const* found;
  TRI_sim_collection_t* sim;
  TRI_voc_fid_t fid;
  bool deleted;
  int res;

  sim = data;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_DOCUMENT || marker->_type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_document_marker_t const* d;
    size_t markerSize;

    if (marker->_type == TRI_DOC_MARKER_DOCUMENT) {
      markerSize = sizeof(TRI_doc_document_marker_t);
    }
    else if (marker->_type == TRI_DOC_MARKER_EDGE) {
      markerSize = sizeof(TRI_doc_edge_marker_t);
    }
    else {
      LOG_FATAL("unknown marker type %d", (int) marker->_type);
      exit(EXIT_FAILURE);
    }

    d = (TRI_doc_document_marker_t const*) marker;

    // check if the document is still active
    TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

    found = TRI_LookupByKeyAssociativePointer(&sim->_primaryIndex, &d->_did);
    deleted = found == NULL || found->_deletion != 0;

    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

    if (deleted) {
      LOG_TRACE("found a stale document: %llu", d->_did);
      return true;
    }

    // write to compactor files
    res = CopyDocument(sim, marker, &result, &fid);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL("cannot write compactor file: %s", TRI_last_error());
      return false;
    }

    // check if the document is still active
    TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

    found = TRI_LookupByKeyAssociativePointer(&sim->_primaryIndex, &d->_did);
    deleted = found == NULL || found->_deletion != 0;

    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(sim);

    // update datafile
    TRI_WRITE_LOCK_DATAFILES_SIM_COLLECTION(sim);

    dfi = TRI_FindDatafileInfoDocCollection(&sim->base, fid);

    if (deleted) {
      dfi->_numberDead += 1;
      dfi->_sizeDead += marker->_size - markerSize;

      LOG_DEBUG("found a stale document after copying: %llu", d->_did);
      TRI_WRITE_UNLOCK_DATAFILES_SIM_COLLECTION(sim);

      return true;
    }

    cnv.c = found;
    cnv.v->_fid = datafile->_fid;
    cnv.v->_data = result;
    cnv.v->_document._data.data = ((char*) cnv.v->_data) + markerSize;

    // update datafile info
    dfi->_numberAlive += 1;
    dfi->_sizeAlive += marker->_size - markerSize;

    TRI_WRITE_UNLOCK_DATAFILES_SIM_COLLECTION(sim);
  }

  // deletion
  else if (marker->_type == TRI_DOC_MARKER_DELETION) {
    // TODO: remove TRI_doc_deletion_marker_t from file

    // write to compactor files
    res = CopyDocument(sim, marker, &result, &fid);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL("cannot write compactor file: %s", TRI_last_error());
      return false;
    }

    // update datafile info
    TRI_WRITE_LOCK_DATAFILES_SIM_COLLECTION(sim);

    dfi = TRI_FindDatafileInfoDocCollection(&sim->base, fid);
    dfi->_numberDeletion += 1;

    TRI_WRITE_UNLOCK_DATAFILES_SIM_COLLECTION(sim);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for journal to sync
////////////////////////////////////////////////////////////////////////////////

static void WaitCompactSync (TRI_sim_collection_t* collection, TRI_datafile_t* datafile) {
  TRI_LockCondition(&collection->_journalsCondition);

  while (datafile->_synced < datafile->_written) {
    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compactify a datafile
////////////////////////////////////////////////////////////////////////////////

static void CompactifyDatafile (TRI_sim_collection_t* sim, TRI_voc_fid_t fid) {
  TRI_datafile_t* df;
  bool ok;
  size_t n;
  size_t i;

  // locate the datafile
  TRI_READ_LOCK_DATAFILES_SIM_COLLECTION(sim);

  n = sim->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = sim->base.base._datafiles._buffer[i];

    if (df->_fid == fid) {
      break;
    }
  }

  TRI_READ_UNLOCK_DATAFILES_SIM_COLLECTION(sim);

  if (i == n) {
    return;
  }

  // now compactify the datafile
  LOG_DEBUG("starting to compactify datafile '%s'", df->_filename);

  ok = TRI_IterateDatafile(df, Compactifier, sim, false);

  if (! ok) {
    LOG_WARNING("failed to compactify the datafile '%s'", df->_filename);
    return;
  }

  // wait for the journals to sync
  WaitCompactSync(sim, df);

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_SIM_COLLECTION(sim);

  n = sim->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = sim->base.base._datafiles._buffer[i];

    if (df->_fid == fid) {
      TRI_RemoveVectorPointer(&sim->base.base._datafiles, i);
      break;
    }
  }

  TRI_WRITE_UNLOCK_DATAFILES_SIM_COLLECTION(sim);

  if (i == n) {
    LOG_WARNING("failed to locate the datafile '%lu'", (unsigned long) df->_fid);
    return;
  }

  // add a deletion marker to the result set container
  TRI_CreateBarrierDatafile(&sim->base._barrierList, df, RemoveDatafileCallback, &sim->base.base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CompactifySimCollection (TRI_sim_collection_t* sim) {
  TRI_vector_t vector;
  size_t n;
  size_t i;

  if (! TRI_TRY_READ_LOCK_DATAFILES_SIM_COLLECTION(sim)) {
    return;
  }
  
  TRI_InitVector(&vector, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t));

  // copy datafile information
  n = sim->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df;
    TRI_doc_datafile_info_t* dfi;

    df = sim->base.base._datafiles._buffer[i];
    dfi = TRI_FindDatafileInfoDocCollection(&sim->base, df->_fid);

    TRI_PushBackVector(&vector, dfi);
  }

  TRI_READ_UNLOCK_DATAFILES_SIM_COLLECTION(sim);

  // handle datafiles with dead objects
  for (i = 0;  i < vector._length;  ++i) {
    TRI_doc_datafile_info_t* dfi;

    dfi = TRI_AtVector(&vector, i);

    if (dfi->_numberDead == 0) {
      continue;
    }

    LOG_DEBUG("datafile = %lu, alive = %lu / %lu, dead = %lu / %lu, deletions = %lu",
              (unsigned long) dfi->_fid,
              (unsigned long) dfi->_numberAlive,
              (unsigned long) dfi->_sizeAlive,
              (unsigned long) dfi->_numberDead,
              (unsigned long) dfi->_sizeDead,
              (unsigned long) dfi->_numberDeletion);

    CompactifyDatafile(sim, dfi->_fid);
  }

  // cleanup local variables
  TRI_DestroyVector(&vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CleanupSimCollection (TRI_sim_collection_t* sim) {
  // loop until done
  while (true) {
    TRI_barrier_list_t* container;
    TRI_barrier_t* element;
    bool hasUnloaded = false;

    container = &sim->base._barrierList;
    element = NULL;

    // check and remove a callback elements at the beginning of the list
    TRI_LockSpin(&container->_lock);

    if (container->_begin == NULL || container->_begin->_type == TRI_BARRIER_ELEMENT) {
      // did not find anything on top of the barrier list or found an element marker
      // this means we must exit
      TRI_UnlockSpin(&container->_lock);
      return;
    }

    element = container->_begin;
    assert(element);

    // found an element to go on with
    container->_begin = element->_next;

    if (element->_next == NULL) {
      container->_end = NULL;
    }
    else {
      element->_next->_prev = NULL;
    }

    TRI_UnlockSpin(&container->_lock);

    // execute callback, sone of the callbacks might delete or free our collection
    if (element->_type == TRI_BARRIER_DATAFILE_CALLBACK) {
      TRI_barrier_datafile_cb_t* de;

      de = (TRI_barrier_datafile_cb_t*) element;

      de->callback(de->_datafile, de->_data);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
      // next iteration
    }
    else if (element->_type == TRI_BARRIER_COLLECTION_UNLOAD_CALLBACK) {
      // collection is unloaded
      TRI_barrier_collection_cb_t* ce;

      ce = (TRI_barrier_collection_cb_t*) element;
      hasUnloaded = ce->callback(ce->_collection, ce->_data);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
      
      if (hasUnloaded) {
        // this has unloaded and freed the collection
        return;
      }
    }
    else if (element->_type == TRI_BARRIER_COLLECTION_DROP_CALLBACK) {
      // collection is dropped
      TRI_barrier_collection_cb_t* ce;

      ce = (TRI_barrier_collection_cb_t*) element;
      hasUnloaded = ce->callback(ce->_collection, ce->_data);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);

      if (hasUnloaded) {
        // this has dropped the collection
        return;
      }
    }
    else {
      // unknown type
      LOG_FATAL("unknown barrier type '%d'", (int) element->_type);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
    }

    // next iteration
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clean up shadows
////////////////////////////////////////////////////////////////////////////////

static void CleanupShadows (TRI_vocbase_t* const vocbase, bool force) {
  // clean unused cursors
  TRI_CleanupShadowData(vocbase->_cursors, (double) SHADOW_CURSOR_MAX_AGE, force);
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
  
  assert(vocbase->_active);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    size_t n;
    size_t i;
    TRI_col_type_e type;
    // keep initial _active value as vocbase->_active might change during compaction loop
    int active = vocbase->_active; 

    if (active == 2) {
      // shadows must be cleaned before collections are handled
      // otherwise the shadows might still hold barriers on collections 
      // and collections cannot be closed properly
      CleanupShadows(vocbase, true);
    }

    // copy all collections
    TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);

    TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* collection;
      TRI_doc_collection_t* doc;

      collection = collections._buffer[i];

      if (! TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(collection)) {
        continue;
      }

      doc = collection->_collection;

      if (doc == NULL) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      type = doc->base._type;

      // for simple document collection, compactify datafiles
      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
          CompactifySimCollection((TRI_sim_collection_t*) doc);
        }
      }

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      // now release the lock and maybe unload the collection or some datafiles
      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        CleanupSimCollection((TRI_sim_collection_t*) doc);
      }
    }

    if (vocbase->_active == 1) {
      // clean up unused shadows
      CleanupShadows(vocbase, false);

      // only sleep while server is still running
      usleep(COMPACTOR_INTERVAL);
    }

    if (active == 2) {
      // server shutdown
      break;
    }
  }

  TRI_DestroyVectorPointer(&collections);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
