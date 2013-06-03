////////////////////////////////////////////////////////////////////////////////
/// @brief priorty queue
///
/// @file
///
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
/// @author Dr. Oreste Costa-Panaia
/// @author Martin Schoenert
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_PRIORITY_QUEUE_PRIORITYQUEUE_H
#define TRIAGENS_PRIORITY_QUEUE_PRIORITYQUEUE_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"


#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                             skiplist public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief The base structure of a priority queue (pq)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_pqueue_base_s {
  // ...........................................................................
  // The current storage allocated to the pq. This is NOT the number of items
  // in the queue.
  // ...........................................................................

  size_t _capacity;

  // ...........................................................................
  // The storage of the items as an array of element size
  // ...........................................................................

  char* _items;

  // ...........................................................................
  // Size of each item (element)
  // ...........................................................................

  uint32_t _itemSize;

  // ...........................................................................
  // The number of actual elements in the pq
  // ...........................................................................

  size_t _count;

  // ...........................................................................
  // A counter which indicates the number of removals which are invalid.
  // ...........................................................................

  size_t _nrFalseRemoves;

  // ...........................................................................
  // A counter which indicates the number of inserts which are invalid.
  // ...........................................................................

  size_t _nrFalseAdds;

  // ...........................................................................
  // by default (reverse = false), items are stored from lowest to highest
  // so that the items which sort lowest are the ones which are removed first.
  // When this is set to true, the ... reverse is true ...eh? This can also
  // be achieved by the call back comparision function
  // ...........................................................................

  bool _reverse;  
  
  // ...........................................................................
  // Additional hidden external structure used outside this priority queue
  // This hidden structure is not available within this priority queue
  // ...........................................................................
  // char[n]

}
TRI_pqueue_base_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief The structure of a priority queue (pq)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_pqueue_s {  
  TRI_pqueue_base_t _base;
  void (*clearStoragePQ) (struct TRI_pqueue_s*, void*);
  uint64_t (*getStoragePQ) (struct TRI_pqueue_s*, void*);
  bool (*isLessPQ) (struct TRI_pqueue_s*, void*, void*);
  void (*updateStoragePQ) (struct TRI_pqueue_s*, void*, uint64_t pos);


  // ...........................................................................
  // default pq add, remove, top methods
  // ...........................................................................

  bool  (*add)    (struct TRI_pqueue_s*, void*);
  bool  (*remove) (struct TRI_pqueue_s*, uint64_t, bool);
  void* (*top)    (struct TRI_pqueue_s*);

}
TRI_pqueue_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                 Priority Queue       constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a priority queue
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitPQueue (TRI_pqueue_t*, size_t, size_t, bool,
                     void (*clearStoragePQ) (struct TRI_pqueue_s*, void*),
                     uint64_t (*getStoragePQ) (struct TRI_pqueue_s*, void*),
                     bool (*isLessPQ) (struct TRI_pqueue_s*, void*, void*),
                     void (*updateStoragePQ) (struct TRI_pqueue_s*, void*, uint64_t pos));


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPQueue (TRI_pqueue_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePQueue (TRI_pqueue_t*);

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
