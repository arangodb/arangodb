////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, list handling
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

#include "fulltext-list.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief we'll set this bit (the highest of a uint32_t) if the list is sorted
/// if the list is not sorted, this bit is cleared
/// This is done as a space optimisation. A big index will contain a lot of
/// document lists, and saving an extra boolean value will likely cost an extra
/// 4 or 8 bytes due to padding. Avoiding saving the sorted flag in an extra
/// member greatly reduces the index sizes
////////////////////////////////////////////////////////////////////////////////

#define SORTED_BIT 2147483648UL

////////////////////////////////////////////////////////////////////////////////
/// @brief growth factor for lists
////////////////////////////////////////////////////////////////////////////////

#define GROWTH_FACTOR 1.2

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
/// @brief compare two entries in a list
////////////////////////////////////////////////////////////////////////////////

static int CompareEntries (const void* lhs, const void* rhs) {
  TRI_fulltext_list_entry_t l = (*(TRI_fulltext_list_entry_t*) lhs);
  TRI_fulltext_list_entry_t r = (*(TRI_fulltext_list_entry_t*) rhs);

  if (l < r) {
    return -1;
  }

  if (l > r) {
    return 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the list is sorted
/// this will check the sorted bit at the start of the list
////////////////////////////////////////////////////////////////////////////////

static inline bool IsSorted (const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*) list;

  return ((*head & SORTED_BIT) != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the list is sorted
////////////////////////////////////////////////////////////////////////////////

static inline void SetIsSorted (TRI_fulltext_list_t* const list,
                                const bool value) {
  uint32_t* head = (uint32_t*) list;

  // yes, we could also do this without branching and with more bit twiddling
  // but this is not the critical path in the code
  if (value) {
    (*head) |= SORTED_BIT;
  }
  else {
    (*head) &= ~SORTED_BIT;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the pointer to the start of the list entries
////////////////////////////////////////////////////////////////////////////////

static inline TRI_fulltext_list_entry_t* GetStart (const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*) list;
  ++head; // numAllocated
  ++head; // numEntries

  return (TRI_fulltext_list_entry_t*) head;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of entries
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t GetNumEntries (const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*) list;
  return *(++head);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the number of entries
////////////////////////////////////////////////////////////////////////////////

static inline void SetNumEntries (TRI_fulltext_list_t* const list,
                                  const uint32_t value) {
  uint32_t* head = (uint32_t*) list;

  *(++head) = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of allocated entries
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t GetNumAllocated (const TRI_fulltext_list_t* const list) {
  uint32_t* head = (uint32_t*) list;

  return (*head & ~SORTED_BIT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the number of allocated entries
////////////////////////////////////////////////////////////////////////////////

static inline void SetNumAllocated (TRI_fulltext_list_t* const list,
                                    const uint32_t value) {
  uint32_t* head = (uint32_t*) list;

  if (IsSorted(list)) {
    *head = value | SORTED_BIT;
  }
  else {
    *head = value & ~SORTED_BIT;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a new list
////////////////////////////////////////////////////////////////////////////////

static void InitList (TRI_fulltext_list_t* const list, const uint32_t size) {
  uint32_t* head = (uint32_t*) list;

  *(head++) = size;
  *(head) = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a list in place
////////////////////////////////////////////////////////////////////////////////

static void SortList (TRI_fulltext_list_t* const list) {
  uint32_t numEntries;

  if (IsSorted(list)) {
    // nothing to do
    return;
  }

  numEntries = GetNumEntries(list);
  if (numEntries > 1) {
    // only sort if more than one elements
    qsort(GetStart(list), numEntries, sizeof(TRI_fulltext_list_entry_t), &CompareEntries);
  }

  SetIsSorted(list, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the memory usage for a list of the specified size
////////////////////////////////////////////////////////////////////////////////

static inline size_t MemoryList (const uint32_t size) {
  return sizeof(uint32_t) + // numAllocated
         sizeof(uint32_t) + // numEntries
         size * sizeof(TRI_fulltext_list_entry_t); // entries
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase an existing list
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_list_t* IncreaseList (TRI_fulltext_list_t* list,
                                          const uint32_t size) {
  TRI_fulltext_list_t* copy;

  copy = TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, list, MemoryList(size));
  if (copy == NULL) {
    // out of memory
    return NULL;
  }
  
  InitList(copy, size);

  return copy;
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
/// @brief clone a list by copying an existing one
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_CloneListFulltextIndex (const TRI_fulltext_list_t* const source) {
  TRI_fulltext_list_t* list;
  uint32_t numEntries;

  if (source == NULL) {
    numEntries = 0;
  }
  else {
    numEntries = GetNumEntries(source);
  }

  list = TRI_CreateListFulltextIndex(numEntries);
  if (list == NULL) {
    return NULL;
  }

  if (numEntries > 0) {
    memcpy(GetStart(list), GetStart(source), numEntries * sizeof(TRI_fulltext_list_entry_t));
    SetNumEntries(list, numEntries);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new list
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_CreateListFulltextIndex (const uint32_t size) {
  TRI_fulltext_list_t* list;

  list = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, MemoryList(size), false);
  if (list == NULL) {
    // out of memory
    return NULL;
  }

  InitList(list, size);

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a list
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeListFulltextIndex (TRI_fulltext_list_t* list) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, list);
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
/// @brief get the memory usage of a list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryListFulltextIndex (const TRI_fulltext_list_t* const list) {
  uint32_t size;

  size = GetNumAllocated(list);
  return MemoryList(size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unionise two lists (a.k.a. logical OR)
/// this will create a new list and free both lhs & rhs
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_UnioniseListFulltextIndex (TRI_fulltext_list_t* lhs,
                                                    TRI_fulltext_list_t* rhs) {
  TRI_fulltext_list_t* list;
  TRI_fulltext_list_entry_t last;
  TRI_fulltext_list_entry_t* lhsEntries;
  TRI_fulltext_list_entry_t* rhsEntries;
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t l, r;
  uint32_t numLhs, numRhs;
  uint32_t listPos;

  if (lhs == NULL) {
    return rhs;
  }
  if (rhs == NULL) {
    return lhs;
  }

  numLhs = GetNumEntries(lhs);
  numRhs = GetNumEntries(rhs);

  // check the easy cases when one of the lists is empty
  if (numLhs == 0) {
    TRI_FreeListFulltextIndex(lhs);
    return rhs;
  }

  if (numRhs == 0) {
    TRI_FreeListFulltextIndex(rhs);
    return lhs;
  }

  list = TRI_CreateListFulltextIndex(numLhs + numRhs);
  if (list == NULL) {
    TRI_FreeListFulltextIndex(lhs);
    TRI_FreeListFulltextIndex(rhs);
    return NULL;
  }

  SortList(lhs);
  lhsEntries = GetStart(lhs);
  l = 0;

  SortList(rhs);
  rhsEntries = GetStart(rhs);
  r = 0;

  listPos = 0;
  listEntries = GetStart(list);
  last = 0;

  while (true) {
    while (l < numLhs && lhsEntries[l] <= last) {
      ++l;
    }

    while (r < numRhs && rhsEntries[r] <= last) {
      ++r;
    }

    if (l >= numLhs && r >= numRhs) {
      break;
    }

    if (l >= numLhs && r < numRhs) {
      listEntries[listPos++] = last = rhsEntries[r++];
    }
    else if (l < numLhs && r >= numRhs) {
      listEntries[listPos++] = last = lhsEntries[l++];
    }
    else if (lhsEntries[l] < rhsEntries[r]) {
      listEntries[listPos++] = last = lhsEntries[l++];
    }
    else {
      listEntries[listPos++] = last = rhsEntries[r++];
    }
  }

  SetNumEntries(list, listPos);
  SetIsSorted(list, true);

  TRI_FreeListFulltextIndex(lhs);
  TRI_FreeListFulltextIndex(rhs);

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief intersect two lists (a.k.a. logical AND)
/// this will create a new list and free both lhs & rhs
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_IntersectListFulltextIndex (TRI_fulltext_list_t* lhs,
                                                     TRI_fulltext_list_t* rhs) {
  TRI_fulltext_list_t* list;
  TRI_fulltext_list_entry_t last;
  TRI_fulltext_list_entry_t* lhsEntries;
  TRI_fulltext_list_entry_t* rhsEntries;
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t l, r;
  uint32_t numLhs, numRhs;
  uint32_t listPos;

  // check if one of the pointers is NULL
  if (lhs == NULL) {
    return rhs;
  }

  if (rhs == NULL) {
    return lhs;
  }

  numLhs = GetNumEntries(lhs);
  numRhs = GetNumEntries(rhs);

  // printf("list intersection lhs: %lu rhs: %lu\n\n", (unsigned long) numLhs, (unsigned long) numRhs);

  // check the easy cases when one of the lists is empty
  if (numLhs == 0 || numRhs == 0) {
    if (lhs != NULL) {
      TRI_FreeListFulltextIndex(lhs);
    }
    if (rhs != NULL) {
      TRI_FreeListFulltextIndex(rhs);
    }

    return TRI_CreateListFulltextIndex(0);
  }


  // we have at least one entry in each list
  list = TRI_CreateListFulltextIndex(numLhs < numRhs ? numLhs : numRhs);
  if (list == NULL) {
    TRI_FreeListFulltextIndex(lhs);
    TRI_FreeListFulltextIndex(rhs);
    return NULL;
  }

  SortList(lhs);
  lhsEntries = GetStart(lhs);
  l = 0;

  SortList(rhs);
  rhsEntries = GetStart(rhs);
  r = 0;

  listPos = 0;
  listEntries = GetStart(list);
  last = 0;

  while (true) {
    while (l < numLhs && lhsEntries[l] <= last) {
      ++l;
    }

    while (r < numRhs && rhsEntries[r] <= last) {
      ++r;
    }

again:
    if (l >= numLhs || r >= numRhs) {
      break;
    }

    if (lhsEntries[l] < rhsEntries[r]) {
      ++l;
      goto again;
    }
    else if (lhsEntries[l] > rhsEntries[r]) {
      ++r;
      goto again;
    }

    // match
    listEntries[listPos++] = last = lhsEntries[l];
    ++l;
    ++r;
  }

  SetNumEntries(list, listPos);
  SetIsSorted(list, true);

  TRI_FreeListFulltextIndex(lhs);
  TRI_FreeListFulltextIndex(rhs);

  // printf("result list has %lu\n\n", (unsigned long) listPos);

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude values from a list
/// this will modify list in place
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_ExcludeListFulltextIndex (TRI_fulltext_list_t* list,
                                                   TRI_fulltext_list_t* exclude) {
  TRI_fulltext_list_entry_t* listEntries;
  TRI_fulltext_list_entry_t* excludeEntries;
  uint32_t numEntries;
  uint32_t numExclude;
  uint32_t i, j, listPos;

  if (list == NULL) {
    TRI_FreeListFulltextIndex(exclude);
    return list;
  }

  if (exclude == NULL) {
    return list;
  }

  numEntries = GetNumEntries(list);
  numExclude = GetNumEntries(exclude);

  if (numEntries == 0 || numExclude == 0) {
    // original list or exclusion list are empty
    TRI_FreeListFulltextIndex(exclude);
    return list;
  }

  SortList(list);

  listEntries    = GetStart(list);
  excludeEntries = GetStart(exclude);

  j = 0;
  listPos = 0;
  for (i = 0; i < numEntries; ++i) {
    TRI_fulltext_list_entry_t entry;

    entry = listEntries[i];
    while (j < numExclude && excludeEntries[j] < entry) {
      ++j;
    }

    if (j < numExclude && excludeEntries[j] == entry) {
      // entry is contained in exclusion list
      continue;
    }

    if (listPos != i) {
      listEntries[listPos] = listEntries[i];
    }
    ++listPos;
  }

  // we may have less results in the list of exclusion
  SetNumEntries(list, listPos);
  TRI_FreeListFulltextIndex(exclude);

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an element into a list
/// this might free the old list and allocate a new, bigger one
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_t* TRI_InsertListFulltextIndex (TRI_fulltext_list_t* list,
                                                  const TRI_fulltext_list_entry_t entry) {
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t numAllocated;
  uint32_t numEntries;
  bool unsort;

  numAllocated = GetNumAllocated(list);
  numEntries   = GetNumEntries(list);
  listEntries  = GetStart(list);
  unsort       = false;

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

    newSize = numEntries * GROWTH_FACTOR;
    
    if (newSize == numEntries) {
      // 0 * something might not be enough...
      newSize = numEntries + 1;
    }

    // increase the existing list
    clone = IncreaseList(list, newSize);
    if (clone == NULL) {
      return NULL;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief rewrites the list of entries using a map of handles
/// returns the number of entries remaining in the list after rewrite
/// the map is provided by the routines that handle the compaction
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_RewriteListFulltextIndex (TRI_fulltext_list_t* const list,
                                       const void* const data) {
  TRI_fulltext_list_entry_t* listEntries;
  TRI_fulltext_list_entry_t* map;
  uint32_t numEntries;
  uint32_t i, j;

  numEntries  = GetNumEntries(list);
  if (numEntries == 0) {
    return 0;
  }

  map = (TRI_fulltext_list_entry_t*) data;
  listEntries = GetStart(list);
  j = 0;

  for (i = 0; i < numEntries; ++i) {
    TRI_fulltext_list_entry_t entry;
    TRI_fulltext_list_entry_t mapped;

    entry = listEntries[i];
    if (entry == 0) {
      continue;
    }

    mapped = map[entry];
    if (mapped == 0) {
      // original value has been deleted
      continue;
    }

    listEntries[j++] = mapped;
  }

  if (j != numEntries) {
    SetNumEntries(list, j);
  }

  return j;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the contents of a list
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void TRI_DumpListFulltextIndex (const TRI_fulltext_list_t* const list) {
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t numEntries;
  uint32_t i;

  numEntries = GetNumEntries(list);
  listEntries = GetStart(list);

  printf("(");

  for (i = 0; i < numEntries; ++i) {
    TRI_fulltext_list_entry_t entry;

    if (i > 0) {
      printf(", ");
    }

    entry = listEntries[i];
    printf("%lu", (unsigned long) entry);
  }

  printf(")");
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of entries
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_NumEntriesListFulltextIndex (const TRI_fulltext_list_t* const list) {
  return GetNumEntries(list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the first list entry
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_list_entry_t* TRI_StartListFulltextIndex (const TRI_fulltext_list_t* const list) {
  return GetStart(list);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
