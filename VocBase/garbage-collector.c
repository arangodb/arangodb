////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase
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

#include "garbage-collector.h"

#include <Basics/logging.h>

#include <VocBase/simple-collection.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static void CheckSyncSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  TRI_voc_size_t nSynced;
  TRI_voc_size_t nWritten;
  char const* synced;
  char* written;
  size_t i;
  size_t n;
  bool ok;
  double ti;

  base = &collection->base.base;
  n = TRI_SizeVectorPointer(&base->_journals);

  for (i = 0;  i < n; ++i) {
    journal = TRI_AtVectorPointer(&base->_journals, i);

    TRI_LockCondition(&collection->_journalsCondition);

    synced = journal->_synced;
    nSynced = journal->_nSynced;

    written = journal->_written;
    nWritten = journal->_nWritten;

    TRI_UnlockCondition(&collection->_journalsCondition);

    if (synced < written) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the journal of a document collection
////////////////////////////////////////////////////////////////////////////////

static void CheckJournalSimCollection (TRI_sim_collection_t* collection) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  size_t i;
  size_t n;

  base = &collection->base.base;
  n = TRI_SizeVectorPointer(&base->_journals);

  for (i = 0;  i < n;) {
    journal = TRI_AtVectorPointer(&base->_journals, i);

    if (journal->_full) {
      LOG_DEBUG("closing full journal '%s'", journal->_filename);

      TRI_LockCondition(&collection->_journalsCondition);
      TRI_CloseJournalDocCollection(&collection->base, i);
      TRI_UnlockCondition(&collection->_journalsCondition);

      n = TRI_SizeVectorPointer(&base->_journals);
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (TRI_SizeVectorPointer(&base->_journals) == 0) {
    TRI_LockCondition(&collection->_journalsCondition);

    journal = TRI_CreateJournalDocCollection(&collection->base);

    if (journal != NULL) {
      LOG_DEBUG("created new journal '%s'", journal->_filename);
    }

    TRI_BroadcastCondition(&collection->_journalsCondition);
    TRI_UnlockCondition(&collection->_journalsCondition);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collector event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_GarbageCollector (void* data) {
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;

  TRI_InitVectorPointer(&collections);

  while (vocbase->_active) {
    size_t n;
    size_t i;

    // copy all collections
    TRI_ReadLockReadWriteLock(&vocbase->_lock);
    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
    TRI_ReadUnlockReadWriteLock(&vocbase->_lock);

    n = TRI_SizeVectorPointer(&collections);
    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* col;
      TRI_doc_collection_t* collection;

      col = TRI_AtVectorPointer(&collections, i);

      if (! col->_loaded) {
        continue;
      }

      collection = col->_collection;

      // for simple document collection, first sync and then seal
      if (collection->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        CheckSyncSimCollection((TRI_sim_collection_t*) collection);
        CheckJournalSimCollection((TRI_sim_collection_t*) collection);
      }
    }

    usleep(1000);
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
