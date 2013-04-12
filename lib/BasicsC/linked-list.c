////////////////////////////////////////////////////////////////////////////////
/// @brief linked list implementation
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "linked-list.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       LINKED LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hash key
////////////////////////////////////////////////////////////////////////////////

uint64_t HashKey (TRI_associative_pointer_t* array, void const* key) {
  return (uint64_t) (uintptr_t) key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hash element
////////////////////////////////////////////////////////////////////////////////

uint64_t HashElement (TRI_associative_pointer_t* array, void const* element) {
  TRI_linked_list_entry_t const* entry = element;

  return (uint64_t) (uintptr_t) entry->_data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief equal key
////////////////////////////////////////////////////////////////////////////////

bool EqualKey (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_linked_list_entry_t const* entry = element;

  return key == entry->_data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief equal element
////////////////////////////////////////////////////////////////////////////////

bool EqualElement (TRI_associative_pointer_t* array, void const* left, void const* right) {
  TRI_linked_list_entry_t const* l = left;
  TRI_linked_list_entry_t const* r = right;

  return l->_data == r->_data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an entry at the front or end of a linked array
////////////////////////////////////////////////////////////////////////////////

static int AddLinkedArray (TRI_linked_array_t* array, 
                           void const* data, 
                           const bool front) {
  TRI_linked_list_entry_t* entry;
  TRI_linked_list_entry_t* found;

  // create entry
  entry = TRI_Allocate(array->_memoryZone, sizeof(TRI_linked_list_entry_t), false);

  if (entry == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  entry->_data = data;

  // insert to lookup table
  found = TRI_InsertElementAssociativePointer(&array->_array, entry, true);

  if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY) {
    TRI_Free(array->_memoryZone, entry);
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  // this should not happen
  if (found != NULL) {
    TRI_RemoveLinkedList(&array->_list, found);
    TRI_Free(array->_memoryZone, found);
  }

  if (front) {
    // add element at the beginning
    TRI_AddFrontLinkedList(&array->_list, entry);
  }
  else {
    // add element at the end
    TRI_AddLinkedList(&array->_list, entry);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inits a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitLinkedList (TRI_linked_list_t* list, TRI_memory_zone_t* zone) {
  list->_memoryZone = zone;
  list->_begin = NULL;
  list->_end = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inits a linked array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitLinkedArray (TRI_linked_array_t* array, TRI_memory_zone_t* zone) {
  array->_memoryZone = zone;

  TRI_InitLinkedList(&array->_list, zone);
  TRI_InitAssociativePointer(&array->_array,
                             zone,
                             HashKey,
                             HashElement,
                             EqualKey,
                             EqualElement);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a linked list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyLinkedList (TRI_linked_list_t* list, TRI_memory_zone_t* zone) {
  TRI_linked_list_entry_t* current = list->_begin;

  // free all remaining entries in the list
  while (current != NULL) {
    TRI_linked_list_entry_t* next = current->_next;

    TRI_Free(zone, current);
    current = next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a linked list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeLinkedList (TRI_memory_zone_t* zone, TRI_linked_list_t* list) {
  TRI_DestroyLinkedList(list, zone);
  TRI_Free(zone, list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a linked array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyLinkedArray (TRI_linked_array_t* array) {
  TRI_DestroyLinkedList(&array->_list, array->_memoryZone);
  TRI_DestroyAssociativePointer(&array->_array);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a linked list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeLinkedArray (TRI_memory_zone_t* zone, TRI_linked_array_t* array) {
  TRI_DestroyLinkedList(&array->_list, zone);
  TRI_Free(zone, array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an entry at the end of a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_AddLinkedList (TRI_linked_list_t* list, TRI_linked_list_entry_t* entry) {
  if (list->_end == NULL) {
    entry->_next = NULL;
    entry->_prev = NULL;

    list->_begin = entry;
    list->_end = entry;
  }
  else {
    entry->_next = NULL;
    entry->_prev = list->_end;

    list->_end->_next = entry;
    list->_end = entry;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an entry at the front of a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFrontLinkedList (TRI_linked_list_t* list, TRI_linked_list_entry_t* entry) {
  TRI_linked_list_entry_t* begin = list->_begin;

  if (begin == NULL) {
    // list is empty
    TRI_AddLinkedList(list, entry);
  }
  else {
    // list is not empty
    begin->_prev = entry;
    entry->_prev = NULL;
    entry->_next = begin;

    list->_begin = entry;
    // end does not change
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from a linked list
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveLinkedList (TRI_linked_list_t* list, TRI_linked_list_entry_t* entry) {

  // element is at the beginning of the chain
  if (entry->_prev == NULL) {
    list->_begin = entry->_next;
  }
  else {
    entry->_prev->_next = entry->_next;
  }

  // element is at the end of the chain
  if (entry->_next == NULL) {
    list->_end = entry->_prev;
  }
  else {
    entry->_next->_prev = entry->_prev;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an entry at the end of a linked array
////////////////////////////////////////////////////////////////////////////////

int TRI_AddLinkedArray (TRI_linked_array_t* array, void const* data) {
  return AddLinkedArray(array, data, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an entry at the beginning of a linked array
////////////////////////////////////////////////////////////////////////////////

int TRI_AddFrontLinkedArray (TRI_linked_array_t* array, void const* data) {
  return AddLinkedArray(array, data, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from a linked array
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveLinkedArray (TRI_linked_array_t* array, void const* data) {
  TRI_linked_list_entry_t* found;

  found = TRI_RemoveKeyAssociativePointer(&array->_array, data);

  if (found != NULL) {
    TRI_RemoveLinkedList(&array->_list, found);
    TRI_Free(array->_memoryZone, found);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves an entry to the end of a linked array
////////////////////////////////////////////////////////////////////////////////

void TRI_MoveToBackLinkedArray (TRI_linked_array_t* array, void const* data) {
  TRI_linked_list_entry_t* found;

  found = TRI_LookupByKeyAssociativePointer(&array->_array, data);

  if (found) {
    if (found->_next != NULL) {
      TRI_RemoveLinkedList(&array->_list, found);
      TRI_AddLinkedList(&array->_list, found);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops front entry from list
////////////////////////////////////////////////////////////////////////////////

void const* TRI_PopFrontLinkedArray (TRI_linked_array_t* array) {
  TRI_linked_list_entry_t* found;
  void const* data;

  found = array->_list._begin;

  if (found == NULL) {
    return NULL;
  }

  data = found->_data;

  TRI_RemoveElementAssociativePointer(&array->_array, found);
  TRI_RemoveLinkedList(&array->_list, found);
  TRI_Free(array->_memoryZone, found);

  return data;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
