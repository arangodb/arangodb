////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup thread
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

#include "cleanup.h"

#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/barrier.h"
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

static int const CLEANUP_INTERVAL = 1 * 1000 * 1000;

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
  LOG_TRACE("cleaning shadows");
  
  // clean unused cursors
  TRI_CleanupShadowData(vocbase->_cursors, SHADOW_CURSOR_MAX_AGE, force);
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
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;
  
  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    size_t n;
    size_t i;
    TRI_col_type_e type;
    // keep initial _state value as vocbase->_state might change during compaction loop
    int state = vocbase->_state; 

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
      TRI_doc_collection_t* doc;

      collection = collections._buffer[i];

      TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

      doc = collection->_collection;

      if (doc == NULL) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      type = doc->base._type;

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      // now release the lock and maybe unload the collection or some datafiles
      if (type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
        CleanupSimCollection((TRI_sim_collection_t*) doc);
      }
    }

    if (vocbase->_state >= 1) {
      // server is still running, clean up unused shadows
      CleanupShadows(vocbase, false);

      TRI_LockCondition(&vocbase->_cleanupCondition);
      TRI_TimedWaitCondition(&vocbase->_cleanupCondition, CLEANUP_INTERVAL);
      TRI_UnlockCondition(&vocbase->_cleanupCondition);
    }

    if (state == 3) {
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
