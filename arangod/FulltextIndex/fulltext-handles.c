////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, handles
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-handles.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief at what percentage of deleted documents should the handle list be
/// cleaned?
////////////////////////////////////////////////////////////////////////////////

#define CLEANUP_THRESHOLD  0.25

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free a handle slot
////////////////////////////////////////////////////////////////////////////////

static void FreeSlot (TRI_fulltext_handle_slot_t* slot) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot->_documents);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot->_deleted);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate a slot on demand
////////////////////////////////////////////////////////////////////////////////

static bool AllocateSlot (TRI_fulltext_handles_t* const handles,
                          const uint32_t slotNumber) {
  TRI_fulltext_handle_slot_t* slot;

  if (handles->_slots[slotNumber] != NULL) {
    return true;
  }

  slot = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_handle_slot_t), false);

  if (slot == NULL) {
    return false;
  }

  // allocate and clear
  slot->_documents = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_doc_t) * handles->_slotSize, true);

  if (slot->_documents == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot);
    return false;
  }

  // allocate and clear deleted flags
  slot->_deleted = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(uint8_t) * handles->_slotSize, true);

  if (slot->_deleted == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot->_documents);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, slot);
    return false;
  }

  // set initial statistics
  slot->_min        = UINT32_MAX; // yes, this is intentional
  slot->_max        = 0;
  slot->_numUsed    = 0;
  slot->_numDeleted = 0;

  if (slotNumber == 0) {
    // first slot is an exception
    slot->_numUsed = 1;
  }

  handles->_slots[slotNumber] = slot;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate or grow the slot list on demand
////////////////////////////////////////////////////////////////////////////////

