////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup thread
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

#include "cleanup.h"

#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/barrier.h"
#include "VocBase/document-collection.h"
#include "VocBase/shadow-data.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clean interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_INTERVAL = (1 * 1000 * 1000);

////////////////////////////////////////////////////////////////////////////////
/// @brief how many cleanup iterations until shadows are cleaned
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_SHADOW_ITERATIONS = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief how many cleanup iterations until indexes are cleaned
////////////////////////////////////////////////////////////////////////////////

static int const CLEANUP_INDEX_ITERATIONS = 5;

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
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static void CleanupDocumentCollection (TRI_document_collection_t* document) {
  // loop until done
  while (true) {
    TRI_barrier_list_t* container;
    TRI_barrier_t* element;
    bool hasUnloaded = false;

    container = &document->base._barrierList;
    element = NULL;

    // check and remove all callback elements at the beginning of the list
    TRI_LockSpin(&container->_lock);

    // check the element on top of the barrier list
    // if it is a TRI_BARRIER_ELEMENT, it means that there is still a reference held
    // to document data in a datafile. We must then not unload or remove a file

    if (container->_begin == NULL ||
        container->_begin->_type == TRI_BARRIER_ELEMENT ||
        container->_begin->_type == TRI_BARRIER_COLLECTION_COMPACTION) {
      // did not find anything at the head of the barrier list or found an element marker
      // this means we must exit and cannot throw away datafiles and can unload collections
      TRI_UnlockSpin(&container->_lock);
      return;
    }

    // no TRI_BARRIER_ELEMENT at the head of the barrier list. This means that there is
    // some other action we can perform (i.e. unloading a datafile or a collection)

    // note that there is no need to check the entire list for a TRI_BARRIER_ELEMENT as
    // the list is filled up in chronological order. New barriers are always added to the
    // tail of the list, and if we have the following list
    // HEAD -> TRI_BARRIER_DATAFILE_CALLBACK -> TRI_BARRIER_ELEMENT
    // then it is still safe to execute the datafile callback operation, even if there
    // is a TRI_BARRIER_ELEMENT after it.
    // This is the case because the TRI_BARRIER_DATAFILE_CALLBACK is only put into the
    // barrier list after changing the pointers in all headers. After the pointers are
    // changed, it is safe to unload/remove an old datafile (that noone points to). And
    // any newer TRI_BARRIER_ELEMENTS will always reference data inside other datafiles.

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

    // yes, we can release the lock here
    TRI_UnlockSpin(&container->_lock);

    // someone else might now insert a new TRI_BARRIER_ELEMENT here, but it will
    // always refer to a different datafile than the one that we will now unload

    // execute callback, sone of the callbacks might delete or free our collection
    if (element->_type == TRI_BARRIER_DATAFILE_DROP_CALLBACK) {
      TRI_barrier_datafile_drop_cb_t* de;

      de = (TRI_barrier_datafile_drop_cb_t*) element;

      de->callback(de->_datafile, de->_data);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
      // next iteration
    }
    else if (element->_type == TRI_BARRIER_DATAFILE_RENAME_CALLBACK) {
      TRI_barrier_datafile_rename_cb_t* de;

      de = (TRI_barrier_datafile_rename_cb_t*) element;

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
      LOG_FATAL_AND_EXIT("unknown barrier type '%d'", (int) element->_type);
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
/// @brief cleanup event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupVocBase (void* data) {
  TRI_vocbase_t* vocbase;
  TRI_vector_pointer_t collections;
  uint64_t iterations = 0;

  vocbase = data;
  assert(vocbase);
  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    size_t n;
    size_t i;
    TRI_col_type_e type;
    // keep initial _state value as vocbase->_state might change during compaction loop
    int state = vocbase->_state;


    ++iterations;

    if (state == 2) {
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
      TRI_primary_collection_t* primary;

      collection = collections._buffer[i];

      TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

      primary = collection->_collection;

      if (primary == NULL) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      type = primary->base._info._type;

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      // we're the only ones that can unload the collection, so using
      // the collection pointer outside the lock is ok

      // maybe cleanup indexes, unload the collection or some datafiles
      if (TRI_IS_DOCUMENT_COLLECTION(type)) {
        TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

        // clean indexes?
        if (iterations % (uint64_t) CLEANUP_INDEX_ITERATIONS == 0) {
          document->cleanupIndexes(document);
        }

        CleanupDocumentCollection(document);
      }
    }

    if (vocbase->_state >= 1) {
      // server is still running, clean up unused shadows
      if (iterations % CLEANUP_SHADOW_ITERATIONS == 0) {
        CleanupShadows(vocbase, false);
      }

      TRI_LockCondition(&vocbase->_cleanupCondition);
      TRI_TimedWaitCondition(&vocbase->_cleanupCondition, (uint64_t) CLEANUP_INTERVAL);
      TRI_UnlockCondition(&vocbase->_cleanupCondition);
    }

    if (state == 3) {
      // server shutdown
      break;
    }

  }

  TRI_DestroyVectorPointer(&collections);

  LOG_TRACE("shutting down cleanup thread");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

