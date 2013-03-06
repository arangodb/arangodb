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


#include "priorityqueue.h"
#include "ShapedJson/shaped-json.h"
#include <BasicsC/logging.h>

// -----------------------------------------------------------------------------
// --SECTION--                                       useful forward declarations
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////
static bool  AddPQueue       (TRI_pqueue_t*, void*);
static bool  CheckPQSize     (TRI_pqueue_t*);
//static bool  CompareIsLess   (TRI_pqueue_t*, void*, void*);
static bool  FixPQ           (TRI_pqueue_t*, uint64_t); 
static char* GetItemAddress  (TRI_pqueue_t*, uint64_t);
static bool  RemovePQueue    (TRI_pqueue_t*, uint64_t, bool);
static void* TopPQueue       (TRI_pqueue_t*);  

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
  
// -----------------------------------------------------------------------------
// --SECTION--                      priority queue  constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a priority queue
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitPQueue (TRI_pqueue_t* pq, size_t initialCapacity, size_t itemSize, bool reverse, 
                     void (*clearStorage) (struct TRI_pqueue_s*, void*), 
                     uint64_t (*getStorage) (struct TRI_pqueue_s*, void*), 
                     bool (*isLess) (struct TRI_pqueue_s*, void*, void*), 
                     void (*updateStorage) (struct TRI_pqueue_s*, void*, uint64_t pos)) {

  if (pq == NULL) {
    return false;
  }
  
  // ..........................................................................  
  // Assign the call back functions
  // ..........................................................................  
  

  // ..........................................................................  
  // This callback is used to remove any memory which may be used as part of
  // the storage within the array _items. Callback required since we do not
  // know if there is any internal structure.
  // ..........................................................................  
  pq->clearStoragePQ = clearStorage;
  
  
  // ..........................................................................  
  // Returns the position within the _items array the element is.
  // Currently this simply is used to perform a consistency check.
  // ..........................................................................  
  pq->getStoragePQ = getStorage;
  
  
  // ..........................................................................  
  // The actual comparison function which returns true if left item is
  // less than right item (otherwise false).
  // ..........................................................................    
  pq->isLessPQ  = isLess;
  
  
  // ..........................................................................  
  // Stores the position of the element within the _items array. Its purpose
  // is to allow the storage position to be located independent of the 
  // priority queue _items array.
  // ..........................................................................    
  pq->updateStoragePQ = updateStorage;

  
  // ..........................................................................  
  // Assign the element size
  // ..........................................................................  
  pq->_base._itemSize = itemSize;


  // ..........................................................................  
  // Initialise invalid access counters
  // ..........................................................................  
  pq->_base._nrFalseRemoves = 0;
  pq->_base._nrFalseAdds    = 0;

  
  // ..........................................................................  
  // Set the capacity and assign memeory for storage
  // ..........................................................................  
  pq->_base._capacity = initialCapacity;
  pq->_base._items    = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, pq->_base._itemSize * pq->_base._capacity, false);
  if (pq->_base._items == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating priority queue storage");
    return false;    
  }
  
  // ..........................................................................  
  // Initialise the memory allcoated for the storage of pq (0 filled)
  // ..........................................................................  
  
  memset(pq->_base._items, 0, pq->_base._itemSize * pq->_base._capacity);

  
  // ..........................................................................  
  // initialise the number of items stored
  // ..........................................................................  
  pq->_base._count = 0;
  
  // ..........................................................................  
  // Determines if the pq should be reversed 
  // ..........................................................................  
  pq->_base._reverse = reverse;

  
  // ..........................................................................  
  // The static call back functions to query, modifiy the pq
  // ..........................................................................  
  pq->add    = AddPQueue;
  pq->remove = RemovePQueue;
  pq->top    = TopPQueue;
  
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a priority queue, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPQueue(TRI_pqueue_t* pq) {
  size_t j; 
  void* thisItem;

  if (pq == NULL) {
    return;
  }  
  
  for (j = 0; j < pq->_base._count; ++j) {
    thisItem = (j * pq->_base._itemSize) + pq->_base._items;
    pq->clearStoragePQ(pq,thisItem);
  }
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, pq->_base._items);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a priority queue and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePQueue(TRI_pqueue_t* pq) {
  if (pq == NULL) {
    return;
  }  
  TRI_DestroyPQueue(pq);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, pq);
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                                priority queue  callback functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an item into the priority queue
////////////////////////////////////////////////////////////////////////////////

