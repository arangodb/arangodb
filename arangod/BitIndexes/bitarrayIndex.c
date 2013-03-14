////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray index
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

#include "bitarrayIndex.h"

#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BitIndexes/bitarray.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/index.h"
#include "VocBase/primary-collection.h"

// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

static void  BitarrayIndexDestroyIterator          (TRI_index_iterator_t*);
static bool  BitarrayIndexHasNextIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexNextIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexNextsIterationCallback   (TRI_index_iterator_t*, int64_t);
static bool  BitarrayIndexHasPrevIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexPrevIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexPrevsIterationCallback   (TRI_index_iterator_t*, int64_t);
static void  BitarrayIndexResetIterator            (TRI_index_iterator_t*, bool);


// .............................................................................
// forward declaration of static functions used for bit mask callbacks
// .............................................................................

static void  BitarrayIndex_destroyMaskSet             (TRI_bitarray_mask_set_t*);
static void  BitarrayIndex_extendMaskSet              (TRI_bitarray_mask_set_t*, const int, const double);
static int   BitarrayIndex_findHelper                 (BitarrayIndex*, TRI_vector_t*, TRI_index_operator_t*, TRI_index_iterator_t*, TRI_bitarray_mask_set_t*);
#if 0
static void  BitarrayIndex_freeMaskSet                (TRI_bitarray_mask_set_t*);
#endif
static int   BitarrayIndex_generateInsertBitMask      (BitarrayIndex*, const TRI_bitarray_index_key_t*, TRI_bitarray_mask_t*);
static int   BitarrayIndex_generateEqualBitMask       (BitarrayIndex*, const TRI_relation_index_operator_t*, TRI_bitarray_mask_t*);
static int   BitarrayIndex_generateEqualBitMaskHelper (TRI_json_t*, TRI_json_t*, uint64_t*);
static void  BitarrayIndex_insertMaskSet              (TRI_bitarray_mask_set_t*, TRI_bitarray_mask_t*, bool);


// .............................................................................
// forward declaration of static functions which are used by the query engine
// .............................................................................

static int                   BitarrayIndex_queryMethodCall  (void*, TRI_index_operator_t*, TRI_index_challenge_t*, void*);
static TRI_index_iterator_t* BitarrayIndex_resultMethodCall (void*, TRI_index_operator_t*, void*, bool (*filter) (TRI_index_iterator_t*));
static int                   BitarrayIndex_freeMethodCall   (void*, void*);


// .............................................................................
// forward declaration of static functions used for debugging callbacks
// comment out to suppress compile warnings -- todo add preprocessor directives
// .............................................................................

/*
static void BitarrayIndex_debugPrintMaskFooter     (const char*);
static void BitarrayIndex_debugPrintMaskHeader     (const char*);
static void BitarrayIndex_debugPrintMask           (BitarrayIndex*, uint64_t);
*/


// -----------------------------------------------------------------------------
// --SECTION--                           bitarrayIndex     common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarrayIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_destroy(BitarrayIndex* baIndex) {
  size_t j;
  if (baIndex == NULL) {
    return;
  }
  for (j = 0;  j < baIndex->_values._length;  ++j) {
    TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j)));
  }
  TRI_DestroyVector(&baIndex->_values);

  TRI_FreeBitarray(baIndex->_bitarray);
  baIndex->_bitarray = NULL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_free(BitarrayIndex* baIndex) {
  if (baIndex == NULL) {
    return;
  }
  BitarrayIndex_destroy(baIndex);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, baIndex);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



//------------------------------------------------------------------------------
// Public Methods
//------------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Assigns a static function call to a function pointer used by Query Engine
////////////////////////////////////////////////////////////////////////////////

