////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "mmfiles-fulltext-list.h"

/// @brief we'll set this bit (the highest of a uint32_t) if the list is sorted
/// if the list is not sorted, this bit is cleared
/// This is done as a space optimisation. A big index will contain a lot of
/// document lists, and saving an extra boolean value will likely cost an extra
/// 4 or 8 bytes due to padding. Avoiding saving the sorted flag in an extra
/// member greatly reduces the index sizes
#define SORTED_BIT 2147483648UL

/// @brief growth factor for lists
#define GROWTH_FACTOR 1.2

#include "Logger/Logger.h"

/// @brief return whether the list is sorted
/// this will check the sorted bit at the start of the list
static inline bool IsSorted(TRI_fulltext_list_t const* list) {
  uint32_t* head = (uint32_t*)list;

  return ((*head & SORTED_BIT) != 0);
}

/// @brief return whether the list is sorted
static inline void SetIsSorted(TRI_fulltext_list_t* const list,
                               bool const value) {
  uint32_t* head = (uint32_t*)list;

  // yes, we could also do this without branching and with more bit twiddling
  // but this is not the critical path in the code
  if (value) {
    (*head) |= SORTED_BIT;
  } else {
    (*head) &= ~SORTED_BIT;
  }
}

/// @brief return the pointer to the start of the list entries
static inline TRI_fulltext_list_entry_t* GetStart(
    const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*)list;
  ++head;  // numAllocated
  ++head;  // numEntries

  return (TRI_fulltext_list_entry_t*)head;
}

/// @brief return the number of entries
static inline uint32_t GetNumEntries(const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*)list;
  return *(++head);
}

/// @brief set the number of entries
static inline void SetNumEntries(TRI_fulltext_list_t* const list,
                                 uint32_t value) {
  uint32_t* head = (uint32_t*)list;

  *(++head) = value;
}

/// @brief return the number of allocated entries
static inline uint32_t GetNumAllocated(TRI_fulltext_list_t const* list) {
  uint32_t* head = (uint32_t*)list;

  return (*head & ~SORTED_BIT);
}

static uint32_t FindListEntry(TRI_fulltext_list_t* list,
                              TRI_fulltext_list_entry_t* listEntries,
                              uint32_t numEntries, 
                              TRI_fulltext_list_entry_t entry) {
  if (numEntries >= 10 && IsSorted(list)) {
    // binary search
    uint32_t l = 0;
    uint32_t r = numEntries - 1;
  
    while (true) {
      // determine midpoint
      uint32_t m = l + ((r - l) / 2);
      TRI_fulltext_list_entry_t value = listEntries[m];
      if (value == entry) {
        return m;
      }

      if (value > entry) {
        if (m == 0) {
          // we must abort because the following subtraction would
          // make the uin32_t underflow to UINT32_MAX!
          break;
        }
        // this is safe
        r = m - 1;
      } else {
        l = m + 1;
      }

      if (r < l) {
        break;
      }
    }
  } else {
    // linear search
    for (uint32_t i = 0; i < numEntries; ++i) {
      if (listEntries[i] == entry) {
        return i;
      }
    }
  }

  return UINT32_MAX;
}

/// @brief initialize a new list
static void InitList(TRI_fulltext_list_t* list, uint32_t size) {
  uint32_t* head = (uint32_t*)list;

  *(head++) = size;
  *(head) = 0;
}

/// @brief get the memory usage for a list of the specified size
static inline size_t MemoryList(uint32_t size) {
  return sizeof(uint32_t) +                         // numAllocated
         sizeof(uint32_t) +                         // numEntries
         size * sizeof(TRI_fulltext_list_entry_t);  // entries
}

/// @brief increase an existing list
static TRI_fulltext_list_t* IncreaseList(TRI_fulltext_list_t* list,
                                         uint32_t size) {
  TRI_fulltext_list_t* copy =
      TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, list, MemoryList(size));

  if (copy != nullptr) {
    InitList(copy, size);
  }

  return copy;
}

void TRI_CloneListMMFilesFulltextIndex(TRI_fulltext_list_t const* source,
                                       std::set<TRI_voc_rid_t>& result) {
  if (source == nullptr) {
    return;
  }
  
  uint32_t numEntries = GetNumEntries(source);
  if (numEntries > 0) {
    TRI_fulltext_list_entry_t* entries = GetStart(source);
    for (uint32_t i = 0; i < numEntries; ++i) {
      result.emplace(entries[i]);
    }
  }
}

