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

#include "pqueueindex.h"
#include "ShapedJson/shaped-json.h"
#include "ShapedJson/json-shaper.h"
#include "VocBase/primary-collection.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"

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


// comparison helpers
static int CompareShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right,
                                        TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper);
static int CompareShapeTypes (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right,
                              TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper);

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

  ok = TRI_InitPQueue(idx->_pq, 100, sizeof(PQIndexElement), false,
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
                           sizeof(PQIndexElement),
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

int PQIndex_add(PQIndex* idx, PQIndexElement* element) {

  if (idx == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  if (element == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  // ...........................................................................
  // Check if item is already added to the associative array
  // ...........................................................................

  if (TRI_FindByKeyAssociativeArray(idx->_aa, element->data) != NULL) {
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
/// @brief inserts an item into a priority queue (same as add method above)
////////////////////////////////////////////////////////////////////////////////

int PQIndex_insert(PQIndex* idx, PQIndexElement* element) {
  return PQIndex_add(idx, element);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes an item from the priority queue (not necessarily the top most)
////////////////////////////////////////////////////////////////////////////////

int PQIndex_remove(PQIndex* idx, PQIndexElement* element) {

  PQIndexElement* item;
  bool ok;

  if (idx == NULL) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED;
  }

  if (element == NULL) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED;
  }

  // ...........................................................................
  // Check if item exists in the associative array.
  // ...........................................................................

  item = TRI_FindByKeyAssociativeArray(idx->_aa, element->data);
  if (item == NULL) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_ITEM_MISSING;
  }


  // ...........................................................................
  // Remove item from the priority queue
  // ...........................................................................

  ok = idx->_pq->remove(idx->_pq,item->pqSlot, true);


  // ...........................................................................
  // Remove item from associative array
  // Must come after remove above, since update storage will be called.
  // ...........................................................................

  ok = TRI_RemoveElementAssociativeArray(idx->_aa,item,NULL) && ok;

  if (!ok) {
    return TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED;
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the top most item without removing it from the queue
////////////////////////////////////////////////////////////////////////////////

PQIndexElements* PQIndex_top(PQIndex* idx, uint64_t numElements) {

  PQIndexElements* result;
  PQIndexElements tempResult;
  PQIndexElement* element;
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
    result->_elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndexElement) * numElements, false);
    if (result->_elements == NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
    result->_numElements = numElements;
    result->_elements[0] = *((PQIndexElement*)(idx->_pq->top(idx->_pq)));
    return result;
  }



  // .............................................................................
  // Two or more elements are 'topped'
  // .............................................................................

  tempResult._elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndexElement) * numElements, false);
  if (tempResult._elements == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  ok = true;
  numCopied = 0;
  for (j = 0; j < numElements; ++j) {
    element = (PQIndexElement*)(idx->_pq->top(idx->_pq));
    if (element == NULL) {
      break;
    }


    tempResult._elements[j] = *element;
    ok = idx->_pq->remove(idx->_pq,element->pqSlot, false);
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

  result->_elements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(PQIndexElement) * numCopied, false);
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
/// @brief removes an item and inserts a new item
////////////////////////////////////////////////////////////////////////////////

bool PQIndex_update(PQIndex* idx, const PQIndexElement* oldElement, const PQIndexElement* newElement) {
  assert(false);
  return false;
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


static void ClearStoragePQIndex(TRI_pqueue_t* pq, void* item) {
  PQIndexElement* element;

  element = (PQIndexElement*)(item);
  if (element == 0) {
    return;
  }
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->fields);
  return;
}


static uint64_t GetStoragePQIndex(TRI_pqueue_t* pq, void* item) {
  PQIndexElement* element;

  element = (PQIndexElement*)(item);
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
  PQIndexElement* leftElement  = (PQIndexElement*)(leftItem);
  PQIndexElement* rightElement = (PQIndexElement*)(rightItem);
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
  if (leftElement->data == rightElement->data) {
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

  leftShaper->lookupShapeId(leftShaper, (leftElement->fields)->_sid);

  for (j = 0; j < maxNumFields; j++) {
    /*
    printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
      *((double*)((j + leftElement->fields)->_data.data)),
      *((double*)((j + rightElement->fields)->_data.data)),
      (uint64_t)(leftElement->data),
      (uint64_t)(rightElement->data)
    );
    */
    compareResult = CompareShapedJsonShapedJson((j + leftElement->fields),
                                                (j + rightElement->fields),
                                                leftShaper, rightShaper);
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
  PQIndexElement* element;
  TRI_associative_array_t* aa;

  element = (PQIndexElement*)(item);
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
  element = (PQIndexElement*)(TRI_FindByElementAssociativeArray(aa, item));
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
  memset(item, 0, sizeof(PQIndexElement));
}


static uint64_t HashKeyPQIndex(TRI_associative_array_t* aa, void* key) {
  uint64_t hash;

  hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, key, sizeof(void*));

  return  hash;
}


static uint64_t HashElementPQIndex(TRI_associative_array_t* aa, void* item) {
  PQIndexElement* element;
  uint64_t hash;

  element = (PQIndexElement*)(item);
  if (element == 0) {
    return 0;
  }

  hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, element->data, sizeof(void*));

  return  hash;
}


static bool IsEmptyElementPQIndex(TRI_associative_array_t* aa, void* item) {
  PQIndexElement* element;

  if (item == NULL) {
    // should never happen
    return false;
  }

  element = (PQIndexElement*)(item);
  if (element->data == NULL) {
    return true;
  }
  return false;
}


static bool IsEqualElementElementPQIndex(TRI_associative_array_t* aa, void* leftItem, void* rightItem) {
  PQIndexElement* leftElement;
  PQIndexElement* rightElement;

  if (leftItem == NULL || rightItem == NULL) {
    // should never happen
    return false;
  }

  leftElement  = (PQIndexElement*)(leftItem);
  rightElement = (PQIndexElement*)(rightItem);
  if (leftElement->data == rightElement->data) {
    return true;
  }
  return false;
}


static bool IsEqualKeyElementPQIndex(TRI_associative_array_t* aa, void* key, void* item) {
  PQIndexElement* element;

  if (item == NULL) {
    return false; // should never happen
  }
  element = (PQIndexElement*)(item);
  if (element->data == key) {
    return true;
  }
  return false;
}


// .............................................................................
// implementation of compare functions
// .............................................................................

static int CompareShapeTypes (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right, TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper) {

  int result;
  size_t j;
  TRI_shape_type_t leftType;
  TRI_shape_type_t rightType;
  const TRI_shape_t* leftShape;
  const TRI_shape_t* rightShape;
  size_t leftListLength;
  size_t rightListLength;
  size_t listLength;
  TRI_shaped_json_t leftElement;
  TRI_shaped_json_t rightElement;
  char* leftString;
  char* rightString;


  leftShape  = leftShaper->lookupShapeId(leftShaper, left->_sid);
  rightShape = rightShaper->lookupShapeId(rightShaper, right->_sid);
  leftType   = leftShape->_type;
  rightType  = rightShape->_type;

  switch (leftType) {

    case TRI_SHAPE_ILLEGAL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        {
          return 0;
        }
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_NULL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        {
          return 1;
        }
        case TRI_SHAPE_NULL:
        {
          return 0;
        }
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_BOOLEAN: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        {
          return 1;
        }
        case TRI_SHAPE_BOOLEAN:
        {
          // check which is false and which is true!
          if ( *((TRI_shape_boolean_t*)(left->_data.data)) == *((TRI_shape_boolean_t*)(right->_data.data)) ) {
            return 0;
          }
          if ( *((TRI_shape_boolean_t*)(left->_data.data)) < *((TRI_shape_boolean_t*)(right->_data.data)) ) {
            return -1;
          }
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_NUMBER: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        {
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        {
          // compare the numbers.
          if ( *((TRI_shape_number_t*)(left->_data.data)) == *((TRI_shape_number_t*)(right->_data.data)) ) {
            return 0;
          }
          if ( *((TRI_shape_number_t*)(left->_data.data)) < *((TRI_shape_number_t*)(right->_data.data)) ) {
            return -1;
          }
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_SHORT_STRING:
    case TRI_SHAPE_LONG_STRING:
    {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        {
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        {
          // compare strings
          // extract the strings
          if (leftType == TRI_SHAPE_SHORT_STRING) {
            leftString = (char*)(sizeof(TRI_shape_length_short_string_t) + left->_data.data);
          }
          else {
            leftString = (char*)(sizeof(TRI_shape_length_long_string_t) + left->_data.data);
          }

          if (rightType == TRI_SHAPE_SHORT_STRING) {
            rightString = (char*)(sizeof(TRI_shape_length_short_string_t) + right->_data.data);
          }
          else {
            rightString = (char*)(sizeof(TRI_shape_length_long_string_t) + right->_data.data);
          }

          result = strcmp(leftString,rightString);
          return result;
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_HOMOGENEOUS_LIST:
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
    case TRI_SHAPE_LIST:
    {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        {
          return 1;
        }
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        case TRI_SHAPE_LIST:
        {
          // unfortunately recursion: check the types of all the entries
          leftListLength  = *((TRI_shape_length_list_t*)(left->_data.data));
          rightListLength = *((TRI_shape_length_list_t*)(right->_data.data));

          // determine the smallest list
          if (leftListLength > rightListLength) {
            listLength = rightListLength;
          }
          else {
            listLength = leftListLength;
          }

          for (j = 0; j < listLength; ++j) {

            if (leftType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(leftShape),
                                              left,j,&leftElement);
            }
            else if (leftType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(leftShape),
                                                   left,j,&leftElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(leftShape),left,j,&leftElement);
            }


            if (rightType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(rightShape),
                                              right,j,&rightElement);
            }
            else if (rightType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(rightShape),
                                                   right,j,&rightElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(rightShape),right,j,&rightElement);
            }

            result = CompareShapeTypes (&leftElement, &rightElement, leftShaper, rightShaper);
            if (result != 0) {
              return result;
            }
          }

          // up to listLength everything matches
          if (leftListLength < rightListLength) {
            return -1;
          }
          else if (leftListLength > rightListLength) {
            return 1;
          }
          return 0;
        }


        case TRI_SHAPE_ARRAY:
        {
          return -1;
        }
      } // end of switch (rightType)
    }

    case TRI_SHAPE_ARRAY:
    {
      assert(false);
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL:
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST:
        case TRI_SHAPE_LIST:
        {
          return 1;
        }
        case TRI_SHAPE_ARRAY:
        {
          assert(false);
          result = 0;
          return result;
        }
      } // end of switch (rightType)
    }

  }
  assert(false);
  return 0; // shut the vc++ up
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Compare a shapded json object recursively if necessary
////////////////////////////////////////////////////////////////////////////////

static int CompareShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right, TRI_shaper_t* leftShaper, TRI_shaper_t* rightShaper) {

  int result;

  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicorgraphically and within each slot according to these rules.
  // ............................................................................


  if (left == NULL && right == NULL) {
    return 0;
  }

  if (left == NULL && right != NULL) {
    return -1;
  }

  if (left != NULL && right == NULL) {
    return 1;
  }

  result = CompareShapeTypes (left, right, leftShaper, rightShaper);

  return result;

}  // end of function CompareShapedJsonShapedJson


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:


