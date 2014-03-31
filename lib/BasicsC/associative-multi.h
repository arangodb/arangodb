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
/// @author Max Neunhoeffer
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
// --SECTION--                                        MULTI ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array of pointers
///
/// This is a data structure that can store pointers to objects. Each object
/// has a unique key (for example a certain attribute) and multiple
/// objects in the associative array can have the same key. Every object
/// can be at most once in the array. 
/// We want to offer constant time complexity for the following
/// operations:
///  - insert pointer to an object into the array
///  - lookup pointer to an object in the array
///  - delete pointer to an object from the array
///  - find one pointer to an object with a given key
/// Furthermore, we want to offer O(n) complexity for the following
/// operation:
///  - find all pointers whose objects have a given key k, where n is
///    the number of objects in the array with this key
/// To this end, we use a hash table and ask the user to provide the following:
///  - a way to hash objects by keys, and to hash keys themselves, 
///  - a way to hash objects by their full identity
///  - a way to compare a key to the key of a given object
///  - a way to compare two elements, either by their keys or by their full 
///    identities.
/// To avoid unnecessary comparisons the user can guarantee that s/he will
/// only try to store non-identical elements into the array. This enables
/// the code to skip comparisons which would otherwise be necessary to
/// ensure uniqueness.
/// The idea of the algorithm is as follows: Each slot in the hash table
/// contains a pointer to the actual object, as well as two unsigned
/// integers "prev" and "next" (being indices in the hash table) to
/// organise a linked list of entries, *within the same hash table*. All
/// objects with the same key are kept in a doubly linked list. The first
/// element in such a linked list is kept at the position determined by
/// its hash with respect to its key (or in the first free slot after this
/// position). All further elements in such a linked list are kept at the
/// position determined by its hash with respect to its full identity
/// (or in the first free slot after this position). Provided the hash
/// table is large enough and the hash functions distribute well enough,
/// this gives the proposed complexity.
///
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_multi_pointer_index_t;
#define TRI_MULTI_POINTER_INVALID_INDEX (((TRI_multi_pointer_index_t)0)-1)

typedef struct TRI_multi_pointer_entry_s {
  void* ptr;   // a pointer to the data stored in this slot
  TRI_multi_pointer_index_t next;  // index of the data following in the linked
                                   // list of all items with the same key
  TRI_multi_pointer_index_t prev;  // index of the data preceding in the linked
                                   // list of all items with the same key
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

  TRI_multi_pointer_entry_t* _table_alloc;   // the table itself
  TRI_multi_pointer_entry_t* _table;         // the table itself, 64 aligned

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

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                                     bool const overwrite,
                                     bool const checkEquality);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementMultiPointer (TRI_multi_pointer_t*, void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeMultiPointer (TRI_multi_pointer_t*, size_t);

// -----------------------------------------------------------------------------
// --SECTION--                     MULTI ASSOCIATIVE POINTERS WITH MULTIPLE KEYS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative multi array of pointers with multiple keys
///
/// This is a data structure that can store pairs (p,k) where p is a
/// pointer to an object and k is a pointer to one of the keys of the
/// object. Each object has one ore more keys (for example multiple
/// values in a list in a certain attribute) and multiple objects in the
/// associative array can have the same key. Every pair (p,k) can be at
/// most once in the array.
/// We want to offer constant time complexity for the following
/// operations:
///  - insert a pair into the array
///  - delete a pair from the array
///  - find one pair (p,k) with a given key k
/// Furthermore, we want to offer O(n) complexity for the following
/// operation:
///  - find the list of pointers p for which there is at least one pair
///    (p,k) in the array, where n is the number of objects in the array
///    with this key
/// To this end, we use a hash table and ask the user to provide the following:
///  - a way to hash a pair (p,k)
///  - a way to hash a key k
///  - a way to compare two keys
///  - a way to compare two elements
/// To avoid unnecessary comparisons the user can guarantee that s/he will
/// only try to store non-identical pairs into the array. This enables
/// the code to skip comparisons which would otherwise be necessary to
/// ensure uniqueness.
/// The idea of the algorithm is as follows: Each slot in the hash table
/// contains a pointer to the actual object, a pointer to its key, as
/// well as two unsigned integers "prev" and "next" (being indices in
/// the hash table) to organise a linked list of entries, *within the
/// same hash table*. All pairs with the same key are kept in a doubly
/// linked list. The first element in such a linked list is kept at the
/// position determined by the hash of its key (or in the first free
/// slot after this position). All further pairs in such a linked list
/// are kept at the position determined by the hash of the pair. (or in
/// the first free slot after this position). Provided the hash table
/// is large enough and the hash functions distribute well enough, this
/// gives the proposed complexity.
///
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_multi_pointer_key_entry_s {
  void* ptr;   // a pointer to the data stored in this slot
  void* key;   // a pointer to the key stored in this slot
  TRI_multi_pointer_index_t next;  // index of the data following in the linked
                                   // list of all items with the same key
  TRI_multi_pointer_index_t prev;  // index of the data preceding in the linked
                                   // list of all items with the same key
} TRI_multi_pointer_key_entry_t;

typedef struct TRI_multi_pointer_key_s {
  uint64_t (*hashKey) (struct TRI_multi_pointer_s*, void const* key);
  uint64_t (*hashElement) (struct TRI_multi_pointer_s*, void const* element);

  bool (*isEqualKeyKey) (struct TRI_multi_pointer_s*, void const* key1, 
                         void const* key2);
  bool (*isEqualElementElement) (struct TRI_multi_pointer_s*, 
                                 void const* el1, void const* el2);

  uint64_t _nrAlloc;     // the size of the table
  uint64_t _nrUsed;      // the number of used entries

  TRI_multi_pointer_key_entry_t* _table;         // the table itself

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
TRI_multi_pointer_key_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitMultiPointerKey (
                          TRI_multi_pointer_key_t* array,
                          TRI_memory_zone_t*,
                          uint64_t (*hashKey) (TRI_multi_pointer_key_t*, 
                                               void const*),
                          uint64_t (*hashElement) (TRI_multi_pointer_key_t*, 
                                                   void const*),
                          bool (*isEqualKeyKey) (TRI_multi_pointer_key_t*, 
                                                 void const*, void const*),
                          bool (*isEqualElementElement) (
                                                 TRI_multi_pointer_key_t*, 
                                                 void const*, 
                                                 void const*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMultiPointerKey (TRI_multi_pointer_key_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeMultiPointerKey (TRI_memory_zone_t*, TRI_multi_pointer_key_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyMultiPointerKey (
                                                  TRI_memory_zone_t*,
                                                  TRI_multi_pointer_key_t*,
                                                  void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementMultiPointerKey (TRI_multi_pointer_key_t*,
                                     void* element,
                                     void* key,
                                     bool const overwrite,
                                     bool const checkEquality);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementMultiPointerKey (TRI_multi_pointer_key_t*, 
                                        void const* element,
                                        void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the array
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeMultiPointerKey (TRI_multi_pointer_key_t*, size_t);

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
