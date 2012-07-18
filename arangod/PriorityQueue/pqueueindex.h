////////////////////////////////////////////////////////////////////////////////
/// @brief priority queue index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. O
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_PRIORITY_QUEUE_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_PRIORITY_QUEUE_INDEX_H 1

#include <BasicsC/common.h>
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

