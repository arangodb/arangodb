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

#include "BasicsC/logging.h"

#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief synchroniser interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const SYNCHRONISER_INTERVAL = (100 * 1000);

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
/// @brief checks if a file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static bool CheckSyncDocumentCollection (TRI_document_collection_t* doc) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  size_t i;
  size_t n;

  worked = false;
  base = &doc->base.base;

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  n = base->_journals._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_journals._buffer[i];

    // we only need to care about physical datafiles
    if (! journal->isPhysical(journal)) {
      // anonymous regions do not need to be synced
      continue;
    }

    TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

    synced = journal->_synced;

    written = journal->_written;

    TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

    if (synced < written) {
      worked = true;
      ok = journal->sync(journal, synced, written);

      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

      if (ok) {
        journal->_synced = written;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

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
/// @brief checks the journal of a simple collection
////////////////////////////////////////////////////////////////////////////////

static bool CheckJournalDocumentCollection (TRI_document_collection_t* doc) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool worked;
  size_t i;
  size_t n;
      
  worked = false;
  base = &doc->base.base;

  if (base->_state != TRI_COL_STATE_WRITE) {
    return false;
  }

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

  n = base->_journals._length;

  for (i = 0;  i < n;) {
    journal = base->_journals._buffer[i];

    if (journal->_full) {
      worked = true;

      LOG_DEBUG("closing full journal '%s'", journal->_filename);

      TRI_CloseJournalPrimaryCollection(&doc->base, i);

      n = base->_journals._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  if (base->_journals._length == 0) {
    journal = TRI_CreateJournalDocumentCollection(doc);

    if (journal != NULL) {
      worked = true;
      LOG_DEBUG("created new journal '%s'", journal->_filename);

      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);
    }
    else {
      // an error occurred when creating the journal file
      LOG_ERROR("could not create journal file");
      
      // we still must wake up the other thread from time to time, otherwise we'll deadlock
      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);
    }
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a compactor file needs to be synced
////////////////////////////////////////////////////////////////////////////////

static bool CheckSyncCompactorDocumentCollection (TRI_document_collection_t* doc) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  size_t i;
  size_t n;
  
  worked = false;
  base = &doc->base.base;

  // .............................................................................
  // the only thread MODIFYING the _compactors variable is this thread,
  // therefore no locking is required to access the _compactors
  // .............................................................................

  n = base->_compactors._length;

  for (i = 0;  i < n; ++i) {
    journal = base->_compactors._buffer[i];

    // we only need to care about physical datafiles
    if (! journal->isPhysical(journal)) {
      // anonymous regions do not need to be synced
      continue;
    }

    TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

    synced = journal->_synced;
    written = journal->_written;

    TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

    if (synced < written) {
      worked = true;
      ok = journal->sync(journal, synced, written);

      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

      if (ok) {
        journal->_synced = written;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

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
/// @brief checks the compactor of a simple collection
////////////////////////////////////////////////////////////////////////////////

static bool CheckCompactorDocumentCollection (TRI_document_collection_t* doc) {
  TRI_collection_t* base;
  TRI_datafile_t* compactor;
  bool worked;
  size_t i;
  size_t n;
  
  worked = false;
  base = &doc->base.base;

  // .............................................................................
  // the only thread MODIFYING the _compactor variable is this thread,
  // therefore no locking is required to access the _compactors
  // .............................................................................
      
  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

  n = base->_compactors._length;

  for (i = 0;  i < n;) {
    compactor = base->_compactors._buffer[i];

    if (compactor->_full) {
      worked = true;

      LOG_DEBUG("closing full compactor '%s'", compactor->_filename);

      TRI_CloseCompactorPrimaryCollection(&doc->base, i);

      n = base->_compactors._length;
      i = 0;
    }
    else {
      ++i;
    }
  }


  if (base->_compactors._length == 0) {
    // we don't have a compactor file anymore
    compactor = TRI_CreateCompactorPrimaryCollection(&doc->base);

    if (compactor != NULL) {
      worked = true;
      LOG_DEBUG("created new compactor '%s'", compactor->_filename);

      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

    }
    else {
      // an error occurred when creating the compactor file
      LOG_ERROR("could not create compactor file");
      
      // we still must wake up the other thread from time to time, otherwise we'll deadlock
      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(doc);
    }
  }
  
  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(doc);

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

  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);


  while (true) {
    size_t n;
    size_t i;
    bool worked;

    // keep initial _state value as vocbase->_state might change during sync loop
    int state = vocbase->_state; 

    worked = false;

    // copy all collections and release the lock
    TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
    TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    // loop over all copied collections
    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* collection;
      TRI_primary_collection_t* primary;

      collection = collections._buffer[i];

      // if we cannot acquire the read lock instantly, we will continue.
      // otherwise we'll risk a multi-thread deadlock between synchroniser,
      // compactor and data-modification threads (e.g. POST /_api/document)
      if (! TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(collection)) {
        continue;
      }

      if (collection->_status != TRI_VOC_COL_STATUS_LOADED) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      primary = collection->_collection;

      // for simple collection, first sync and then seal
      type = primary->base._info._type;

      if (TRI_IS_DOCUMENT_COLLECTION(type)) {
        bool result;

        result = CheckSyncDocumentCollection((TRI_document_collection_t*) primary);
        worked |= result;

        result = CheckJournalDocumentCollection((TRI_document_collection_t*) primary);
        worked |= result;

        result = CheckSyncCompactorDocumentCollection((TRI_document_collection_t*) primary);
        worked |= result;

        result = CheckCompactorDocumentCollection((TRI_document_collection_t*) primary);
        worked |= result;
      }

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    }

    // only sleep while server is still running and no-one is waiting
    if (! worked && vocbase->_state == 1) {
      TRI_LOCK_SYNCHRONISER_WAITER_VOC_BASE(vocbase);

      if (vocbase->_syncWaiters == 0) {
        TRI_WAIT_SYNCHRONISER_WAITER_VOC_BASE(vocbase, (uint64_t) SYNCHRONISER_INTERVAL);
      }

      TRI_UNLOCK_SYNCHRONISER_WAITER_VOC_BASE(vocbase);
    }

    // server shutdown
    if (state == 2) {
      break;
    }
    
  }

  TRI_DestroyVectorPointer(&collections);
  
  LOG_TRACE("shutting down synchroniser thread");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