static bool AllocateSlotList (TRI_fulltext_handles_t* const handles,
                              const uint32_t targetNumber) {
  TRI_fulltext_handle_slot_t** slots;
  uint32_t currentNumber;

  currentNumber = handles->_numSlots;

  if (targetNumber <= handles->_numSlots || targetNumber == 0) {
    // nothing to do
    return true;
  }

  slots = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_handle_slot_t*) * targetNumber, true);

  if (slots == NULL) {
    // out of memory
    return false;
  }

  if (currentNumber > 0) {
    // copy old slot pointers
    memcpy(slots, handles->_slots, sizeof(TRI_fulltext_handle_slot_t*) * currentNumber);
  }

  if (handles->_slots != NULL) {
    // free old list pointer
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, handles->_slots);
  }

  // new slot is empty
  slots[targetNumber - 1] = NULL;

  handles->_slots    = slots;
  handles->_numSlots = targetNumber;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a handles instance
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handles_t* TRI_CreateHandlesFulltextIndex (const uint32_t slotSize) {
  TRI_fulltext_handles_t* handles;

  handles = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_handles_t), false);

  if (handles == NULL) {
    return NULL;
  }

  handles->_numDeleted = 0;
  handles->_next       = 1;

  handles->_slotSize   = slotSize;
  handles->_numSlots   = 0;
  handles->_slots      = NULL;
  handles->_map        = NULL;

  return handles;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a handles instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHandlesFulltextIndex (TRI_fulltext_handles_t* handles) {

  if (handles->_slots != NULL) {
    uint32_t i;

    for (i = 0; i < handles->_numSlots; ++i) {
      if (handles->_slots[i] != NULL) {
        FreeSlot(handles->_slots[i]);
      }
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, handles->_slots);
  }

  if (handles->_map != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, handles->_map);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, handles);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get number of documents (including deleted)
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_NumHandlesHandleFulltextIndex (TRI_fulltext_handles_t* const handles) {
  return (handles->_next - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get number of deleted documents
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_NumDeletedHandleFulltextIndex (TRI_fulltext_handles_t* const handles) {
  return handles->_numDeleted;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get handle list deletion grade
////////////////////////////////////////////////////////////////////////////////

double TRI_DeletionGradeHandleFulltextIndex (TRI_fulltext_handles_t* const handles) {
  return ((double) handles->_numDeleted / (double) handles->_next);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the handle list should be compacted
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShouldCompactHandleFulltextIndex (TRI_fulltext_handles_t* const handles) {
  return (TRI_DeletionGradeHandleFulltextIndex(handles) > CLEANUP_THRESHOLD);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compact the handle list. this will create a new handle list
/// and leaves the old one untouched
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handles_t* TRI_CompactHandleFulltextIndex (TRI_fulltext_handles_t* const original) {
  TRI_fulltext_handles_t* clone;
  TRI_fulltext_handle_t* map;
  uint32_t originalHandle, targetHandle;
  uint32_t i;

  map = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_handle_t) * original->_next, false);
  if (map == NULL) {
    return NULL;
  }

  clone = TRI_CreateHandlesFulltextIndex(original->_slotSize);

  if (clone == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, map);
    return NULL;
  }

  originalHandle = 1;
  targetHandle   = 1;

  for (i = 0; i < original->_numSlots; ++i) {
    TRI_fulltext_handle_slot_t* originalSlot;
    uint32_t start;
    uint32_t j;

    if (i == 0) {
      start =1;
    }
    else {
      start = 0;
    }

    originalSlot = original->_slots[i];
    for (j = start; j < originalSlot->_numUsed; ++j) {
      if (originalSlot->_deleted[j] == 1) {
        // printf("- setting map at #%lu to 0\n", (unsigned long) j);
        map[originalHandle++] = 0;
      }
      else {
        // printf("- setting map at #%lu to %lu\n", (unsigned long) j, (unsigned long) targetHandle);
        map[originalHandle++] = targetHandle++;
        TRI_InsertHandleFulltextIndex(clone, originalSlot->_documents[j]);
      }
    }
  }

  clone->_map = map;

  return clone;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document and return a handle for it
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_handle_t TRI_InsertHandleFulltextIndex (TRI_fulltext_handles_t* const handles,
                                                     const TRI_fulltext_doc_t document) {
  TRI_fulltext_handle_t handle;
  TRI_fulltext_handle_slot_t* slot;
  uint32_t slotNumber;
  uint32_t slotPosition;

  handle = handles->_next;

  if (handle == UINT32_MAX - 1) {
    // out of handles
    return 0;
  }

  slotNumber   = handle / handles->_slotSize;
  slotPosition = handle % handles->_slotSize;

  if (! AllocateSlotList(handles, slotNumber + 1)) {
    // out of memory
    return 0;
  }

  if (! AllocateSlot(handles, slotNumber)) {
    // out of memory
    return 0;
  }

  slot = handles->_slots[slotNumber];

  // fill in document
  slot->_documents[slotPosition] = document;
  slot->_numUsed++;
  // no need to fill in deleted flag as it is initialised to false

  if (document > slot->_max) {
    slot->_max = document;
  }
  if (document < slot->_min) {
    slot->_min = document;
  }

  handles->_next++;

  return handle;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a document as deleted in the handle list
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteDocumentHandleFulltextIndex (TRI_fulltext_handles_t* const handles,
                                            const TRI_fulltext_doc_t document) {
  uint32_t i;

  if (document == 0) {
    return true;
  }

  for (i = 0; i < handles->_numSlots; ++i) {
    TRI_fulltext_handle_slot_t* slot;
    uint32_t lastPosition;
    uint32_t j;

    slot = handles->_slots[i];
    lastPosition = slot->_numUsed;

    if (slot->_min > document || slot->_max < document || lastPosition <= slot->_numDeleted) {
      continue;
    }

    // we're in a relevant slot. now check its documents
    for (j = 0; j < lastPosition; ++j) {
      if (slot->_documents[j] == document) {
        slot->_deleted[j]   = 1;
        slot->_documents[j] = 0;
        slot->_numDeleted++;
        handles->_numDeleted++;
        return true;
      }
    }
    // this wasn't the correct slot unfortunately. now try next
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the document id for a handle
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_doc_t TRI_GetDocumentFulltextIndex (const TRI_fulltext_handles_t* const handles,
                                                 const TRI_fulltext_handle_t handle) {
  TRI_fulltext_handle_slot_t* slot;
  uint32_t slotNumber;
  uint32_t slotPosition;

  slotNumber = handle / handles->_slotSize;
#if TRI_FULLTEXT_DEBUG
  if (slotNumber >= handles->_numSlots) {
    // not found
    return 0;
  }
#endif

  slot = handles->_slots[slotNumber];
  slotPosition = handle % handles->_slotSize;
  if (slot->_deleted[slotPosition]) {
    // document was deleted
    return 0;
  }

  return slot->_documents[slotPosition];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all handles
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void TRI_DumpHandleFulltextIndex (TRI_fulltext_handles_t* const handles) {
  uint32_t i;

  for (i = 0; i < handles->_numSlots; ++i) {
    TRI_fulltext_handle_slot_t* slot;
    uint32_t j;

    slot = handles->_slots[i];

    printf("- slot %lu (%lu used, %lu deleted)\n",
           (unsigned long) i,
           (unsigned long) slot->_numUsed,
           (unsigned long) slot->_numDeleted);

    // we're in a relevant slot. now check its documents
    for (j = 0; j < slot->_numUsed; ++j) {
      printf("  - #%lu  %d  %llu\n",
             (unsigned long) (i * handles->_slotSize + j),
             (int) slot->_deleted[j],
             (unsigned long long) slot->_documents[j]);
    }
    printf("\n");
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory usage for the handles
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryHandleFulltextIndex (const TRI_fulltext_handles_t* const handles) {
  size_t memory;
  size_t perSlot;
  uint32_t numSlots;

  numSlots = handles->_numSlots;

  perSlot = (sizeof(TRI_fulltext_doc_t) + sizeof(uint8_t)) * handles->_slotSize;

  // slots list
  memory =  sizeof(TRI_fulltext_handle_slot_t*) * numSlots;
  // slot memory
  memory += (sizeof(TRI_fulltext_handle_slot_t) + perSlot) * numSlots;

  return memory;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
