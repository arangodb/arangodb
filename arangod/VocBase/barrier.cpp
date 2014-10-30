////////////////////////////////////////////////////////////////////////////////
/// @brief deletion barriers for datafiles
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "barrier.h"

#include "Basics/logging.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           BARRIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts the barrier element into the linked list of barrier elemnents
/// of the collection
////////////////////////////////////////////////////////////////////////////////

static void LinkBarrierElement (TRI_barrier_t* element,
                                TRI_barrier_list_t* container) {
  TRI_ASSERT(container != nullptr);
  TRI_ASSERT(element != nullptr);

  element->_container = container;

  TRI_LockSpin(&container->_lock);

  // empty list
  if (container->_end == nullptr) {
    element->_next = nullptr;
    element->_prev = nullptr;

    container->_begin = element;
    container->_end = element;
  }

  // add to the end
  else {
    element->_next = nullptr;
    element->_prev = container->_end;

    container->_end->_next = element;
    container->_end = element;
  }

  if (element->_type == TRI_BARRIER_ELEMENT) {
    // increase counter for barrier elements
    ++container->_numBarrierElements;
  }

  TRI_UnlockSpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for a barrier of a certain type
////////////////////////////////////////////////////////////////////////////////

template <typename T> static T* CreateBarrier () {
  return static_cast<T*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(T), false));
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitBarrierList (TRI_barrier_list_t* container,
                          TRI_document_collection_t* collection) {
  container->_collection = collection;

  TRI_InitSpin(&container->_lock);

  container->_begin = nullptr;
  container->_end   = nullptr;

  container->_numBarrierElements = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a barrier list
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBarrierList (TRI_barrier_list_t* container) {
  TRI_barrier_t* ptr;
  TRI_barrier_t* next;

  ptr = container->_begin;

  while (ptr != nullptr) {
    ptr->_container = nullptr;
    next = ptr->_next;

    if (ptr->_type == TRI_BARRIER_COLLECTION_UNLOAD_CALLBACK ||
        ptr->_type == TRI_BARRIER_COLLECTION_DROP_CALLBACK ||
        ptr->_type == TRI_BARRIER_DATAFILE_DROP_CALLBACK ||
        ptr->_type == TRI_BARRIER_DATAFILE_RENAME_CALLBACK ||
        ptr->_type == TRI_BARRIER_COLLECTION_REPLICATION ||
        ptr->_type == TRI_BARRIER_COLLECTION_COMPACTION) {

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ptr);
    }
    else if (ptr->_type == TRI_BARRIER_ELEMENT) {
      LOG_ERROR("logic error. shouldn't have barrier elements in barrierlist on unload");
    }
    else {
      LOG_ERROR("unknown barrier type");
    }

    ptr = next;
  }

  TRI_DestroySpin(&container->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the barrier list contains an element of a certain type
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContainsBarrierList (TRI_barrier_list_t* container,
                              TRI_barrier_type_e type) {
  TRI_LockSpin(&container->_lock);

  if (type == TRI_BARRIER_ELEMENT) {
    // shortcut
    bool hasBarriers = (container->_numBarrierElements > 0);
    TRI_UnlockSpin(&container->_lock);

    return hasBarriers;
  }

  TRI_barrier_t* ptr = container->_begin;

  while (ptr != nullptr) {
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
  TRI_barrier_blocker_t* element = CreateBarrier<TRI_barrier_blocker_t>();

  if (element == nullptr) {
    return nullptr;
  }

  element->base._type         = TRI_BARRIER_ELEMENT;
  element->_data              = nullptr;

  element->_line              = line;
  element->_filename          = filename;
  element->_usedByExternal    = false;
  element->_usedByTransaction = false;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new replication barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierReplication (TRI_barrier_list_t* container) {
  TRI_barrier_replication_t* element = CreateBarrier<TRI_barrier_replication_t>();

  if (element == nullptr) {
    return nullptr;
  }

  element->base._type = TRI_BARRIER_COLLECTION_REPLICATION;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compaction barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierCompaction (TRI_barrier_list_t* container) {
  TRI_barrier_compaction_t* element = CreateBarrier<TRI_barrier_compaction_t>();

  if (element == nullptr) {
    return nullptr;
  }

  element->base._type = TRI_BARRIER_COLLECTION_COMPACTION;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile deletion barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierDropDatafile (TRI_barrier_list_t* container,
                                              TRI_datafile_t* datafile,
                                              void (*callback) (struct TRI_datafile_s*, void*),
                                              void* data) {
  TRI_barrier_datafile_drop_cb_t* element = CreateBarrier<TRI_barrier_datafile_drop_cb_t>();

  if (element == nullptr) {
    return nullptr;
  }

  element->base._type = TRI_BARRIER_DATAFILE_DROP_CALLBACK;

  element->_datafile = datafile;
  element->_data = data;

  element->callback = callback;

  LinkBarrierElement(&element->base, container);

  return &element->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new datafile rename barrier
////////////////////////////////////////////////////////////////////////////////

TRI_barrier_t* TRI_CreateBarrierRenameDatafile (TRI_barrier_list_t* container,
                                                TRI_datafile_t* datafile,
                                                void (*callback) (struct TRI_datafile_s*, void*),
                                                void* data) {
  TRI_barrier_datafile_rename_cb_t* element = CreateBarrier<TRI_barrier_datafile_rename_cb_t>();

  if (element == nullptr) {
    return nullptr;
  }

  element->base._type = TRI_BARRIER_DATAFILE_RENAME_CALLBACK;

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
                                                  struct TRI_collection_t* collection,
                                                  bool (*callback) (struct TRI_collection_t*, void*),
                                                  void* data) {
  TRI_barrier_collection_cb_t* element = CreateBarrier<TRI_barrier_collection_cb_t>();

  if (element == nullptr) {
    return nullptr;
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
                                                struct TRI_collection_t* collection,
                                                bool (*callback) (struct TRI_collection_t*, void*),
                                                void* data) {
  TRI_barrier_collection_cb_t* element = CreateBarrier<TRI_barrier_collection_cb_t>();

  if (element == nullptr) {
    return nullptr;
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

  TRI_ASSERT(element != nullptr);
  container = element->_container;
  TRI_ASSERT(container != nullptr);

  TRI_LockSpin(&container->_lock);

  // element is at the beginning of the chain
  if (element->_prev == nullptr) {
    container->_begin = element->_next;
  }
  else {
    element->_prev->_next = element->_next;
  }

  // element is at the end of the chain
  if (element->_next == nullptr) {
    container->_end = element->_prev;
  }
  else {
    element->_next->_prev = element->_prev;
  }

  if (element->_type == TRI_BARRIER_ELEMENT) {
    // decrease counter for barrier elements
    --container->_numBarrierElements;
  }

  TRI_UnlockSpin(&container->_lock);

  // free the element
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes and frees a barrier element or datafile deletion marker,
/// this is used for barriers used by transaction or by external to protect
/// the flags by the lock
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBarrier (TRI_barrier_blocker_t* element, bool fromTransaction) {
  TRI_barrier_list_t* container;

  TRI_ASSERT(element != nullptr);
  container = element->base._container;
  TRI_ASSERT(container != nullptr);

  TRI_LockSpin(&container->_lock);

  // First see who might still be using the barrier:
  if (fromTransaction) {
    TRI_ASSERT(element->_usedByTransaction == true);
    element->_usedByTransaction = false;
  }
  else {
    TRI_ASSERT(element->_usedByExternal == true);
    element->_usedByExternal = false;
  }

  if (element->_usedByTransaction == false &&
      element->_usedByExternal == false) {
    // Really free it:

    // element is at the beginning of the chain
    if (element->base._prev == nullptr) {
      container->_begin = element->base._next;
    }
    else {
      element->base._prev->_next = element->base._next;
    }

    // element is at the end of the chain
    if (element->base._next == nullptr) {
      container->_end = element->base._prev;
    }
    else {
      element->base._next->_prev = element->base._prev;
    }

    if (element->base._type == TRI_BARRIER_ELEMENT) {
      // decrease counter for barrier elements
      --container->_numBarrierElements;
    }

    TRI_UnlockSpin(&container->_lock);

    // free the element
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
  }
  else {
    // Somebody else is still using it, so leave it intact:
    TRI_UnlockSpin(&container->_lock);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
