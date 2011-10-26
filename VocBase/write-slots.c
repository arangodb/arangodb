////////////////////////////////////////////////////////////////////////////////
/// @brief write slots
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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "write-slots.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialses a write slots
////////////////////////////////////////////////////////////////////////////////

void TRI_InitWriteSlots (TRI_col_write_slots_t* slots) {
  size_t i;
  TRI_col_write_slot_t* next;
  TRI_col_write_slot_t* last = NULL;

  for (i = 0;  i < TRI_COL_WRITE_SLOTS;  ++i) {
    next = TRI_Allocate(sizeof(TRI_col_write_slot_t));

    next->_begin = NULL;
    next->_end = NULL;
    next->_backward = NULL;
    next->_forward = last;

    last = next;
  }

  slots->_free = last;
  slots->_used = NULL;

  slots->_position = NULL;
  slots->_synced = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the writes slots, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyWriteSlots (TRI_col_write_slots_t* slots) {
  TRI_col_write_slot_t* next;
  TRI_col_write_slot_t* current;

  // clear free list
  current = slots->_free;

  while (current != NULL) {
    next = current->_forward;
    TRI_Free(current);
    current = next;
  }

  // clear used list
  current = slots->_used;

  while (current != 0) {
    next = current->_forward;
    TRI_Free(current);
    current = next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the writes slots
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeWriteSlots (TRI_col_write_slots_t* slots) {
  TRI_DestroyWriteSlots(slots);
  TRI_Free(slots);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a free write slot, returns 0 if none is free
////////////////////////////////////////////////////////////////////////////////

TRI_col_write_slot_t* TRI_AcquireWriteSlots (TRI_col_write_slots_t* slots, void* begin, void* end) {
  TRI_col_write_slot_t* slot;
  TRI_col_write_slot_t* used;

  if (slots->_free == NULL) {
    return NULL;
  }

  slot = slots->_free;
  slots->_free = slot->_forward;

  slot->_forward = NULL;
  slot->_begin = begin;
  slot->_end = end;

  if (slots->_used == NULL) {
    slot->_backward = NULL;

    slots->_used = slot;
  }
  else {
    used = slots->_used;

    while (used->_forward != NULL) {
      used = used->_forward;
    }

    used->_forward = slot;
    slot->_backward = used;
  }

  if (slots->_synced == NULL) {
    slots->_synced = begin;
  }

  return slot;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a write slot
////////////////////////////////////////////////////////////////////////////////

void* TRI_ReleaseWriteSlots (TRI_col_write_slots_t* slots, TRI_col_write_slot_t* slot) {
  void* position;

  // correct the predecessor
  if (slot->_backward == NULL) {
    slots->_used = slot->_forward;

    // correct the position
    if (slots->_used == NULL) {
      slots->_position = slot->_end;
    }
    else {
      slots->_position = slots->_used->_begin;
    }
  }
  else {
    slot->_backward->_forward = slot->_forward;
  }

  // correct the successor
  if (slot->_forward != NULL) {
    slot->_forward->_backward = slot->_backward;
  }

  // put back onto free list
  slot->_backward = NULL;
  slot->_begin = NULL;
  slot->_end = NULL;

  slot->_forward = slots->_free;
  slots->_free = slot;

  position = slots->_position;

  return position;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