int BittarrayIndex_assignMethod(void* methodHandle, TRI_index_method_assignment_type_e methodType) {
  switch (methodType) {

    case TRI_INDEX_METHOD_ASSIGNMENT_FREE : {
      TRI_index_query_free_method_call_t* call = (TRI_index_query_free_method_call_t*)(methodHandle);
      *call = BitarrayIndex_freeMethodCall;
      break;
    }

    case TRI_INDEX_METHOD_ASSIGNMENT_QUERY : {
      TRI_index_query_method_call_t* call = (TRI_index_query_method_call_t*)(methodHandle);
      *call = BitarrayIndex_queryMethodCall;
      break;
    }

    case TRI_INDEX_METHOD_ASSIGNMENT_RESULT : {
      TRI_index_query_result_method_call_t* call = (TRI_index_query_result_method_call_t*)(methodHandle);
      *call = BitarrayIndex_resultMethodCall;
      break;
    }

    default : {
      assert(false);
    }
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new bitarray index. Failure will return an appropriate error code
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_new(BitarrayIndex** baIndex,
                      TRI_memory_zone_t* memoryZone,
                      size_t cardinality,
                      TRI_vector_t* values,
                      bool supportUndef,
                      void* context) {
  int     result;
  size_t  numArrays;
  int j;


  // ...........................................................................
  // Some simple checks
  // ...........................................................................

  if (baIndex == NULL) {
    assert(false);
    return TRI_ERROR_INTERNAL;
  }



  // ...........................................................................
  // If the bit array index has arealdy been created, return internal error
  // ...........................................................................

  if (*baIndex != NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // If the memory zone is invalid, then return an internal error
  // ...........................................................................

  if (memoryZone == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // Create the bit array index structure
  // ...........................................................................

  *baIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(BitarrayIndex), true);
  if (*baIndex == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }


  // ...........................................................................
  // Copy the values into this index
  // ...........................................................................


  TRI_InitVector(&((*baIndex)->_values), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_json_t));

  for (j = 0;  j < values->_length;  ++j) {
    TRI_json_t value;
    TRI_CopyToJson(TRI_UNKNOWN_MEM_ZONE, &value, (TRI_json_t*)(TRI_AtVector(values,j)));
    TRI_PushBackVector(&((*baIndex)->_values), &value);
  }


  // ...........................................................................
  // Store whether or not the index supports 'undefined' documents (that is
  // documents with attributes which do not match those of the index
  // ...........................................................................

  (*baIndex)->_supportUndef = supportUndef;

  // ...........................................................................
  // Determine the number of bit columns which will comprise the bit array index.
  // ...........................................................................

  numArrays = cardinality;

  // ...........................................................................
  // Create the bit arrays
  // ...........................................................................

  result = TRI_InitBitarray(&((*baIndex)->_bitarray), memoryZone, numArrays, NULL);


  // ...........................................................................
  // return the result of creating the bit  arrays
  // ...........................................................................

  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into one or more bit arrays
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_insert (BitarrayIndex* baIndex, TRI_bitarray_index_key_t* element) {
  int result;
  TRI_bitarray_mask_t mask;

  // ..........................................................................
  // At the current time we have no way in which to store undefined documents
  // need some sort of parameter passed here.
  // ..........................................................................

  // ..........................................................................
  // generate bit mask -- initialise first
  // ..........................................................................

  mask._mask       = 0;
  mask._ignoreMask = 0;

  result = BitarrayIndex_generateInsertBitMask(baIndex, element, &mask);
  //debugPrintMask(baIndex, mask._mask);

  // ..........................................................................
  // It may happen that the values for one or more attributes are unsupported
  // in this an error will be returned.
  // ..........................................................................

  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }

  // ..........................................................................
  // insert the bit mask into the bit array
  // ..........................................................................

  result = TRI_InsertBitMaskElementBitarray(baIndex->_bitarray, &mask, element->data);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate one or more documents which match an index operator
////////////////////////////////////////////////////////////////////////////////

TRI_index_iterator_t* BitarrayIndex_find (BitarrayIndex* baIndex,
                                          TRI_index_operator_t* indexOperator,
                                          TRI_vector_t* shapeList,
                                          TRI_bitarray_index_t* collectionIndex,
                                          bool (*filter) (TRI_index_iterator_t*) ) {

  TRI_index_iterator_t* iterator;
  TRI_bitarray_mask_set_t maskSet;
  int result;

  // ...........................................................................
  // Attempt to allocate memory for the index iterator which stores the results
  // if any of the lookup.
  // ...........................................................................

  iterator = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_t), true);
  if (iterator == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL; // calling procedure needs to care when the iterator is null
  }


  // ...........................................................................
  // We now initialise the index iterator with all the call back functions.
  // ...........................................................................

  TRI_InitVector(&(iterator->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_interval_t));

  iterator->_index           = collectionIndex;
  iterator->_currentInterval = 0;
  iterator->_cursor          = NULL;

  iterator->_filter          = filter;
  iterator->_hasNext         = BitarrayIndexHasNextIterationCallback;
  iterator->_next            = BitarrayIndexNextIterationCallback;
  iterator->_nexts           = BitarrayIndexNextsIterationCallback;
  iterator->_hasPrev         = BitarrayIndexHasPrevIterationCallback;
  iterator->_prev            = BitarrayIndexPrevIterationCallback;
  iterator->_prevs           = BitarrayIndexPrevsIterationCallback;
  iterator->_destroyIterator = BitarrayIndexDestroyIterator;
  iterator->_reset           = BitarrayIndexResetIterator;

  // ...........................................................................
  // We now initialise the mask set and set the array to some reasonable size
  // ...........................................................................

  maskSet._setSize   = 0;
  maskSet._arraySize = 0;
  maskSet._maskArray = 0;
  BitarrayIndex_extendMaskSet(&maskSet, 20, 0.0);

  result = BitarrayIndex_findHelper(baIndex, shapeList, indexOperator, iterator, &maskSet);

  if (result == TRI_ERROR_NO_ERROR) {
    //BitarrayIndex_debugPrintMaskHeader("lookup");
    //BitarrayIndex_debugPrintMask(baIndex,(maskSet._maskArray)->_mask);
    //BitarrayIndex_debugPrintMask(baIndex,(maskSet._maskArray)->_ignoreMask);
    //BitarrayIndex_debugPrintMaskFooter("");
    result = TRI_LookupBitMaskSetBitarray(baIndex->_bitarray, &maskSet, iterator);
  }

  BitarrayIndex_destroyMaskSet(&maskSet);

  if (result != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  return iterator;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the bit arrays and master table
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_remove(BitarrayIndex* baIndex, TRI_bitarray_index_key_t* element) {
  int result;
  result = TRI_RemoveElementBitarray(baIndex->_bitarray, element->data);
  return result;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Implementation of static functions forward declared above
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

void BitarrayIndexDestroyIterator(TRI_index_iterator_t* iterator) {
  TRI_DestroyVector(&(iterator->_intervals));
}


bool  BitarrayIndexHasNextIterationCallback(TRI_index_iterator_t* iterator) {

  if (iterator->_intervals._length == 0) {
    return false;
  }

  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return false;
  }

  return true;
}


void* BitarrayIndexNextIterationCallback(TRI_index_iterator_t* iterator) {
  TRI_index_iterator_interval_t* interval;

  iterator->_currentDocument = NULL;

  if (iterator->_cursor == NULL) {
    iterator->_currentInterval = 0;
  }


  if (iterator->_intervals._length == 0) {
    return NULL;
  }


  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return NULL;
  }


  interval = (TRI_index_iterator_interval_t*)(TRI_AtVector(&(iterator->_intervals),iterator->_currentInterval));

  if (interval == NULL) { // should not occur -- something is wrong
    LOG_WARNING("internal error in BitarrayIndexNextIterationCallback");
    return NULL;
  }

  iterator->_cursor          = interval->_leftEndPoint;
  iterator->_currentDocument = interval->_leftEndPoint;

  ++iterator->_currentInterval;
  return iterator->_currentDocument;
}


void* BitarrayIndexNextsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  int64_t j;
  void* result = NULL;
  void* lastValidResult = NULL;

  for (j = 0; j < jumpSize; ++j) {
    result = BitarrayIndexNextIterationCallback(iterator);
    if (result != NULL) {
      lastValidResult = result;
    }
    if (result == NULL) {
      break;
    }
  }
  return lastValidResult;
}


bool BitarrayIndexHasPrevIterationCallback(TRI_index_iterator_t* iterator) {

  if (iterator->_intervals._length == 0) {
    return false;
  }

  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return false;
  }

  return true;
}


