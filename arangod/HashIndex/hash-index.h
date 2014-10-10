////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HASH_INDEX_HASH__INDEX_H
#define ARANGODB_HASH_INDEX_HASH__INDEX_H 1

#include "Basics/Common.h"

#include "HashIndex/hash-array.h"
#include "HashIndex/hash-array-multi.h"
#include "VocBase/index.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t;
struct TRI_shaped_sub_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index element
///
/// This structure is used for the elements of an hash index.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_index_element_s {
  struct TRI_doc_mptr_t*   _document;
  struct TRI_shaped_sub_s* _subObjects;
}
TRI_hash_index_element_t;

typedef struct TRI_hash_index_element_multi_s {
  struct TRI_doc_mptr_t*   _document;
  struct TRI_shaped_sub_s* _subObjects;
  struct TRI_hash_index_element_multi_s* _next;
}
TRI_hash_index_element_multi_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_index_s {
  TRI_index_t base;

  union {
    TRI_hash_array_t _hashArray;   // the hash array itself, unique values
    TRI_hash_array_multi_t _hashArrayMulti;   // the hash array itself, non-unique values
  };
  TRI_vector_t     _paths;       // a list of shape pid which identifies the fields of the index
}
TRI_hash_index_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (struct TRI_document_collection_t*,
                                  TRI_idx_iid_t,
                                  TRI_vector_pointer_t*,
                                  TRI_vector_t*,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

TRI_index_result_t TRI_LookupHashIndex (TRI_index_t*,
                                        struct TRI_index_search_value_s*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
