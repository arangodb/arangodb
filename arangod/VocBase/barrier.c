////////////////////////////////////////////////////////////////////////////////
/// @brief deletion barriers for datafiles
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

#include "barrier.h"

#include "BasicsC/logging.h"
#include "VocBase/primary-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           BARRIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts the barrier element into the linked list of barrier elemnents
/// of the collection
////////////////////////////////////////////////////////////////////////////////
  
static void LinkBarrierElement (TRI_barrier_t* element, TRI_barrier_list_t* container) {
  element->_container = container;

  TRI_LockSpin(&container->_lock);

  // empty list
  if (container->_end == NULL) {
    element->_next = NULL;
    element->_prev = NULL;

    container->_begin = element;
    container->_end = element;
  }

  // add to the end
  else {
    element->_next = NULL;
    element->_prev = container->_end;

    container->_end->_next = element;
    container->_end = element;
  }
  
  TRI_UnlockSpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitBarrierList (TRI_barrier_list_t* container, TRI_primary_collection_t* collection) {
  container->_collection = collection;

  TRI_InitSpin(&container->_lock);

  container->_begin = NULL;
  container->_end = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBarrierList (TRI_barrier_list_t* container) {
  TRI_barrier_t* ptr;
  TRI_barrier_t* next;

  ptr = container->_begin;

  while (ptr != NULL) {
    ptr->_container = NULL;
    next = ptr->_next;

    if (ptr->_type == TRI_BARRIER_COLLECTION_UNLOAD_CALLBACK ||
        ptr->_type == TRI_BARRIER_COLLECTION_DROP_CALLBACK ||
        ptr->_type == TRI_BARRIER_DATAFILE_CALLBACK) {
      // free data still allocated in barrier elements
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ptr);
    }
    else if (ptr->_type == TRI_BARRIER_ELEMENT) {
      LOG_ERROR("logic error. shouldn't have barrier elements in barrierlist on unload");
    }

    ptr = next;
  }

  TRI_DestroySpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the barrier list contains an element of a certain type
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContainsBarrierList (TRI_barrier_list_t* container, TRI_barrier_type_e type) {
  TRI_barrier_t* ptr;

  TRI_LockSpin(&container->_lock);

  ptr = container->_begin;

  while (ptr != NULL) {
    if (ptr->_type == type) {
      TRI_UnlockSpin(&container->_lock);
      return true;
    }
    ptr = ptr->_next;
  }

  TRI_UnlockSpin(&container->_lock);

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new barrier element
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierElementZ (TRI_barrier_list_t* container,
                                          size_t line,
                                          char const* filename) {
  TRI_barrier_blocker_t* element;

  element = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_barrier_blocker_t), false);

  if (element == NULL) {
    return NULL;
  }

  element->base._type = TRI_BARRIER_ELEMENT;

  element->_line = line;
  element->_filename = filename;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile deletion barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierDatafile (TRI_barrier_list_t* container,
                                          TRI_datafile_t* datafile,
                                          void (*callback) (struct TRI_datafile_s*, void*),
                                          void* data) {
  TRI_barrier_datafile_cb_t* element;

  element = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_barrier_datafile_cb_t), false);

  if (!element) {
    return NULL;
  }

  element->base._type = TRI_BARRIER_DATAFILE_CALLBACK;

  element->_datafile = datafile;
  element->_data = data;

  element->callback = callback;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection unload barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierUnloadCollection (TRI_barrier_list_t* container,
                                                  struct TRI_collection_s* collection,
                                                  bool (*callback) (struct TRI_collection_s*, void*),
                                                  void* data) {
  TRI_barrier_collection_cb_t* element;
  

  element = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_barrier_collection_cb_t), false);

  if (element == NULL) {
    return NULL;
  }

  
  element->base._type = TRI_BARRIER_COLLECTION_UNLOAD_CALLBACK;

  element->_collection = collection;
  element->_data = data;

  element->callback = callback;

  LinkBarrierElement(&element->base, container);
  

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection drop barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierDropCollection (TRI_barrier_list_t* container,
                                                struct TRI_collection_s* collection,
                                                bool (*callback) (struct TRI_collection_s*, void*),
                                                void* data) {
  TRI_barrier_collection_cb_t* element;

  element = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_barrier_collection_cb_t), false);

  if (!element) {
    return NULL;
  }

  element->base._type = TRI_BARRIER_COLLECTION_DROP_CALLBACK;

  element->_collection = collection;
  element->_data = data;

  element->callback = callback;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes and frees a barrier element or datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBarrier (TRI_barrier_t* element) {
  TRI_barrier_list_t* container;

  container = element->_container;

  TRI_LockSpin(&container->_lock);

  // element is at the beginning of the chain
  if (element->_prev == NULL) {
    container->_begin = element->_next;
  }
  else {
    element->_prev->_next = element->_next;
  }

  // element is at the end of the chain
  if (element->_next == NULL) {
    container->_end = element->_prev;
  }
  else {
    element->_next->_prev = element->_prev;
  }

  TRI_UnlockSpin(&container->_lock);

  // free the element
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
