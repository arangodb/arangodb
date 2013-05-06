////////////////////////////////////////////////////////////////////////////////
/// @brief associative array implementation
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
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_ASSOCIATIVE_H
#define TRIAGENS_BASICS_C_ASSOCIATIVE_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_associative_array_s {
  uint64_t (*hashKey) (struct TRI_associative_array_s*, void*);
  uint64_t (*hashElement) (struct TRI_associative_array_s*, void*);

  void (*clearElement) (struct TRI_associative_array_s*, void*);

  bool (*isEmptyElement) (struct TRI_associative_array_s*, void*);
  bool (*isEqualKeyElement) (struct TRI_associative_array_s*, void*, void*);
  bool (*isEqualElementElement) (struct TRI_associative_array_s*, void*, void*);


  uint32_t _elementSize;

  uint32_t _nrAlloc;     // the size of the table
  uint32_t _nrUsed;      // the number of used entries

  char* _table;          // the table itself

#ifdef TRI_INTERNAL_STATS
  uint64_t _nrFinds;     // statistics: number of lookup calls
  uint64_t _nrAdds;      // statistics: number of insert calls
  uint64_t _nrRems;      // statistics: number of remove calls
  uint64_t _nrResizes;   // statistics: number of resizes

  uint64_t _nrProbesF;   // statistics: number of misses while looking up
  uint64_t _nrProbesA;   // statistics: number of misses while inserting
  uint64_t _nrProbesD;   // statistics: number of misses while removing
  uint64_t _nrProbesR;   // statistics: number of misses while adding
