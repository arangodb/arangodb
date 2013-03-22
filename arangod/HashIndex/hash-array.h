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

#ifndef TRIAGENS_HASH_INDEX_HASH_ARRAY_H
#define TRIAGENS_HASH_INDEX_HASH_ARRAY_H 1

#include "BasicsC/common.h"
#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_hash_index_element_s;
struct TRI_index_search_value_s;

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

typedef struct TRI_hash_array_s {
  size_t _numFields; // the number of fields indexes

  uint64_t _nrAlloc; // the size of the table
  uint64_t _nrUsed; // the number of used entries

  struct TRI_hash_index_element_s* _table; // the table itself, aligned to a cache line boundary

#ifdef TRI_INTERNAL_STATS
  uint64_t _nrFinds; // statistics: number of lookup calls
  uint64_t _nrAdds; // statistics: number of insert calls
  uint64_t _nrRems; // statistics: number of remove calls
  uint64_t _nrResizes; // statistics: number of resizes

  uint64_t _nrProbesF; // statistics: number of misses while looking up
  uint64_t _nrProbesA; // statistics: number of misses while inserting
  uint64_t _nrProbesD; // statistics: number of misses while removing
  uint64_t _nrProbesR; // statistics: number of misses while adding
#endif
}
TRI_hash_array_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH ARRAY
// -----------------------------------------------------------------------------

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

int TRI_InitHashArray (TRI_hash_array_t*,
                       size_t initialDocumentCount,
                       size_t numFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArray (TRI_hash_array_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArray (TRI_hash_array_t*);

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

struct TRI_hash_index_element_s* TRI_LookupByKeyHashArray (TRI_hash_array_t*,
                                                           struct TRI_index_search_value_s* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

struct TRI_hash_index_element_s* TRI_FindByKeyHashArray (TRI_hash_array_t*,
                                                         struct TRI_index_search_value_s* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

struct TRI_hash_index_element_s* TRI_LookupByElementHashArray (TRI_hash_array_t*,
                                                               struct TRI_hash_index_element_s* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

struct TRI_hash_index_element_s* TRI_FindByElementHashArray (TRI_hash_array_t*,
                                                             struct TRI_hash_index_element_s* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementHashArray (TRI_hash_array_t*,
                                struct TRI_hash_index_element_s* element,
                                bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyHashArray (TRI_hash_array_t*,
                            struct TRI_index_search_value_s* key,
                            struct TRI_hash_index_element_s* element,
                            bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArray (TRI_hash_array_t*,
                                struct TRI_hash_index_element_s* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeyHashArray (TRI_hash_array_t*,
                            struct TRI_index_search_value_s* key);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  MULTI HASH ARRAY
// -----------------------------------------------------------------------------

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

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hash_array_t*,
                                                    struct TRI_index_search_value_s* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByElementHashArrayMulti (TRI_hash_array_t*,
                                                        struct TRI_hash_index_element_s* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementHashArrayMulti (TRI_hash_array_t*,
                                     struct TRI_hash_index_element_s* element,
                                     bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyHashArrayMulti (TRI_hash_array_t*,
                                 struct TRI_index_search_value_s* key,
                                 struct TRI_hash_index_element_s* element,
                                 bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArrayMulti (TRI_hash_array_t*,
                                     struct TRI_hash_index_element_s* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeyHashArrayMulti (TRI_hash_array_t*,
                                  struct TRI_index_search_value_s* key);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
