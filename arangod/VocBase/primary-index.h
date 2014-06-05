////////////////////////////////////////////////////////////////////////////////
/// @brief primary hash index functionality
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

#ifndef TRIAGENS_VOC_BASE_PRIMARY_INDEX_H
#define TRIAGENS_VOC_BASE_PRIMARY_INDEX_H 1

#include "BasicsC/common.h"

struct TRI_doc_mptr_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array of pointers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_primary_index_s {
  uint32_t _nrAlloc;     // the size of the table
  uint32_t _nrUsed;      // the number of used entries

  void** _table;         // the table itself

  TRI_memory_zone_t* _memoryZone;
}
TRI_primary_index_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the index
////////////////////////////////////////////////////////////////////////////////

int TRI_InitPrimaryIndex (TRI_primary_index_t*,
                          TRI_memory_zone_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_primary_index_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyPrimaryIndex (TRI_primary_index_t*, 
                                   void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the index
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyPrimaryIndex (TRI_primary_index_t*, 
                               struct TRI_doc_mptr_t const*,
                               void const**);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key/element from the index
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyPrimaryIndex (TRI_primary_index_t*, 
                                 void const* key);

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
