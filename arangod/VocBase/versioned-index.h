////////////////////////////////////////////////////////////////////////////////
/// @brief versioned index of a collection
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_VERSIONED_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_VERSIONED_INDEX_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_s;
struct TRI_doc_operation_context_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                   VERSIONED INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief versioned index of a collection
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_versioned_index_s {
  struct TRI_doc_mptr_s** _table;
  uint64_t                _nrAlloc;
  uint64_t                _nrUsed;
  TRI_read_write_lock_t   _lock;
}
TRI_versioned_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the versioned index
////////////////////////////////////////////////////////////////////////////////

TRI_versioned_index_t* TRI_CreateVersionedIndex (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the versioned index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVersionedIndex (TRI_versioned_index_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVersionedIndex (TRI_versioned_index_t* const idx,
                              struct TRI_doc_operation_context_s* const context, 
                              struct TRI_doc_mptr_s* const doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an element in the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateVersionedIndex (TRI_versioned_index_t* const,
                              struct TRI_doc_operation_context_s* const, 
                              struct TRI_doc_mptr_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteVersionedIndex (TRI_versioned_index_t* const,
                              struct TRI_doc_operation_context_s* const,
                              struct TRI_doc_mptr_s* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