/// @brief clone a list by copying an existing one
TRI_fulltext_list_t* TRI_CloneListMMFilesFulltextIndex(
    TRI_fulltext_list_t const* source) {
  uint32_t numEntries;

  if (source == nullptr) {
    numEntries = 0;
  } else {
    numEntries = GetNumEntries(source);
  }

  TRI_fulltext_list_t* list = TRI_CreateListMMFilesFulltextIndex(numEntries);

  if (list != nullptr) {
    if (numEntries > 0) {
      memcpy(GetStart(list), GetStart(source),
             numEntries * sizeof(TRI_fulltext_list_entry_t));
      SetNumEntries(list, numEntries);
    }
  }

  return list;
}

/// @brief create a new list
TRI_fulltext_list_t* TRI_CreateListMMFilesFulltextIndex(uint32_t size) {
  TRI_fulltext_list_t* list =
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, MemoryList(size));

  if (list == nullptr) {
    // out of memory
    return nullptr;
  }

  InitList(list, size);

  return list;
}

/// @brief free a list
void TRI_FreeListMMFilesFulltextIndex(TRI_fulltext_list_t* list) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, list);
}

/// @brief get the memory usage of a list
size_t TRI_MemoryListMMFilesFulltextIndex(TRI_fulltext_list_t const* list) {
  uint32_t size = GetNumAllocated(list);
  return MemoryList(size);
}

/// @brief insert an element into a list
/// this might free the old list and allocate a new, bigger one
TRI_fulltext_list_t* TRI_InsertListMMFilesFulltextIndex(
    TRI_fulltext_list_t* list, TRI_fulltext_list_entry_t entry) {
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t numAllocated;
  uint32_t numEntries;
  bool unsort;

  numAllocated = GetNumAllocated(list);
  numEntries = GetNumEntries(list);
  listEntries = GetStart(list);
  unsort = false;

  if (numEntries > 0) {
    TRI_fulltext_list_entry_t lastEntry;

    // check whether the entry is already contained in the list
    lastEntry = listEntries[numEntries - 1];
    if (entry == lastEntry) {
      // entry is already contained. no need to insert the same value again
      return list;
    }

    if (entry < lastEntry) {
      // we're adding at the end. we must update the sorted property if
      // the list is not sorted anymore
      unsort = true;
    }
  }

  if (numEntries + 1 >= numAllocated) {
    // must allocate more memory
    TRI_fulltext_list_t* clone;
    uint32_t newSize;

    newSize = (uint32_t)(numEntries * GROWTH_FACTOR);

    if (newSize == numEntries) {
      // 0 * something might not be enough...
      newSize = numEntries + 1;
    }

    // increase the existing list
    clone = IncreaseList(list, newSize);
    if (clone == nullptr) {
      return nullptr;
    }

    // switch over
    if (list != clone) {
      list = clone;
      listEntries = GetStart(list);
    }
  }

  if (unsort) {
    SetIsSorted(list, false);
  }

  // insert at the end
  listEntries[numEntries] = entry;
  SetNumEntries(list, numEntries + 1);

  return list;
}

/// @brief remove an element from a list
/// this might free the old list and allocate a new, smaller one
TRI_fulltext_list_t* TRI_RemoveListMMFilesFulltextIndex(
    TRI_fulltext_list_t* list, TRI_fulltext_list_entry_t entry) {
  if (list == nullptr) {
    return nullptr;
  }

  uint32_t numEntries = GetNumEntries(list);

  if (numEntries == 0) {
    // definitely not contained...
    return list;
  }
  
  TRI_fulltext_list_entry_t* listEntries = GetStart(list);
  uint32_t i = FindListEntry(list, listEntries, numEntries, entry);
  
  if (i == UINT32_MAX) {
    // not found
    return list;
  }
   
  // found! 
  --numEntries; 

  if (numEntries == 0) {
    // free all memory
    TRI_FreeListMMFilesFulltextIndex(list);
    return nullptr;
  }

  while (i < numEntries) {
    listEntries[i] = listEntries[i + 1];
    ++i;
  }

  SetNumEntries(list, numEntries);
    
  uint32_t numAllocated = GetNumAllocated(list);

  if (numAllocated > 4 && numEntries < numAllocated / 2) {
    // list is only half full now
    TRI_fulltext_list_t* clone = TRI_CloneListMMFilesFulltextIndex(list);

    if (clone != nullptr) {
      TRI_FreeListMMFilesFulltextIndex(list);
      return clone;
    }
  }

  return list;
}

/// @brief return the number of entries
uint32_t TRI_NumEntriesListMMFilesFulltextIndex(TRI_fulltext_list_t const* list) {
  return GetNumEntries(list);
}

/// @brief return a pointer to the first list entry
TRI_fulltext_list_entry_t* TRI_StartListMMFilesFulltextIndex(
    TRI_fulltext_list_t const* list) {
  return GetStart(list);
}
