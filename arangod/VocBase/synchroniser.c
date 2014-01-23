////////////////////////////////////////////////////////////////////////////////
/// @brief synchroniser
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

#include "synchroniser.h"

#include "BasicsC/logging.h"

#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

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

static bool CheckSyncDocumentCollection (TRI_document_collection_t* document) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool ok;
  bool worked;
  char const* synced;
  char* written;
  size_t i;
  size_t n;

  worked = false;
  base = &document->base.base;

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

    TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    synced  = journal->_synced;
    written = journal->_written;
    TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

    if (synced < written) {
      worked = true;
      ok = journal->sync(journal, synced, written);

      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      if (ok) {
        journal->_synced = written;
      }
      else {
        journal->_state = TRI_DF_STATE_WRITE_ERROR;
      }

      TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(document);
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      if (ok) {
        LOG_TRACE("msync succeeded %p, size %lu", synced, (unsigned long) (written - synced));
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

static bool CheckJournalDocumentCollection (TRI_document_collection_t* document) {
  TRI_collection_t* base;
  TRI_datafile_t* journal;
  bool worked;
  size_t i;
  size_t n;

  worked = false;
  base = &document->base.base;

  if (base->_state != TRI_COL_STATE_WRITE) {
    return false;
  }

  // .............................................................................
  // the only thread MODIFYING the _journals variable is this thread,
  // therefore no locking is required to access the _journals
  // .............................................................................

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  n = base->_journals._length;

  for (i = 0;  i < n;) {
    journal = base->_journals._buffer[i];

    if (journal->_full) {
      worked = true;

      LOG_DEBUG("closing full journal '%s'", journal->getName(journal));

      TRI_CloseJournalPrimaryCollection(&document->base, i);

      n = base->_journals._length;
      i = 0;
    }
    else {
      ++i;
    }
  }

  // create a new journal if we do not have one, AND, if there was a request to create one
  // (sometimes we don't need a journal, e.g. directly after db._create(collection); when
  // the collection is still empty)
  if (base->_journals._length == 0 && 
      document->_requestedJournalSize > 0) {
    TRI_voc_size_t targetSize = document->base.base._info._maximalSize;

    if (document->_requestedJournalSize > 0 && 
        document->_requestedJournalSize > targetSize) {
      targetSize = document->_requestedJournalSize;
    }

    journal = TRI_CreateJournalDocumentCollection(document, targetSize);

    if (journal != NULL) {
      worked = true;
      document->_requestedJournalSize = 0;
      document->_rotateRequested      = false;

      LOG_DEBUG("created new journal '%s'", journal->getName(journal));
    }
    else {
      // an error occurred when creating the journal file
      LOG_ERROR("could not create journal file");
    }
  }
  else if (document->_rotateRequested) {
    // only a rotate was requested
    document->_rotateRequested = false;
  }
  
  // always broadcast, otherwise other threads waiting for the broadcast might deadlock!
  TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

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
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;

  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);


  while (true) {
    size_t i, n;
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
      bool result;

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

      // for document collection, first sync and then seal
      result = CheckSyncDocumentCollection((TRI_document_collection_t*) primary);
      worked |= result;

      result = CheckJournalDocumentCollection((TRI_document_collection_t*) primary);
      worked |= result;

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    }

    // only sleep while server is still running and no-one is waiting
    if (! worked && vocbase->_state == 1) {
      TRI_LOCK_SYNCHRONISER_WAITER_VOCBASE(vocbase);

      if (vocbase->_syncWaiters == 0) {
        TRI_WAIT_SYNCHRONISER_WAITER_VOCBASE(vocbase, (uint64_t) SYNCHRONISER_INTERVAL);
      }

      TRI_UNLOCK_SYNCHRONISER_WAITER_VOCBASE(vocbase);
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
