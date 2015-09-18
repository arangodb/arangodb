////////////////////////////////////////////////////////////////////////////////
/// @brief associative array implementation
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_ASSOCIATIVE_H
#define ARANGODB_BASICS_C_ASSOCIATIVE_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAssociativePointer (TRI_associative_pointer_t* array,
                                TRI_memory_zone_t*,
                                uint64_t (*hashKey) (TRI_associative_pointer_t*, void const*),
                                uint64_t (*hashElement) (TRI_associative_pointer_t*, void const*),
                                bool (*isEqualKeyElement) (TRI_associative_pointer_t*, void const*, void const*),
                                bool (*isEqualElementElement) (TRI_associative_pointer_t*, void const*, void const*));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativePointer (TRI_associative_pointer_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a string key
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashStringKeyAssociativePointer (TRI_associative_pointer_t*,
                                              void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief General function to determine equality of two string values
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualStringKeyAssociativePointer (TRI_associative_pointer_t*,
                                           void const*,
                                           void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief reserves space in the array for extra elements
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReserveAssociativePointer (TRI_associative_pointer_t*,
                                    int32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativePointer (TRI_associative_pointer_t*,
                                         void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativePointer (TRI_associative_pointer_t*,
                                             void const* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativePointer (TRI_associative_pointer_t*,
                                           void* element,
                                           bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativePointer (TRI_associative_pointer_t*,
                                       void const* key,
                                       void* element,
                                       bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyAssociativePointer2 (TRI_associative_pointer_t*,
                                      void const*,
                                      void*,
                                      void const**);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativePointer (TRI_associative_pointer_t*,
                                       void const* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativePointer (const TRI_associative_pointer_t* const);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
