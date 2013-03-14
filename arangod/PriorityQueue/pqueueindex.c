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

#include "pqueueindex.h"

#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/primary-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--             priority queue index some useful forward declarations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

// callbacks for the priority queue
static void     ClearStoragePQIndex(TRI_pqueue_t*, void*);
static uint64_t GetStoragePQIndex(TRI_pqueue_t*, void*);
static bool     IsLessPQIndex(TRI_pqueue_t*,void*, void*);
static void     UpdateStoragePQIndex(TRI_pqueue_t*, void*, uint64_t);


// callbacks for the associative array
static void     ClearElementPQIndex(TRI_associative_array_t*, void*);
static uint64_t HashKeyPQIndex(TRI_associative_array_t*, void*);
static uint64_t HashElementPQIndex(TRI_associative_array_t*, void*);
static bool     IsEmptyElementPQIndex(TRI_associative_array_t*, void*);
static bool     IsEqualElementElementPQIndex(TRI_associative_array_t*, void*, void*);
static bool     IsEqualKeyElementPQIndex(TRI_associative_array_t*, void*, void*);

// ...............................................................................
// some internal error numbers which can be mapped later to global error numbers
// ...............................................................................

enum {
  PQIndex_InvalidIndexError = -1,
  PQIndex_InvalidElementError = -10,
  
  PQIndex_ElementMissingAssociativeArrayError = 1,
  PQIndex_DuplicateElement = 10,
  
  PQIndex_RemoveInternalError = 1001
} PQueueIndexErrors;  

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                 priority queue index constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief removes any allocated memory internal to the index structure
////////////////////////////////////////////////////////////////////////////////

void PQueueIndex_destroy(PQIndex* idx) {
  if (idx == NULL) {
    return;
  }
  TRI_FreePQueue(idx->_pq);
  TRI_FreeAssociativeArray(TRI_UNKNOWN_MEM_ZONE, idx->_aa);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the index and frees any allocated memory
////////////////////////////////////////////////////////////////////////////////

void PQueueIndex_free(PQIndex* idx) {
  if (idx == NULL) {
    return;
  }
  PQueueIndex_destroy(idx); 
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a priority queue index
////////////////////////////////////////////////////////////////////////////////



PQIndex* PQueueIndex_new (void) {

  PQIndex* idx;
  bool ok;
  
  // ..........................................................................  
  // Allocate the Priority Que Index
  // ..........................................................................  
  
  idx = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndex), false);

  if (idx == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating priority queue index");
    return NULL;
  }

  
  // ..........................................................................  
  // Allocate the priority que
  // Remember to add any additional structure you need
  // ..........................................................................  
  
  idx->_pq = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_pqueue_t) + sizeof(void*), false);
  if (idx->_pq == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating priority queue index");
    return NULL;
  }

  
  // ..........................................................................  
  // Allocate the associative array
  // ..........................................................................  
  
  idx->_aa = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_associative_array_t), false);
  if (idx->_aa == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_pq);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating priority queue index");
    return NULL;
  }
  

  // ..........................................................................  
  // Initialise the priority que
  // ..........................................................................  
  
  ok = TRI_InitPQueue(idx->_pq,
                      100,
                      sizeof(TRI_pq_index_element_t),
                      false, 
                      ClearStoragePQIndex,
                      GetStoragePQIndex, 
                      IsLessPQIndex, 
                      UpdateStoragePQIndex);

  if (! ok) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_pq);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_aa);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
    return NULL;    
  }

  
  // ..........................................................................  
  // Initialise the associative array
  // ..........................................................................  

  TRI_InitAssociativeArray(idx->_aa,
                           TRI_UNKNOWN_MEM_ZONE, 
                           sizeof(TRI_pq_index_element_t), 
                           HashKeyPQIndex,
                           HashElementPQIndex,
                           ClearElementPQIndex,
                           IsEmptyElementPQIndex,
                           IsEqualKeyElementPQIndex,
                           IsEqualElementElementPQIndex);
                           
                           
  // ..........................................................................  
  // Add the associative array at the end of the pq so that we can access later
  //memcpy((char*)(idx->_pq) + sizeof(TRI_pqueue_t),&(idx->_aa),sizeof(TRI_associative_array_t*));
  // ..........................................................................  

  *((TRI_associative_array_t**)((char*)(idx->_pq) + sizeof(TRI_pqueue_t))) = idx->_aa;
  
  
  return idx;
}


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