void* BitarrayIndexPrevIterationCallback(TRI_index_iterator_t* iterator) {
  TRI_index_iterator_interval_t* interval;

  iterator->_currentDocument = NULL;

  if (iterator->_cursor == NULL) {
    iterator->_currentInterval = iterator->_intervals._length - 1;
  }

  if (iterator->_currentInterval >= iterator->_intervals._length) {
    return NULL;
  }

  if (iterator->_intervals._length == 0) {
    return NULL;
  }


  interval = (TRI_index_iterator_interval_t*)(TRI_AtVector(&(iterator->_intervals),iterator->_currentInterval));

  if (interval == NULL) { // should not occur -- something is wrong
    LOG_WARNING("internal error in BitarrayIndexPrevIterationCallback");
    return NULL;
  }

  iterator->_cursor          = interval->_leftEndPoint;
  iterator->_currentDocument = interval->_leftEndPoint;

  if (iterator->_currentInterval == 0) {
    iterator->_currentInterval = iterator->_intervals._length;
  }
  else {
    --iterator->_currentInterval;
  }
  return iterator->_currentDocument;
}


void* BitarrayIndexPrevsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  int64_t j;
  void* result = NULL;
  void* lastValidResult = NULL;

  for (j = 0; j < jumpSize; ++j) {
    result = BitarrayIndexPrevIterationCallback(iterator);
    if (result != NULL) {
      lastValidResult = result;
    }
    if (result == NULL) {
      break;
    }
  }
  return lastValidResult;
}


void BitarrayIndexResetIterator(TRI_index_iterator_t* iterator, bool beginning) {
  if (beginning) {
    iterator->_cursor = NULL;
    iterator->_currentInterval = 0;
    iterator->_currentDocument = NULL;
    return;
  }

  iterator->_cursor = NULL;
  iterator->_currentInterval = 0;
  iterator->_currentDocument = NULL;
  if (iterator->_intervals._length > 0) {
    iterator->_currentInterval = iterator->_intervals._length - 1;
  }
}


// .............................................................................
// forward declaration of static functions used internally here
// .............................................................................

int BitarrayIndex_findHelper(BitarrayIndex* baIndex,
                             TRI_vector_t* shapeList,
                             TRI_index_operator_t* indexOperator,
                             TRI_index_iterator_t* iterator,
                             TRI_bitarray_mask_set_t* maskSet) {

  int result;

  // ............................................................................
  // Process the indexOperator recursively
  // ............................................................................

  switch (indexOperator->_type) {

    case TRI_AND_INDEX_OPERATOR: {
      int j;
      TRI_bitarray_mask_set_t leftMaskSet;
      TRI_bitarray_mask_set_t rightMaskSet;
      TRI_logical_index_operator_t* logicalOperator = (TRI_logical_index_operator_t*)(indexOperator);
      TRI_index_operator_t* leftOp                  = logicalOperator->_left;
      TRI_index_operator_t* rightOp                 = logicalOperator->_right;

      leftMaskSet._setSize   = 0;
      leftMaskSet._arraySize = 0;
      leftMaskSet._maskArray = 0;
      BitarrayIndex_extendMaskSet(&leftMaskSet, 20, 0.0);
      rightMaskSet._setSize   = 0;
      rightMaskSet._arraySize = 0;
      rightMaskSet._maskArray = 0;
      BitarrayIndex_extendMaskSet(&rightMaskSet, 20, 0.0);


      // ........................................................................
      // For 'AND' operator, we simply take the intersection of the masks generated
      // ........................................................................

      result = BitarrayIndex_findHelper(baIndex, shapeList, leftOp, iterator, &leftMaskSet);
      if (result != TRI_ERROR_NO_ERROR) {
        BitarrayIndex_destroyMaskSet(&leftMaskSet);
        return result;
      }

      result = BitarrayIndex_findHelper(baIndex, shapeList, rightOp, iterator, &rightMaskSet);
      if (result != TRI_ERROR_NO_ERROR) {
        BitarrayIndex_destroyMaskSet(&rightMaskSet);
        return result;
      }

      for (j = 0; j < leftMaskSet._setSize; ++j) {
        int k;
        TRI_bitarray_mask_t* leftMask = leftMaskSet._maskArray + j;
        TRI_bitarray_mask_t andMask;
        leftMask->_mask = leftMask->_mask | leftMask->_ignoreMask;
        for (k = 0; k < rightMaskSet._setSize; ++k) {
          TRI_bitarray_mask_t* rightMask = rightMaskSet._maskArray + k;
          rightMask->_mask = rightMask->_mask | rightMask->_ignoreMask;
          andMask._mask = leftMask->_mask & rightMask->_mask;
          andMask._ignoreMask = 0;
          BitarrayIndex_insertMaskSet(maskSet,&andMask, true);
        }
      }

      break;
    }


    case TRI_OR_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* logicalOperator = (TRI_logical_index_operator_t*)(indexOperator);
      TRI_index_operator_t* leftOp                  = logicalOperator->_left;
      TRI_index_operator_t* rightOp                 = logicalOperator->_right;


      // ........................................................................
      // For 'OR' operator, we simply take the union of the masks generated
      // ........................................................................

      result = BitarrayIndex_findHelper(baIndex, shapeList, leftOp, iterator, maskSet);
      if (result != TRI_ERROR_NO_ERROR) {
        return result;
      }


      result = BitarrayIndex_findHelper(baIndex, shapeList, rightOp, iterator, maskSet);
      if (result != TRI_ERROR_NO_ERROR) {
        return result;
      }

      break;
    }


    case TRI_EQ_INDEX_OPERATOR: {
      TRI_bitarray_mask_t mask;
      TRI_relation_index_operator_t* relationOperator = (TRI_relation_index_operator_t*)(indexOperator);

      mask._mask       = 0;
      mask._ignoreMask = 0;

      // ............................................................................
      // for bitarray indexes, the number of attribute values ALWAYS matches the
      // the number of parameters for a TRI_EQ_INDEX_OPERATOR. However, the client
      // may wish some attributes to be ignored, so some values will be '{}'.
      // ............................................................................

      if (relationOperator->_numFields != shapeList->_length) {
        assert(false);
      }


      // ............................................................................
      // generate the bit mask
      // ............................................................................

      result = BitarrayIndex_generateEqualBitMask(baIndex, relationOperator, &mask);
      //BitarrayIndex_debugPrintMask(baIndex, mask._mask);
      //BitarrayIndex_debugPrintMask(baIndex, mask._ignoreMask);

      // .......................................................................
      // for now return this error
      // .......................................................................

      if (result == TRI_ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE) {
        return result;
      }

      // .......................................................................
      // Append the mask generated here to the mask set
      // .......................................................................

      BitarrayIndex_insertMaskSet(maskSet,&mask, true);
      //BitarrayIndex_debugPrintMaskHeader("lookup");
      //BitarrayIndex_debugPrintMask(baIndex,mask._mask);
      //BitarrayIndex_debugPrintMask(baIndex,mask._ignoreMask);
      //BitarrayIndex_debugPrintMaskFooter("");

      break;
    }


    case TRI_NE_INDEX_OPERATOR: {
      // todo
      result = TRI_ERROR_INTERNAL;
      assert(false);
      break;
    }


    case TRI_LE_INDEX_OPERATOR: {
      // todo -- essentially (since finite number) take the union
      result = TRI_ERROR_INTERNAL;
      assert(false);
      break;
    }


    case TRI_LT_INDEX_OPERATOR: {
      // todo
      result = TRI_ERROR_INTERNAL;
      assert(false);
      break;
    }


    case TRI_GE_INDEX_OPERATOR: {
      // todo
      result = TRI_ERROR_INTERNAL;
      assert(false);
      break;
    }


    case TRI_GT_INDEX_OPERATOR: {
      // todo
      result = TRI_ERROR_INTERNAL;
      assert(false);
      break;
    }

    default: {
      result = TRI_ERROR_INTERNAL;
      assert(0);
    }

  } // end of switch statement


  return result;
}



////////////////////////////////////////////////////////////////////////////////
// Given the index structure and the list of shaped json values which came from
// some document we generate a bit mask.
////////////////////////////////////////////////////////////////////////////////

static bool isEqualJson(TRI_json_t* left, TRI_json_t* right) {

  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL || right == NULL) {
    return false;
  }

  if (left->_type != right->_type) {
    return false;
  }

  switch (left->_type) {

    case TRI_JSON_UNUSED: {
      return true;
    }

    case TRI_JSON_NULL: {
      return true;
    }

    case TRI_JSON_BOOLEAN: {
      return (left->_value._boolean == right->_value._boolean);
    }

    case TRI_JSON_NUMBER: {
      return (left->_value._number == right->_value._number);
    }

    case TRI_JSON_STRING: {
      return (strcmp(left->_value._string.data, right->_value._string.data) == 0);
    }

    case TRI_JSON_ARRAY: {
      int j;

      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }

      for (j = 0; j < (left->_value._objects._length / 2); ++j) {
        TRI_json_t* leftName;
        TRI_json_t* leftValue;
        TRI_json_t* rightValue;

        leftName = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),2*j));
        if (leftName == NULL) {
          return false;
        }

        leftValue  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),(2*j) + 1));
        rightValue = TRI_LookupArrayJson(right, leftName->_value._string.data);

        if (isEqualJson(leftValue, rightValue)) {
          continue;
        }
        return false;
      }

      return true;
    }

    case TRI_JSON_LIST: {
      int j;

      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }

      for (j = 0; j < left->_value._objects._length; ++j) {
        TRI_json_t* subLeft;
        TRI_json_t* subRight;
        subLeft  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j));
        subRight = (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j));
        if (isEqualJson(subLeft, subRight)) {
          continue;
        }
        return false;
      }

      return true;
    }

    default: {
      assert(false);
    }
  }

  return false; // shut the vc++ up
}



////////////////////////////////////////////////////////////////////////////////
// Implementation of static functions used for bit mask callbacks
////////////////////////////////////////////////////////////////////////////////

static void  BitarrayIndex_destroyMaskSet(TRI_bitarray_mask_set_t* maskSet) {
  if (maskSet != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, maskSet->_maskArray);
  }
}


static void  BitarrayIndex_extendMaskSet(TRI_bitarray_mask_set_t* maskSet, const int nSize, const double increaseBy) {
  size_t newSize;
  TRI_bitarray_mask_t* newArray;

  if (nSize <= 0) {
    newSize = (increaseBy * maskSet->_arraySize) + 1;
  }
  else {
    newSize = nSize;
  }

  newArray = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_bitarray_mask_t) * newSize, true);

  if (newArray == NULL) {
    // todo report memory failure
    return;
  }

  if (maskSet->_maskArray == 0 || maskSet->_arraySize == 0) {
    maskSet->_maskArray = newArray;
    maskSet->_arraySize = newSize;
    maskSet->_setSize   = 0;
    return;
  }

  memcpy(newArray, maskSet->_maskArray, sizeof(TRI_bitarray_mask_t) * maskSet->_setSize);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, maskSet->_maskArray);
  maskSet->_maskArray = newArray;
  maskSet->_arraySize = newSize;
}

#if 0
static void  BitarrayIndex_freeMaskSet(TRI_bitarray_mask_set_t* maskSet) {
  BitarrayIndex_destroyMaskSet(maskSet);
  if (maskSet != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, maskSet);
  }
}
#endif

static int BitarrayIndex_generateEqualBitMaskHelper(TRI_json_t* valueList, TRI_json_t* value, uint64_t* mask) {
  int i;
  uint64_t other = 0;
  uint64_t tempMask = 0;


  // .............................................................................
  // valueList is a list of possible values for a given attribute.
  // value is the attribute value to determine if it exists within the list of
  // allowed values.
  // .............................................................................

  for (i = 0; i < valueList->_value._objects._length; ++i) {
    int k;
    TRI_json_t* listEntry = (TRI_json_t*)(TRI_AtVector(&(valueList->_value._objects), i));

    // ...........................................................................
    // if the ith possible set of values is not a list, do comparison
    // ...........................................................................

    if (listEntry->_type != TRI_JSON_LIST) {
      if (isEqualJson(value, listEntry)) {
        tempMask = tempMask | (1 << i);
      }
      continue; // there may be further matches!
    }


    // ...........................................................................
    // ith entry in the set of possible values is a list
    // ...........................................................................

    // ...........................................................................
    // Special case of an empty list -- this means all other values
    // ...........................................................................

    if (listEntry->_value._objects._length == 0) { // special case determine what the bit position of other is
      other = (1 << i);
      continue; // there may be further matches!
    }


    for (k = 0; k < listEntry->_value._objects._length; k++) {
      TRI_json_t* subListEntry;
      subListEntry = (TRI_json_t*)(TRI_AtVector(&(listEntry->_value._objects), k));
      if (isEqualJson(value, subListEntry)) {
        tempMask = tempMask | (1 << i);
        break;
      }
    }

  }

  if (tempMask != 0) {
    *mask = *mask | tempMask;
    return TRI_ERROR_NO_ERROR;
  }

  if (other != 0) {
    *mask = *mask | other;
    return TRI_ERROR_NO_ERROR;
  }


  // ...........................................................................
  // Allow this as an option when Bitarray index is created.
  // ...........................................................................

  return TRI_ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE;


}


