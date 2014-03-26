////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array implementation
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_ASSOCIATIVE_MULTI_H
#define TRIAGENS_BASICS_C_ASSOCIATIVE_MULTI_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                           MULTI ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_multi_array_s {
  uint64_t (*hashKey) (struct TRI_multi_array_s*, void*);
  uint64_t (*hashElement) (struct TRI_multi_array_s*, void*);

  void (*clearElement) (struct TRI_multi_array_s*, void*);

  bool (*isEmptyElement) (struct TRI_multi_array_s*, void*);
  bool (*isEqualKeyElement) (struct TRI_multi_array_s*, void*, void*);
  bool (*isEqualElementElement) (struct TRI_multi_array_s*, void*, void*);

  uint32_t _elementSize; // the size of the elements which are to be stored in the table

  uint64_t _nrAlloc;     // the size of the table
  uint64_t _nrUsed;      // the number of used entries

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
TRI_multi_array_t;

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

int TRI_InitMultiArray (TRI_multi_array_t*,
                        TRI_memory_zone_t*,
                        size_t elementSize,
                        uint64_t (*hashKey) (TRI_multi_array_t*, void*),
                        uint64_t (*hashElement) (TRI_multi_array_t*, void*),
                        void (*clearElement) (TRI_multi_array_t*, void*),
                        bool (*isEmptyElement) (TRI_multi_array_t*, void*),
                        bool (*isEqualKeyElement) (TRI_multi_array_t*, void*, void*),
                        bool (*isEqualElementElement) (TRI_multi_array_t*, void*, void*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiArray (TRI_multi_array_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiArray (TRI_memory_zone_t*, TRI_multi_array_t*);

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

TRI_vector_pointer_t TRI_LookupByKeyMultiArray (TRI_memory_zone_t*,
                                                TRI_multi_array_t*,
                                                void* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByElementMultiArray (TRI_memory_zone_t*,
                                                    TRI_multi_array_t*,
                                                    void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementMultiArray (TRI_multi_array_t*, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyMultiArray (TRI_multi_array_t*, void* key, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementMultiArray (TRI_multi_array_t*, void* element, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyMultiArray (TRI_multi_array_t*, void* key, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        MULTI ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array of pointers
///
/// This is a data structure that can store pointers to objects. Each object
/// has a key and multiple objects in the associative array can have the
/// same key. We want to offer constant time complexity for the following
/// operations:
///  - insert pointer to an object into array
///  - lookup pointer to an object in array
///  - delete pointer to an object from array
///  - find one pointer to an object with a given key
/// Furthermore, we want to offer O(k) complexity for the following
/// operation:
///  - find all pointers whose thingies have a given key, where k is
///    the number of thingies in the array with this key
/// To this end, we use a hash table and ask the user to provide the following:
///  - a way to hash objects by keys, and to hash keys themselves, 
///  - a way to hash objects by their full identity
///  - a way to compare a key to the key of a given object
///  - a way to compare two elements, either by their keys or by their full 
///    identities.
/// To avoid unnecessary comparisons the user can guarantee that s/he will
/// only ever store non-identical elements into the array.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_multi_pointer_entry_s {
  void* ptr;   // a pointer to the data stored in this slot
  void* next;  // a pointer to the data following in the linked list of all
               // items with the same key
  void* prev;  // a pointer to the data preceding in the linked list of all
               // items with the same key
} TRI_multi_pointer_entry_t;

typedef struct TRI_multi_pointer_s {
  uint64_t (*hashKey) (struct TRI_multi_pointer_s*, void const* key);
  uint64_t (*hashElement) (struct TRI_multi_pointer_s*, void const* element, 
                           bool byKey);

  bool (*isEqualKeyElement) (struct TRI_multi_pointer_s*, void const* key, 
                             void const* element);
  bool (*isEqualElementElement) (struct TRI_multi_pointer_s*, 
                                 void const* el1, void const* el2, bool byKey);

  uint64_t _nrAlloc;     // the size of the table
  uint64_t _nrUsed;      // the number of used entries

  TRI_multi_pointer_entry_t* _table;         // the table itself

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
TRI_multi_pointer_t;

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

int TRI_InitMultiPointer (TRI_multi_pointer_t* array,
                          TRI_memory_zone_t*,
                          uint64_t (*hashKey) (TRI_multi_pointer_t*, 
                                               void const*),
                          uint64_t (*hashElement) (TRI_multi_pointer_t*, 
                                                   void const*, bool),
                          bool (*isEqualKeyElement) (TRI_multi_pointer_t*, 
                                                     void const*, void const*),
                          bool (*isEqualElementElement) (TRI_multi_pointer_t*, 
                                                         void const*, 
                                                         void const*, bool));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiPointer (TRI_multi_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiPointer (TRI_memory_zone_t*, TRI_multi_pointer_t*);

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

TRI_vector_pointer_t TRI_LookupByKeyMultiPointer (TRI_memory_zone_t*,
                                                  TRI_multi_pointer_t*,
                                                  void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementMultiPointer (TRI_multi_pointer_t*, 
                                       void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementMultiPointer (TRI_multi_pointer_t*,
                                     void*,
                                     const bool overwrite,
                                     const bool checkEquality);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementMultiPointer (TRI_multi_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeMultiPointer (TRI_multi_pointer_t*, size_t);

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
