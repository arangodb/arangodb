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

#include <Basics/conversions.h>
#include <Basics/files.h>
#include <Basics/logging.h>
#include <Basics/strings.h>

#include <VocBase/simple-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compatify intervall in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const COMPACTOR_INTERVALL = 5 * 1000 * 1000;

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
/// Note that the function is garbs a lock. We have to release this
/// lock, in order to allow the gc to start when waiting for a journal
/// to appear.
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectCompactor (TRI_sim_collection_t* collection,
                                        TRI_voc_size_t size,
                                        TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  bool ok;
  size_t i;
  size_t n;

  TRI_LockCondition(&collection->_journalsCondition);

  while (true) {
    n = collection->base.base._compactors._length;

    for (i = 0;  i < n;  ++i) {

      // select datafile
      datafile = collection->base.base._compactors._buffer[i];

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
/// @brief write document to file
////////////////////////////////////////////////////////////////////////////////

static bool CopyDocument (TRI_sim_collection_t* collection,
                          TRI_df_marker_t const* marker,
                          TRI_df_marker_t** result,
                          TRI_voc_fid_t* fid) {
  TRI_datafile_t* journal;
  TRI_voc_size_t total;

  // find and select a journal
  total = marker->_size;
  journal = SelectCompactor(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
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
////////////////////////////////////////////////////////////////////////////////

static void RemoveDatafileCallback (TRI_datafile_t* datafile, void* data) {
  TRI_collection_t* collection;
  char* old;
  char* filename;
  char* name;
  char* number;
  bool ok;

  collection = data;

  number = TRI_StringUInt32(datafile->_fid);
  name = TRI_Concatenate3String("deleted-", number, ".db");
  filename = TRI_Concatenate2File(collection->_directory, name);
  TRI_FreeString(number);
  TRI_FreeString(name);

  old = datafile->_filename;
  ok = TRI_RenameDatafile(datafile, filename);

  if (! ok) {
    LOG_ERROR("cannot rename obsolete datafile '%s' to '%s': %s",
              old,
              filename,
              TRI_last_error());
  }

  LOG_INFO("finished compactify datafile '%s'", datafile->_filename);

  ok = TRI_CloseDatafile(datafile);

  if (! ok) {
    LOG_ERROR("cannot close obsolete datafile '%s': %s",
              datafile->_filename,
              TRI_last_error());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator
////////////////////////////////////////////////////////////////////////////////

static bool Compactifier (TRI_df_marker_t const* marker, void* data, TRI_datafile_t* datafile, bool journal) {
  TRI_df_marker_t* result;
  TRI_doc_datafile_info_t* dfi;
  TRI_doc_mptr_t const* found;
  TRI_sim_collection_t* collection;
  TRI_voc_fid_t fid;
  bool ok;
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } cnv;

  collection = data;

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
    TRI_ReadLockReadWriteLock(&collection->_lock);

    found = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    if (found == NULL || found->_deletion != 0) {
      TRI_ReadUnlockReadWriteLock(&collection->_lock);
      LOG_TRACE("found a stale document: %lu", d->_did);
      return true;
    }

    TRI_ReadUnlockReadWriteLock(&collection->_lock);

    // write to compactor files
    ok = CopyDocument(collection, marker, &result, &fid);

    if (! ok) {
      LOG_FATAL("cannot write compactor file: ", TRI_last_error());
      return false;
    }

    // check if the document is still active
    TRI_WriteLockReadWriteLock(&collection->_lock);

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, fid);
    found = TRI_LookupByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    if (found == NULL || found->_deletion != 0) {
      dfi->_numberDead += 1;
      dfi->_sizeDead += marker->_size - markerSize;

      LOG_DEBUG("found a stale document after copying: %lu", d->_did);
      TRI_WriteUnlockReadWriteLock(&collection->_lock);

      return true;
    }

    cnv.c = found;
    cnv.v->_fid = datafile->_fid;
    cnv.v->_data = result;
    cnv.v->_document._data.data = ((char*) cnv.v->_data) + markerSize;

    // update datafile info
    dfi->_numberAlive += 1;
    dfi->_sizeAlive += marker->_size - markerSize;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
  }

  // deletion
  else if (marker->_type == TRI_DOC_MARKER_DELETION) {
    TRI_doc_deletion_marker_t const* d;

    d = (TRI_doc_deletion_marker_t const*) marker;

    // TODO: remove TRI_doc_deletion_marker_t from file

    // write to compactor files
    ok = CopyDocument(collection, marker, &result, &fid);

    if (! ok) {
      LOG_FATAL("cannot write compactor file: ", TRI_last_error());
      return false;
    }

    // update datafile info
    TRI_WriteLockReadWriteLock(&collection->_lock);

    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, fid);

    dfi->_numberDeletion += 1;

    TRI_WriteUnlockReadWriteLock(&collection->_lock);
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

static void CompactifyDatafile (TRI_sim_collection_t* collection, TRI_voc_fid_t fid) {
  TRI_datafile_t* df;
  bool ok;
  size_t n;
  size_t i;

  // locate the datafile
  TRI_ReadLockReadWriteLock(&collection->_lock);

  n = collection->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = collection->base.base._datafiles._buffer[i];

    if (df->_fid == fid) {
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  if (i == n) {
    return;
  }

  // now compactify the datafile
  LOG_INFO("starting to compactify datafile '%s'", df->_filename);

  ok = TRI_IterateDatafile(df, Compactifier, collection, false);

  if (! ok) {
    LOG_WARNING("failed to compactify the datafile '%s'", df->_filename);
    return;
  }

  // wait for the jounrals to sync
  WaitCompactSync(collection, df);

  // remove the datafile from the list of datafiles
  TRI_ReadLockReadWriteLock(&collection->_lock);

  n = collection->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    df = collection->base.base._datafiles._buffer[i];

    if (df->_fid == fid) {
      TRI_RemoveVectorPointer(&collection->base.base._datafiles, i);
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  if (i == n) {
    LOG_WARNING("failed to locate the datafile '%lu'", (unsigned long) df->_fid);
    return;
  }

  // add a deletion marker to the result set container
  TRI_AddDatafileRSContainer(&collection->base._resultSets, df, RemoveDatafileCallback, &collection->base.base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CompactifySimCollection (TRI_sim_collection_t* collection) {
  TRI_vector_t vector;
  size_t n;
  size_t i;

  TRI_InitVector(&vector, sizeof(TRI_doc_datafile_info_t));

  // copy datafile information
  TRI_ReadLockReadWriteLock(&collection->_lock);

  n = collection->base.base._datafiles._length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df;
    TRI_doc_datafile_info_t* dfi;

    df = collection->base.base._datafiles._buffer[i];
    dfi = TRI_FindDatafileInfoDocCollection(&collection->base, df->_fid);

    TRI_PushBackVector(&vector, dfi);
  }

  TRI_ReadUnlockReadWriteLock(&collection->_lock);

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

    CompactifyDatafile(collection, dfi->_fid);
  }

  // cleanup local variables
  TRI_DestroyVector(&vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CleanupSimCollection (TRI_sim_collection_t* collection) {
  TRI_rs_container_t* container;
  TRI_rs_container_element_t* element;

  container = &collection->base._resultSets;
  element = NULL;

  // check and remove a datafile element at the beginning of the list
  TRI_LockSpin(&container->_lock);

  if (container->_begin != NULL && container->_begin->_type == TRI_RSCE_DATAFILE) {
    element = container->_begin;

    container->_begin = element->_next;

    if (element->_next == NULL) {
      container->_end = NULL;
    }
    else {
      element->_next->_prev = NULL;
    }
  }

  TRI_UnlockSpin(&container->_lock);

  if (element == NULL) {
    return;
  }

  // execute callback
  element->datafileCallback(element->_datafile, element->_datafileData);

  // try again
  CleanupSimCollection(collection);
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
  TRI_col_type_e type;

  TRI_InitVectorPointer(&collections);

  while (vocbase->_active) {
    size_t n;
    size_t i;

    // copy all collections
    TRI_ReadLockReadWriteLock(&vocbase->_lock);
    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
    TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* col;
      TRI_doc_collection_t* collection;

      col = collections._buffer[i];

      if (! col->_loaded) {
        continue;
      }

      collection = col->_collection;

      // for simple document collection, compatify datafiles
      type = collection->base._type;

      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        CompactifySimCollection((TRI_sim_collection_t*) collection);
        CleanupSimCollection((TRI_sim_collection_t*) collection);
      }
    }

    usleep(COMPACTOR_INTERVALL);
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
