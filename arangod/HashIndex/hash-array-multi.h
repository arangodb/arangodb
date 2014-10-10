////////////////////////////////////////////////////////////////////////////////
/// @brief multi-hash array implementation, using a linked-list for collisions
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
/// @author Dr. Oreste Costa-Panaia
/// @author Martin Schoenert
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HASH_INDEX_HASH__ARRAY_MULTI_H
#define ARANGODB_HASH_INDEX_HASH__ARRAY_MULTI_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_hash_index_element_multi_s;
struct TRI_index_search_value_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_array_multi_s {
  size_t _numFields; // the number of fields indexes

  uint64_t _nrAlloc; // the size of the table
  uint64_t _nrUsed;  // the number of used entries
  uint64_t _nrOverflowUsed;  // the number of overflow entries used
  uint64_t _nrOverflowAlloc;  // the number of overflow entries allocated

  struct TRI_hash_index_element_multi_s* _table; // the table itself, aligned to a cache line boundary
  struct TRI_hash_index_element_multi_s* _tablePtr; // the table itself

  struct TRI_hash_index_element_multi_s* _freelist;

  TRI_vector_pointer_t   _blocks;
}
TRI_hash_array_multi_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitHashArrayMulti (TRI_hash_array_multi_t*,
                            size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArrayMulti (TRI_hash_array_multi_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArrayMulti (TRI_hash_array_multi_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryUsageHashArrayMulti (TRI_hash_array_multi_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeHashArrayMulti (TRI_hash_array_multi_t*,
                              size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hash_array_multi_t const*,
                                                    struct TRI_index_search_value_s const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementHashArrayMulti (TRI_hash_array_multi_t*,
                                     struct TRI_index_search_value_s const*,
                                     struct TRI_hash_index_element_multi_s*,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementHashArrayMulti (TRI_hash_array_multi_t*,
                                     struct TRI_index_search_value_s const*,
                                     struct TRI_hash_index_element_multi_s*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