int PQIndex_insert (PQIndex* idx, TRI_pq_index_element_t* element) {
  if (idx == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  if (element == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ...........................................................................
  // Check if item is already added to the associative array
  // ...........................................................................

  if (TRI_FindByKeyAssociativeArray(idx->_aa, element->_document) != NULL) {
    // attempt to add duplicate document to the priority queue
    return TRI_ERROR_ARANGO_INDEX_PQ_INSERT_FAILED;
  }
  
  // ...........................................................................
  // Initialise the priority queue array storage pointer
  // ...........................................................................

  element->pqSlot = 0;
  
  // ...........................................................................
  // Add item to associative array
  // ...........................................................................

  if (!TRI_InsertElementAssociativeArray(idx->_aa, element, false)) {
    // can not add item to associative array -- give up on insert
    return TRI_ERROR_ARANGO_INDEX_PQ_INSERT_FAILED;
  }
  
  if (!idx->_pq->add(idx->_pq, element)) {
    TRI_RemoveElementAssociativeArray(idx->_aa, element, NULL);
    // can not add item to priority queue array -- give up on insert
    return TRI_ERROR_ARANGO_INDEX_PQ_INSERT_FAILED;
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an item from the priority queue (not necessarily the top most)
////////////////////////////////////////////////////////////////////////////////

int PQIndex_remove (PQIndex* idx, TRI_doc_mptr_t const* doc) {
  TRI_pq_index_element_t* item;  
  bool ok;
  
  if (idx == NULL) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED;
  }

  // ...........................................................................
  // Check if item exists in the associative array.
  // ...........................................................................

  item = TRI_FindByKeyAssociativeArray(idx->_aa, CONST_CAST(doc));

  if (item == NULL) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_ITEM_MISSING;
  }
  
  // ...........................................................................
  // Remove item from the priority queue
  // ...........................................................................
  
  ok = idx->_pq->remove(idx->_pq, item->pqSlot, true);
 
  // ...........................................................................
  // Remove item from associative array
  // Must come after remove above, since update storage will be called.
  // ...........................................................................
  
  ok = TRI_RemoveElementAssociativeArray(idx->_aa, item, NULL) && ok;
  
  if (!ok) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED;
  }
  
  return TRI_ERROR_NO_ERROR;  
}  

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the top most item without removing it from the queue
////////////////////////////////////////////////////////////////////////////////

PQIndexElements* PQIndex_top (PQIndex* idx, uint64_t numElements) {

  PQIndexElements* result;
  PQIndexElements tempResult;
  TRI_pq_index_element_t* element;  
  uint64_t j;
  bool ok;
  uint64_t numCopied;
  
  
  if (idx == NULL) {
    return NULL;
  }

  if (numElements < 1) {
    return NULL;
  }

  // .............................................................................  
  // Optimise for the common case where we remove only a single element
  // .............................................................................  
  
  if (numElements == 1) {
    result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndexElements), false);

    if (result == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    element = idx->_pq->top(idx->_pq);

    if (element == NULL) {
      result->_numElements = 0;
      result->_elements = NULL;
    }
    else {
      result->_elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_pq_index_element_t) * numElements, false);

      if (result->_elements == NULL) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
        TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
        return NULL;
      }

      result->_numElements = numElements;
      result->_elements[0] = *element;
    }

    return result;
  }  
  
  // .............................................................................  
  // Two or more elements are 'topped'
  // .............................................................................  
  
  tempResult._elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_pq_index_element_t) * numElements, false);

  if (tempResult._elements == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  ok = true;
  numCopied = 0;

  for (j = 0; j < numElements; ++j) {
    element = (TRI_pq_index_element_t*)(idx->_pq->top(idx->_pq));

    if (element == NULL) {
      break;
    }      
    
    tempResult._elements[j] = *element;
    ok = idx->_pq->remove(idx->_pq, element->pqSlot, false);

    if (!ok) {
      break;
    }

    ++numCopied;
  }

  result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndexElements), false);

  if (result == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, tempResult._elements);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  result->_elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_pq_index_element_t) * numCopied, false);

  if (result->_elements == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, tempResult._elements);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  
  result->_numElements = numCopied;
  
  for (j = 0; j < numCopied; ++j) {
    result->_elements[j] = tempResult._elements[j];
    result->_elements[j].pqSlot = 0;
    idx->_pq->add(idx->_pq, &(result->_elements[j])); 
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tempResult._elements);
    
  return result;
} 

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--             priority queue index implementation of callbacks
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup PriorityQueueIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// callbacks for the priority queue
// .............................................................................

