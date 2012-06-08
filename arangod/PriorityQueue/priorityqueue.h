////////////////////////////////////////////////////////////////////////////////
/// @brief priorty queue
///
/// @file
///
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
/// @author Dr. Frank Celler
/// @author Dr. O
/// @author Martin Schoenert
/// @author Copyright 2009-2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_PRIORITY_QUEUE_H
#define TRIAGENS_BASICS_C_PRIORITY_QUEUE_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"


#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Currently only double values are supported.
// -----------------------------------------------------------------------------




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
  // Additional hidden extenral structure used outside this priority queue
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
  // default pq add, remove ,top methods
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
