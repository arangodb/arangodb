////////////////////////////////////////////////////////////////////////////////
/// @brief priority queue index
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_PRIORITY_QUEUE_PQUEUEINDEX_H
#define TRIAGENS_PRIORITY_QUEUE_PQUEUEINDEX_H 1

#include "BasicsC/common.h"
#include "PriorityQueue/priorityqueue.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------------------------------------------------
// --SECTION--                                 priority queue index public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


// ...............................................................................
// Define the structure of a priority queue index
// Currently the priority queue defines its own storage for the position integer
// into the priority queue array. That is, we do not store a 'hidden' attribute
// in the doc.
// ...............................................................................

typedef struct {
  TRI_pqueue_t* _pq; // the actual priority queue
  TRI_associative_array_t* _aa; // storage for the pointer into the pq array
} PQIndex;

// ...............................................................................
// Currently only support one shaped_json field (attribute) of the type 'number'
// Can extend later
// ...............................................................................

typedef struct {
  size_t numFields;          // the number of fields
  TRI_shaped_json_t* fields; // list of shaped json objects which the collection should know about
  void* data;                // master document pointer
  void* collection;          // pointer to the collection;
  uint64_t pqSlot;           // int pointer to the position in the pq array
} PQIndexElement;


typedef struct {
  size_t _numElements;
  PQIndexElement* _elements; // simple list of elements
} PQIndexElements;



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--            Priority Queue Index      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief reclaims memory allocated for the index, releasing the Priority
/// Queue  storage and Associative Array storage.
////////////////////////////////////////////////////////////////////////////////

void PQueueIndex_destroy (PQIndex*);

void PQueueIndex_free (PQIndex*);


////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the index
////////////////////////////////////////////////////////////////////////////////

PQIndex* PQueueIndex_new (void);


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--            Priority Queue Index                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an item into a priority queue
////////////////////////////////////////////////////////////////////////////////

int PQIndex_add (PQIndex*, PQIndexElement*);


////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an item into a priority queue (same as add method above)
////////////////////////////////////////////////////////////////////////////////

int PQIndex_insert (PQIndex*, PQIndexElement*);


////////////////////////////////////////////////////////////////////////////////
/// @brief removes an item from the priority queue (not necessarily the top most)
////////////////////////////////////////////////////////////////////////////////

int PQIndex_remove (PQIndex*, PQIndexElement*);


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the top most item without removing it from the queue
////////////////////////////////////////////////////////////////////////////////

PQIndexElements* PQIndex_top (PQIndex*, uint64_t);


////////////////////////////////////////////////////////////////////////////////
/// @brief removes an item and inserts a new item
////////////////////////////////////////////////////////////////////////////////

bool PQIndex_update (PQIndex*, const PQIndexElement*, const PQIndexElement*);

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

