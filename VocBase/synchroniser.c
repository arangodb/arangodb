////////////////////////////////////////////////////////////////////////////////
/// @brief synchroniser
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

#include "synchroniser.h"

#include <BasicsC/logging.h>

#include <VocBase/simple-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static bool CheckSyncSimCollection (TRI_sim_collection_t* sim) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  TRI_voc_size_t nWritten;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  double ti;
  size_t i;
  size_t n;

  worked = false;
  base = &sim->base.base;

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  n = base->_journals._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_journals._buffer[i];

    TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    synced = journal->_synced;

    written = journal->_written;
    nWritten = journal->_nWritten;

    TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    if (synced < written) {
      worked = true;
      ok = TRI_msync(journal->_fd, synced, written);
      ti = TRI_microtime();

      TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      if (ok) {
        journal->_synced = written;
        journal->_nSynced = nWritten;
        journal->_lastSynced = ti;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BROADCAST_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
      TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      if (ok) {
        LOG_TRACE("msync succeeded %p, size %lu", synced, (unsigned long)(written - synced));
      }
      else {
        LOG_ERROR("msync failed with: %s", TRI_last_error());
      }
    }
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the journal of a document collection
////////////////////////////////////////////////////////////////////////////////

static bool CheckJournalSimCollection (TRI_sim_collection_t* sim) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool worked;
  size_t i;
  size_t n;

  worked = false;
  base = &sim->base.base;

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  n = base->_journals._length;

  for (i = 0;  i < n;) {
    journal = base->_journals._buffer[i];

    if (journal->_full) {
      worked = true;

      LOG_DEBUG("closing full journal '%s'", journal->_filename);

      TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
      TRI_CloseJournalDocCollection(&sim->base, i);
      TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      n = base->_journals._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (base->_journals._length == 0) {
    worked = true;

    TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    journal = TRI_CreateJournalDocCollection(&sim->base);

    if (journal != NULL) {
      LOG_DEBUG("created new journal '%s'", journal->_filename);
    }

    TRI_BROADCAST_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
    TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a compactor file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static bool CheckSyncCompactorSimCollection (TRI_sim_collection_t* sim) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  double ti;
  size_t i;
  size_t n;

  worked = false;
  base = &sim->base.base;

  // .............................................................................
  // the only thread MODIFYING the _compactors variable is this thread,
  // therefore no locking is required to access the _compactors
  // .............................................................................

  n = base->_compactors._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_compactors._buffer[i];

    TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    synced = journal->_synced;
    written = journal->_written;

    TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    if (synced < written) {
      worked = true;
      ok = TRI_msync(journal->_fd, synced, written);
      ti = TRI_microtime();

      TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      if (ok) {
        journal->_synced = written;
        journal->_lastSynced = ti;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BROADCAST_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
      TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      if (ok) {
        LOG_TRACE("msync succeeded %p, size %lu", synced, (unsigned long)(written - synced));
      }
      else {
        LOG_ERROR("msync failed with: %s", TRI_last_error());
      }
    }
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the compactor of a document collection
////////////////////////////////////////////////////////////////////////////////

static bool CheckCompactorSimCollection (TRI_sim_collection_t* sim) {
  TRI_collection_t* base;
  TRI_datafile_t* compactor;
  bool worked;
  size_t i;
  size_t n;

  worked = false;
  base = &sim->base.base;

  // .............................................................................
  // the only thread MODIFYING the _compactor variable is this thread,
  // therefore no locking is required to access the _compactors
  // .............................................................................

  n = base->_compactors._length;

  for (i = 0;  i < n;) {
    compactor = base->_compactors._buffer[i];

    if (compactor->_full) {
      worked = true;

      LOG_DEBUG("closing full compactor '%s'", compactor->_filename);

      TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
      TRI_CloseCompactorDocCollection(&sim->base, i);
      TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

      n = base->_compactors._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (base->_compactors._length == 0) {
    worked = true;

    TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);

    compactor = TRI_CreateCompactorDocCollection(&sim->base);

    if (compactor != NULL) {
      LOG_DEBUG("created new compactor '%s'", compactor->_filename);
    }

    TRI_BROADCAST_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
    TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(sim);
  }

  return worked;
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
/// @brief synchroniser event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_SynchroniserVocBase (void* data) {
  TRI_col_type_e type;
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;

  assert(vocbase->_active);

  TRI_InitVectorPointer(&collections);

  while (true) {
    size_t n;
    size_t i;
    bool worked;
    // keep initial _active value as vocbase->_active might change during compaction loop
    int active = vocbase->_active; 

    worked = false;

    // copy all collections
    TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);

    TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* collection;
      TRI_doc_collection_t* doc;
      bool result;

      collection = collections._buffer[i];

      TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

      if (collection->_status != TRI_VOC_COL_STATUS_LOADED) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      doc = collection->_collection;

      // for simple document collection, first sync and then seal
      type = doc->base._type;

      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        result = CheckSyncSimCollection((TRI_sim_collection_t*) doc);
        worked |= result;

        result = CheckJournalSimCollection((TRI_sim_collection_t*) doc);
        worked |= result;

        result = CheckSyncCompactorSimCollection((TRI_sim_collection_t*) doc);
        worked |= result;

        result = CheckCompactorSimCollection((TRI_sim_collection_t*) doc);
        worked |= result;
      }

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    }

    if (! worked && vocbase->_active == 1) {
      // only sleep while server is still running
      usleep(1000);
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