static void ClearStoragePQIndex (TRI_pqueue_t* pq, void* item) {
  TRI_pq_index_element_t* element;
  
  element = (TRI_pq_index_element_t*)(item);

  if (element == 0) {
    return;
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
  return;
}


static uint64_t GetStoragePQIndex(TRI_pqueue_t* pq, void* item) {
  TRI_pq_index_element_t* element;
  
  element = (TRI_pq_index_element_t*)(item);
  if (element == 0) {
    return 0;
  }
  return element->pqSlot;
}


// .............................................................................
// True if the leftItem is less than the rightItem
// .............................................................................

static bool IsLessPQIndex(TRI_pqueue_t* pq, void* leftItem, void* rightItem) {
  size_t maxNumFields;
  TRI_pq_index_element_t* leftElement  = (TRI_pq_index_element_t*)(leftItem);
  TRI_pq_index_element_t* rightElement = (TRI_pq_index_element_t*)(rightItem);
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;
  int compareResult;

  if (leftElement == NULL && rightElement == NULL) {
    return false;
  }    
  
  if (leftElement == NULL && rightElement != NULL) {
    return true;
  }    
  
  if (leftElement != NULL && rightElement == NULL) {
    return false;
  }    

  if (leftElement == rightElement) {
    return false;
  }    
    
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................

  if (leftElement->_document == rightElement->_document) {
    return false;
  }    
  
  if (leftElement->numFields < rightElement->numFields) {
    maxNumFields = leftElement->numFields;
  }
  else {
    maxNumFields = rightElement->numFields;
  }
  
  leftShaper  = ((TRI_primary_collection_t*)(leftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(rightElement->collection))->_shaper;
  
  compareResult = 0;

  for (j = 0;  j < maxNumFields;  j++) {
    compareResult = TRI_CompareShapeTypes(leftElement->_document,
                                          &leftElement->_subObjects[j],
                                          NULL,
                                          rightElement->_document,
                                          &rightElement->_subObjects[j],
                                          NULL,
                                          leftShaper,
                                          rightShaper);

    if (compareResult != 0) {
      break;
    }
  }

  if (compareResult  < 0) {
    return true;
  }
  
  return false;
}


static void UpdateStoragePQIndex(TRI_pqueue_t* pq, void* item, uint64_t position) {
  TRI_pq_index_element_t* element;
  TRI_associative_array_t* aa;
  
  element = (TRI_pq_index_element_t*)(item);
  if (element == 0) {
    LOG_ERROR("invalid priority queue element received");
    return;
  }

  element->pqSlot = position;

  // ...........................................................................
  // Since the items stored in the hash array are pointers, we must update these
  // as well. The associative array is stored at the end of the Priority Queue
  // structure.
  // ...........................................................................
  aa = *((TRI_associative_array_t**)((char*)(pq) + sizeof(TRI_pqueue_t)));
  element = (TRI_pq_index_element_t*)(TRI_FindByElementAssociativeArray(aa, item));
  if (element == 0) {
    LOG_ERROR("invalid priority queue/ associative array element received");
    return;
  }
  element->pqSlot = position;
}


// .............................................................................
// callbacks for the associative array
// .............................................................................


static void ClearElementPQIndex(TRI_associative_array_t* aa, void* item) {
  if (item == NULL) {
    return;
  }
  memset(item, 0, sizeof(TRI_pq_index_element_t));
}


static uint64_t HashKeyPQIndex(TRI_associative_array_t* aa, void* key) {
  uint64_t hash;
  
  hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, key, sizeof(void*)); 
  
  return  hash;
}


static uint64_t HashElementPQIndex(TRI_associative_array_t* aa, void* item) {
  TRI_pq_index_element_t* element;
  uint64_t hash;
  
  element = (TRI_pq_index_element_t*)(item);
  if (element == 0) {
    return 0;
  }
  
  hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, (void*) element->_document, sizeof(void*)); 
  
  return  hash;
}


static bool IsEmptyElementPQIndex(TRI_associative_array_t* aa, void* item) {
  TRI_pq_index_element_t* element;
  
  if (item == NULL) {
    // should never happen
    return false;
  }  
  
  element = (TRI_pq_index_element_t*)(item);

  if (element->_document == NULL) {
    return true;
  }  

  return false;
}


static bool IsEqualElementElementPQIndex(TRI_associative_array_t* aa, void* leftItem, void* rightItem) {
  TRI_pq_index_element_t* leftElement;
  TRI_pq_index_element_t* rightElement;
  
  if (leftItem == NULL || rightItem == NULL) {
    // should never happen
    return false;
  }  
  
  leftElement  = (TRI_pq_index_element_t*)(leftItem);
  rightElement = (TRI_pq_index_element_t*)(rightItem);

  if (leftElement->_document == rightElement->_document) {
    return true;
  }  

  return false;
}


static bool IsEqualKeyElementPQIndex(TRI_associative_array_t* aa, void* key, void* item) {
  TRI_pq_index_element_t* element;
  
  if (item == NULL) {
    return false; // should never happen
  }
  element = (TRI_pq_index_element_t*)(item);  

  if (element->_document == key) {
    return true;
  }

  return false;  
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