#endif

  TRI_memory_zone_t* _memoryZone;
}
TRI_associative_array_t;

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
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativeArray (TRI_associative_array_t*,
                               TRI_memory_zone_t*,
                               size_t elementSize,
                               uint64_t (*hashKey) (TRI_associative_array_t*, void*),
                               uint64_t (*hashElement) (TRI_associative_array_t*, void*),
                               void (*clearElement) (TRI_associative_array_t*, void*),
                               bool (*isEmptyElement) (TRI_associative_array_t*, void*),
                               bool (*isEqualKeyElement) (TRI_associative_array_t*, void*, void*),
                               bool (*isEqualElementElement) (TRI_associative_array_t*, void*, void*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeArray (TRI_associative_array_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeArray (TRI_memory_zone_t*, TRI_associative_array_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativeArray (TRI_associative_array_t*, void* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeyAssociativeArray (TRI_associative_array_t*, void* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativeArray (TRI_associative_array_t*, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementAssociativeArray (TRI_associative_array_t*, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementAssociativeArray (TRI_associative_array_t*, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyAssociativeArray (TRI_associative_array_t*, void* key, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementAssociativeArray (TRI_associative_array_t*, void* element, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyAssociativeArray (TRI_associative_array_t*, void* key, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativeArray (const TRI_associative_array_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array of pointers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_associative_pointer_s {
  uint64_t (*hashKey) (struct TRI_associative_pointer_s*, void const*);
  uint64_t (*hashElement) (struct TRI_associative_pointer_s*, void const*);

  bool (*isEqualKeyElement) (struct TRI_associative_pointer_s*, void const*, void const*);
  bool (*isEqualElementElement) (struct TRI_associative_pointer_s*, void const*, void const*);

  uint32_t _nrAlloc;     // the size of the table
  uint32_t _nrUsed;      // the number of used entries

  void** _table;         // the table itself

#ifdef TRI_INTERNAL_STATS
  uint64_t _nrFinds;     // statistics: number of lookup calls
  uint64_t _nrAdds;      // statistics: number of insert calls
  uint64_t _nrRems;      // statistics: number of remove calls
  uint64_t _nrResizes;   // statistics: number of resizes

  uint64_t _nrProbesF;   // statistics: number of misses while looking up
  uint64_t _nrProbesA;   // statistics: number of misses while inserting
  uint64_t _nrProbesD;   // statistics: number of misses while removing
  uint64_t _nrProbesR;   // statistics: number of misses while adding
#endif

  TRI_memory_zone_t* _memoryZone;
}
TRI_associative_pointer_t;

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
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativePointer (TRI_associative_pointer_t* array,
                                 TRI_memory_zone_t*,
                                 uint64_t (*hashKey) (TRI_associative_pointer_t*, void const*),
                                 uint64_t (*hashElement) (TRI_associative_pointer_t*, void const*),
                                 bool (*isEqualKeyElement) (TRI_associative_pointer_t*, void const*, void const*),
                                 bool (*isEqualElementElement) (TRI_associative_pointer_t*, void const*, void const*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativePointer (TRI_associative_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativePointer (TRI_memory_zone_t*, TRI_associative_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a pointer
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashPointerKeyAssociativePointer (TRI_associative_pointer_t*,
                                               void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a string key
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashStringKeyAssociativePointer (TRI_associative_pointer_t*,
                                              void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief General function to determine equality of two string values
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualStringKeyAssociativePointer (TRI_associative_pointer_t*, void const*, void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativePointer (TRI_associative_pointer_t*, void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativePointer (TRI_associative_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativePointer (TRI_associative_pointer_t*, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativePointer (TRI_associative_pointer_t*, void const* key, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativePointer (TRI_associative_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativePointer (TRI_associative_pointer_t*, void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativePointer (const TRI_associative_pointer_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                ASSOCIATIVE SYNCED
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array of synced pointers
///
/// Note that lookup, insert, and remove are protected using a read-write lock.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_associative_synced_s {
  uint64_t (*hashKey) (struct TRI_associative_synced_s*, void const*);
  uint64_t (*hashElement) (struct TRI_associative_synced_s*, void const*);

  bool (*isEqualKeyElement) (struct TRI_associative_synced_s*, void const*, void const*);
  bool (*isEqualElementElement) (struct TRI_associative_synced_s*, void const*, void const*);

  uint32_t _nrAlloc;     // the size of the table
  uint32_t _nrUsed;      // the number of used entries

  void** _table;         // the table itself

  TRI_read_write_lock_t _lock;

#ifdef TRI_INTERNAL_STATS
  uint64_t _nrFinds;     // statistics: number of lookup calls
  uint64_t _nrAdds;      // statistics: number of insert calls
  uint64_t _nrRems;      // statistics: number of remove calls
  uint64_t _nrResizes;   // statistics: number of resizes

  uint64_t _nrProbesF;   // statistics: number of misses while looking up
  uint64_t _nrProbesA;   // statistics: number of misses while inserting
  uint64_t _nrProbesD;   // statistics: number of misses while removing
  uint64_t _nrProbesR;   // statistics: number of misses while adding
#endif

  TRI_memory_zone_t* _memoryZone;
}
TRI_associative_synced_t;

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
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

void TRI_InitAssociativeSynced (TRI_associative_synced_t* array,
                                TRI_memory_zone_t*,
                                uint64_t (*hashKey) (TRI_associative_synced_t*, void const*),
                                uint64_t (*hashElement) (TRI_associative_synced_t*, void const*),
                                bool (*isEqualKeyElement) (TRI_associative_synced_t*, void const*, void const*),
                                bool (*isEqualElementElement) (TRI_associative_synced_t*, void const*, void const*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeSynced (TRI_associative_synced_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeSynced (TRI_memory_zone_t*, TRI_associative_synced_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByKeyAssociativeSynced (TRI_associative_synced_t*, void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByElementAssociativeSynced (TRI_associative_synced_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativeSynced (TRI_associative_synced_t*, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativeSynced (TRI_associative_synced_t*, void const* key, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativeSynced (TRI_associative_synced_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativeSynced (TRI_associative_synced_t*, void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativeSynced (TRI_associative_synced_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