static bool AddPQueue(TRI_pqueue_t* pq, void* item) {
  size_t k;

  
  // ...........................................................................
  // Some basic checks for consistency
  // ...........................................................................
  
  if (pq == NULL) {
    return false;
  }
  
  if (pq->getStoragePQ(pq,item) != 0) {
    pq->_base._nrFalseAdds++;
  }

  
  // ...........................................................................
  // handle case that pq is too small
  // ...........................................................................
  
  if (!CheckPQSize(pq)) {
    return false;
  }  
  
  
  // ...........................................................................
  // increase the number of items in the queue
  // ...........................................................................
  
  pq->_base._count++;

  
  // ...........................................................................  
  // add the item to the end
  // ...........................................................................
  k = (pq->_base._count - 1) * pq->_base._itemSize;
  memcpy( k + pq->_base._items, item,  pq->_base._itemSize);
  

  // ...........................................................................  
  // call back so that the item can store the position of the entry into the
  // pq array back into the document (row, whatever).
  // ...........................................................................  
  pq->updateStoragePQ(pq, k + pq->_base._items, pq->_base._count);


  // ...........................................................................  
  // and fix the priority queue (re-sort)
  // ...........................................................................  
  FixPQ(pq, pq->_base._count);

  
  return true;
}


   
////////////////////////////////////////////////////////////////////////////////
/// @brief rmeoves an item from the pq -- needs the position 
////////////////////////////////////////////////////////////////////////////////

static bool RemovePQueue(TRI_pqueue_t* pq, uint64_t position, bool destroyItem) {

  char* lastItem;
  char* thisItem;
  
  if (pq == NULL) {
    return false;
  }

  
  // ...........................................................................
  // Check that the position sent is valid
  // ...........................................................................
  
  if (position == 0 || position > pq->_base._count) {
    pq->_base._nrFalseRemoves++;
    return false;
  }


  // ...........................................................................
  // obtain the pointer positions
  // ...........................................................................
  
  thisItem = ((position - 1) * pq->_base._itemSize) + pq->_base._items;  
  lastItem = ((pq->_base._count - 1) * pq->_base._itemSize) + pq->_base._items;  


  // ...........................................................................
  // Set the position in the priority queue array back in the document 
  // ...........................................................................
  
  pq->updateStoragePQ(pq, thisItem, 0);
  if (destroyItem) {
    pq->clearStoragePQ(pq,thisItem);
  }  
  
  
  // ...........................................................................
  // perhaps we are lucky and the item to be removed is at the end of queue?
  // ...........................................................................
    
  if (position == pq->_base._count) {
    memset(thisItem, 0, pq->_base._itemSize);
    pq->_base._count--;
    return true;
  }

  
  // ...........................................................................
  // No such luck - so work a little harder.
  // ...........................................................................
  
  memcpy(thisItem, lastItem, pq->_base._itemSize);
  memset(lastItem, 0, pq->_base._itemSize);
  pq->_base._count--;
  FixPQ(pq,position);
  
  return true;
}


   


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the top most item in the priority queue
////////////////////////////////////////////////////////////////////////////////

static void* TopPQueue(TRI_pqueue_t* pq) {

  if ( (pq == NULL) || (pq->_base._count == 0) ) {
    return NULL;
  }  

  return (pq->_base._items);
}



 
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                priority queue  helper  functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueue
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief checks if additional capacity is required and extends if so
////////////////////////////////////////////////////////////////////////////////

static bool CheckPQSize(TRI_pqueue_t* pq) {
  char* newItems;
  
  if (pq == NULL) {
    return false;
  }
  
  if (pq->_base._capacity > (pq->_base._count + 1) ) {
    return true;
  }
  
  pq->_base._capacity = pq->_base._capacity * 2;
  // allocate and fill with NUL bytes
  newItems = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, pq->_base._capacity * pq->_base._itemSize, true);
  if (newItems == NULL) {
    return false;    
  }
 
  memcpy(newItems, pq->_base._items, (pq->_base._count * pq->_base._itemSize) );
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, pq->_base._items);
  pq->_base._items = newItems;
  
  return true;  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief rebalances the heap (priority queue storage) using partial order