int BitarrayIndex_generateInsertBitMask (BitarrayIndex* baIndex,
                                         const TRI_bitarray_index_key_t* element,
                                         TRI_bitarray_mask_t* mask) {

  TRI_shaper_t* shaper;
  int j;
  int shiftLeft;
  int result;

  // ...........................................................................
  // some safety checks first
  // ...........................................................................

  if (baIndex == NULL || element == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  if (element->collection == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  // ...........................................................................
  // We could be trying to store an 'undefined' document into the bitarray
  // We determine this implicitly. If element->numFields b == 0, then we
  // assume that the document did not have any matching attributes, yet since
  // we are here we wish to store this fact.
  // ...........................................................................

  if (!baIndex->_supportUndef && (element->numFields == 0 || element->fields == NULL)) {
    return TRI_ERROR_INTERNAL;
  }


  if (baIndex->_supportUndef && element->numFields == 0) {
    mask->_mask       = 1;
    mask->_ignoreMask = 0;
    return TRI_ERROR_NO_ERROR;
  }


  // ...........................................................................
  // attempt to convert the stored TRI_shaped_json_t into TRI_Json_t so that
  // we can make a comparison between what values the bitarray index requires
  // and what values the document has sent.
  // ...........................................................................

  shaper      = ((TRI_primary_collection_t*)(element->collection))->_shaper;
  mask->_mask = 0;
  shiftLeft   = 0;

  for (j = 0; j < baIndex->_values._length; ++j) {
    TRI_json_t* valueList;
    TRI_json_t* value;
    uint64_t    tempMask;

    value      = TRI_JsonShapedJson(shaper, &(element->fields[j])); // from shaped json to simple json
    valueList  = (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j));
    tempMask   = 0;


    // .........................................................................
    // value is now the shaped json converted into plain json for comparison
    // ........................................................................

    result = BitarrayIndex_generateEqualBitMaskHelper(valueList, value, &tempMask);

    // ............................................................................
    // remove the json entry created from the shaped json
    // ............................................................................

    TRI_FreeJson(shaper->_memoryZone, value);

    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }

    mask->_mask = mask->_mask | (tempMask << shiftLeft);

    shiftLeft += valueList->_value._objects._length;
  }

  return TRI_ERROR_NO_ERROR;
}



int BitarrayIndex_generateEqualBitMask(BitarrayIndex* baIndex, const TRI_relation_index_operator_t* relationOperator, TRI_bitarray_mask_t* mask) {
  int j;
  int shiftLeft;
  int ignoreShiftLeft;
  int result;

  // ...........................................................................
  // some safety checks first
  // ...........................................................................

  if (baIndex == NULL || relationOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // supportUndef -- refers to where the document does not have 1 or more
  // attributes which are defined in the index. (Not related to whether or
  // not the attribute has a value defined in the set of supported values.)
  // ...........................................................................


  // ...........................................................................
  // if the number of attributes is 0, then something must be wrong
  // ...........................................................................

  if (relationOperator->_numFields == 0) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // if an attribute which was defined in the index, was not sent by the client,
  // then that bitarray column is ignored.
  // ...........................................................................

  mask->_mask       = 0;
  mask->_ignoreMask = 0;
  shiftLeft         = 0;
  ignoreShiftLeft   = 0;

  for (j = 0; j < baIndex->_values._length; ++j) { // loop over the number of attributes defined in the index
    TRI_json_t* valueList;
    TRI_json_t* value;
    uint64_t    tempMask;


    value     = (TRI_json_t*)(TRI_AtVector(&(relationOperator->_parameters->_value._objects), j));
    valueList = (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j));


    ignoreShiftLeft += valueList->_value._objects._length;

    // .........................................................................
    // client did not send us this attribute (hence undefined value), therefore
    // this particular column we ignore
    // .........................................................................

    if (value->_type == TRI_JSON_UNUSED) {
      uint64_t tempInt;
      tempMask = ~((~(uint64_t)(0)) << ignoreShiftLeft);
      tempInt  = (~(uint64_t)(0)) << (ignoreShiftLeft - valueList->_value._objects._length);
      mask->_ignoreMask = mask->_ignoreMask | (tempMask & tempInt);
      tempMask = 0;
    }


    // .........................................................................
    // the client sent us one or more values for the attribute. If client sent
    // us [a_1,a_2,...,a_n] as a value, then this is interpretated as turning
    // on those bits. Note that if the value is itself a json list, then
    // this must be sent as [[l_1,l_2,...,l_m]]
    // .........................................................................

    else {
      tempMask = 0;


      // ........................................................................
      // the value (for this jth attribute) sent by the client is NOT a list
      // ........................................................................

      if (value->_type != TRI_JSON_LIST) {
        result = BitarrayIndex_generateEqualBitMaskHelper(valueList, value, &tempMask);


        // .......................................................................
        // for now return this error
        // .......................................................................

        if (result == TRI_ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE) {
          return result;
        }

      }


      // ........................................................................
      // the value (for this jth attribute) sent by the client IS  a list
      // we now have to loop through all the entries in this list
      // ........................................................................

      else {
        int i;
        for (i = 0; i < value->_value._objects._length; ++i) {
          TRI_json_t* listEntry = (TRI_json_t*)(TRI_AtVector(&(value->_value._objects), i));
          result = BitarrayIndex_generateEqualBitMaskHelper(valueList, listEntry, &tempMask);
          if (result == TRI_ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE) {
            return result;
          }
        }
      }

    }

    // ............................................................................
    // When we create a bitarray index, for example: ensureBitarray("x",[0,[],1,2,3])
    // and we insert doc with {"x" : "hello world"}, then, since the value of "x"
    // does not match 0,1,2 or 3 and [] appears as a valid list item, the doc is
    // inserted with a mask of 01000
    // This is what other means below
    // ............................................................................

    mask->_mask = mask->_mask | (tempMask << shiftLeft);

    shiftLeft += valueList->_value._objects._length;
  }

  // ................................................................................
  // check whether we actually ignore everything!
  // ................................................................................

  if (mask->_mask == 0 && !baIndex->_supportUndef) {
    return TRI_ERROR_INTERNAL;
  }

  //debugPrintMaskHeader("lookup mask/ignore mask");
  //debugPrintMask(baIndex,mask->_mask);
  //debugPrintMask(baIndex,mask->_ignoreMask);
  //debugPrintMaskFooter("");

  return TRI_ERROR_NO_ERROR;
}


