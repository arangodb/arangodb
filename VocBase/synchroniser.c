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

#include <Basics/logging.h>

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

static bool CheckSyncSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  TRI_voc_size_t nSynced;
  TRI_voc_size_t nWritten;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  double ti;
  size_t i;
  size_t n;

  worked = false;
  base = &collection->base.base;

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  n = base->_journals._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_journals._buffer[i];

    TRI_LockCondition(&collection->_journalsCondition);

    synced = journal->_synced;
    nSynced = journal->_nSynced;

    written = journal->_written;
    nWritten = journal->_nWritten;

    TRI_UnlockCondition(&collection->_journalsCondition);

    if (synced < written) {
      worked = true;
      ok = TRI_msync(journal->_fd, synced, written);
      ti = TRI_microtime();

      TRI_LockCondition(&collection->_journalsCondition);

      if (ok) {
        journal->_synced = written;
        journal->_nSynced = nWritten;
        journal->_lastSynced = ti;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BroadcastCondition(&collection->_journalsCondition);
      TRI_UnlockCondition(&collection->_journalsCondition);

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

static bool CheckJournalSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool worked;
  size_t i;
  size_t n;

  worked = false;
  base = &collection->base.base;

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

      TRI_LockCondition(&collection->_journalsCondition);
      TRI_CloseJournalDocCollection(&collection->base, i);
      TRI_UnlockCondition(&collection->_journalsCondition);

      n = base->_journals._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (base->_journals._length == 0) {
    worked = true;

    TRI_LockCondition(&collection->_journalsCondition);

    journal = TRI_CreateJournalDocCollection(&collection->base);

    if (journal != NULL) {
      LOG_DEBUG("created new journal '%s'", journal->_filename);
    }

    TRI_BroadcastCondition(&collection->_journalsCondition);
    TRI_UnlockCondition(&collection->_journalsCondition);
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a compactor file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static bool CheckSyncCompactorSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  TRI_voc_size_t nSynced;
  TRI_voc_size_t nWritten;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  double ti;
  size_t i;
  size_t n;

  worked = false;
  base = &collection->base.base;

  // .............................................................................
  // the only thread MODIFYING the _compactors variable is this thread,
  // therefore no locking is required to access the _compactors
  // .............................................................................

  n = base->_compactors._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_compactors._buffer[i];

    TRI_LockCondition(&collection->_journalsCondition);

    synced = journal->_synced;
    nSynced = journal->_nSynced;

    written = journal->_written;
    nWritten = journal->_nWritten;

    TRI_UnlockCondition(&collection->_journalsCondition);

    if (synced < written) {
      worked = true;
      ok = TRI_msync(journal->_fd, synced, written);
      ti = TRI_microtime();

      TRI_LockCondition(&collection->_journalsCondition);

      if (ok) {
        journal->_synced = written;
        journal->_nSynced = nWritten;
        journal->_lastSynced = ti;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BroadcastCondition(&collection->_journalsCondition);
      TRI_UnlockCondition(&collection->_journalsCondition);

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

static bool CheckCompactorSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* compactor;
  bool worked;
  size_t i;
  size_t n;

  worked = false;
  base = &collection->base.base;

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

      TRI_LockCondition(&collection->_journalsCondition);
      TRI_CloseCompactorDocCollection(&collection->base, i);
      TRI_UnlockCondition(&collection->_journalsCondition);

      n = base->_compactors._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (base->_compactors._length == 0) {
    worked = true;

    TRI_LockCondition(&collection->_journalsCondition);

    compactor = TRI_CreateCompactorDocCollection(&collection->base);

    if (compactor != NULL) {
      LOG_DEBUG("created new compactor '%s'", compactor->_filename);
    }

    TRI_BroadcastCondition(&collection->_journalsCondition);
    TRI_UnlockCondition(&collection->_journalsCondition);
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

  TRI_InitVectorPointer(&collections);

  while (vocbase->_active) {
    size_t n;
    size_t i;
    bool worked;

    worked = false;

    // copy all collections
    TRI_ReadLockReadWriteLock(&vocbase->_lock);
    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
    TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* col;
      TRI_doc_collection_t* collection;
      bool result;

      col = collections._buffer[i];

      if (! col->_loaded) {
        continue;
      }

      collection = col->_collection;

      // for simple document collection, first sync and then seal
      type = collection->base._type;

      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        result = CheckSyncSimCollection((TRI_sim_collection_t*) collection);
        worked |= result;

        result = CheckJournalSimCollection((TRI_sim_collection_t*) collection);
        worked |= result;

        result = CheckSyncCompactorSimCollection((TRI_sim_collection_t*) collection);
        worked |= result;

        result = CheckCompactorSimCollection((TRI_sim_collection_t*) collection);
        worked |= result;
      }
    }

    if (! worked) {
      usleep(1000);
    }
  }

  TRI_DestroyVectorPointer(&collections);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