////////////////////////////////////////////////////////////////////////////////

static bool FixPQ(TRI_pqueue_t* pq, uint64_t position) {

  char* currentItem;   // the element which may be out of place in the heap
  uint64_t currentPos; // the current item we are checking against.
  char* parentItem;
  char* itemAddress;
  char* leftChildItem;
  char* rightChildItem;
  
  currentPos  = position;
  currentItem = (char*)(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, pq->_base._itemSize, false));
  if (currentItem == NULL) { // out of memory
    return false;
  }  
  
  itemAddress = GetItemAddress(pq,currentPos);
  if (itemAddress == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentItem);
    return false;
  }

  
  // ...........................................................................
  // Temporarily save our item just changed (via removal or addition)
  // ...........................................................................
  
  memcpy(currentItem,itemAddress,pq->_base._itemSize);
  

  // ...........................................................................
  // Remember that given position m, it's parent in the heap is given by m/2
  // Remember that the item we have to fix can be in the wrong place either because
  // it is less than it's parent or because it is greater than its children.
  // ...........................................................................
  
  
  // ...........................................................................
  // item is moved up if the item is LESS than its parent.  
  // ...........................................................................
  
  while ((currentPos / 2) >= 1) {
    parentItem  = GetItemAddress(pq, currentPos / 2);
    if (!pq->isLessPQ(pq,currentItem,parentItem)) {
      break;
    }
    itemAddress = GetItemAddress(pq,currentPos);
    memcpy(itemAddress,parentItem,pq->_base._itemSize);
    pq->updateStoragePQ(pq, itemAddress, currentPos);
    currentPos = currentPos / 2;
  }

  
  // ...........................................................................
  // If the position has changed, then we moved the item up somewhere
  // ...........................................................................
  
  if (currentPos != position) {
    itemAddress = GetItemAddress(pq,currentPos);
    memcpy(itemAddress,currentItem,pq->_base._itemSize);
    pq->updateStoragePQ(pq, itemAddress, currentPos);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentItem);
    return true;
  }


  // ...........................................................................
  // ok perhaps item has to move down the heap since it is too heavy
  // ...........................................................................
 
  while (pq->_base._count >= (2 * currentPos)) {
    leftChildItem  = GetItemAddress(pq, (currentPos * 2));
    rightChildItem = GetItemAddress(pq, (currentPos * 2) + 1);

    // this is to keep the heap balanced     
    if ( (pq->_base._count > (2 * currentPos)) && 
         (pq->isLessPQ(pq, rightChildItem,leftChildItem)) && 
         (pq->isLessPQ(pq, rightChildItem, currentItem))
        ) {
      itemAddress = GetItemAddress(pq,currentPos);
      memcpy(itemAddress,rightChildItem,pq->_base._itemSize);
      pq->updateStoragePQ(pq, itemAddress, currentPos);
      currentPos = (2 * currentPos) + 1;
    }
    
    // move item down    
    else if (pq->isLessPQ(pq, leftChildItem, currentItem)) {
      itemAddress = GetItemAddress(pq,currentPos);
      memcpy(itemAddress,leftChildItem,pq->_base._itemSize);
      pq->updateStoragePQ(pq, itemAddress, currentPos);
      currentPos = 2 * currentPos;
    }
    else {
      break;
    }  
  }  
  
  itemAddress = GetItemAddress(pq,currentPos);
  memcpy(itemAddress,currentItem,pq->_base._itemSize);
  pq->updateStoragePQ(pq, itemAddress, currentPos);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, currentItem);
  return true;          
}


/* currently unused
static bool CompareIsLess(TRI_pqueue_t* pq, void* leftItem, void* rightItem) {
  if (pq->_base._reverse) {
    return !pq->isLessPQ(pq, leftItem, rightItem); 
  }
  return pq->isLessPQ(pq, leftItem, rightItem); 
}
*/


////////////////////////////////////////////////////////////////////////////////
/// @brief given a position returns the element stored in the priorty queue 
////////////////////////////////////////////////////////////////////////////////

static char* GetItemAddress(TRI_pqueue_t* pq, uint64_t position) {
  if (position < 1 || position > pq->_base._count) {
    return NULL;
  }

  return ((position - 1) * pq->_base._itemSize) + pq->_base._items;
    
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
