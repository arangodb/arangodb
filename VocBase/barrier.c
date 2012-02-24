////////////////////////////////////////////////////////////////////////////////
/// @brief delete barriers for datafiles
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

#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           BARRIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_InitBarrierList (TRI_barrier_list_t* container, TRI_doc_collection_t* collection) {
  container->_collection = collection;

  TRI_InitSpin(&container->_lock);

  container->_begin = NULL;
  container->_end = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set container
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBarrierList (TRI_barrier_list_t* container) {
  TRI_barrier_t* ptr;
  TRI_barrier_t* next;

  ptr = container->_begin;

  while (ptr != NULL) {
    next = ptr->_next;
    ptr->_container = NULL;
    ptr = next;
  }

  TRI_DestroySpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new barrier element
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierElement (TRI_barrier_list_t* container) {
  TRI_barrier_t* element;

  element = TRI_Allocate(sizeof(TRI_barrier_t));

  element->_type = TRI_BARRIER_ELEMENT;
  element->_container = container;
  element->_datafile = NULL;
  element->datafileCallback = NULL;

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

  return element;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile deletion barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierDatafile (TRI_barrier_list_t* container,
                                          TRI_datafile_t* datafile,
                                          void (*callback) (struct TRI_datafile_s*, void*),
                                          void* data) {
  TRI_barrier_t* element;

  element = TRI_Allocate(sizeof(TRI_barrier_t));

  element->_type = TRI_BARRIER_DATAFILE;
  element->_container = container;
  element->_datafile = datafile;
  element->_datafileData = data;
  element->datafileCallback = callback;

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

  return element;
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
  TRI_Free(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