static void  BitarrayIndex_insertMaskSet(TRI_bitarray_mask_set_t* maskSet, TRI_bitarray_mask_t* mask, bool checkForDuplicate) {
  if (checkForDuplicate) {
    int j;
    for (j = 0; j < maskSet->_setSize; ++j) {
      TRI_bitarray_mask_t* inMask = maskSet->_maskArray + j;
      if (inMask->_mask == mask->_mask && inMask->_ignoreMask == mask->_ignoreMask) {
        return;
      }
    }
  }

  if (maskSet->_setSize == maskSet->_arraySize) {
    BitarrayIndex_extendMaskSet(maskSet, -1, 1.2);
  }

  memcpy((maskSet->_maskArray + maskSet->_setSize), mask, sizeof(TRI_bitarray_mask_t));
  ++maskSet->_setSize;
}



////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared query engine callback functions
////////////////////////////////////////////////////////////////////////////////

static int BitarrayIndex_queryMethodCall(void* theIndex, TRI_index_operator_t* indexOperator,
                                         TRI_index_challenge_t* challenge, void* data) {
  BitarrayIndex* baIndex = (BitarrayIndex*)(theIndex);
  if (baIndex == NULL || indexOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  assert(false);
  return TRI_ERROR_NO_ERROR;
}

static TRI_index_iterator_t* BitarrayIndex_resultMethodCall(void* theIndex, TRI_index_operator_t* indexOperator,
                                          void* data, bool (*filter) (TRI_index_iterator_t*)) {
  BitarrayIndex* baIndex = (BitarrayIndex*)(theIndex);
  if (baIndex == NULL || indexOperator == NULL) {
    return NULL;
  }
  assert(false);
  return NULL;
}

static int BitarrayIndex_freeMethodCall(void* theIndex, void* data) {
  BitarrayIndex* baIndex = (BitarrayIndex*)(theIndex);
  if (baIndex == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  assert(false);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared debug functions
////////////////////////////////////////////////////////////////////////////////

/*
void BitarrayIndex_debugPrintMaskFooter(const char* footer) {
  printf("%s\n\n",footer);
}

void BitarrayIndex_debugPrintMaskHeader(const char* header) {
  printf("------------------- %s --------------------------\n",header);
}


void BitarrayIndex_debugPrintMask(BitarrayIndex* baIndex, uint64_t mask) {
  int j;

  for (j = 0; j < baIndex->_bitarray->_numColumns; ++j) {
    if ((mask & ((uint64_t)(1) << j)) == 0) {
      printf("0");
    }
    else {
      printf("1");
    }
  }
  printf("\n");
}
*/


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:


