////////////////////////////////////////////////////////////////////////////////
/// @brief compare methods used for skiplist indexes
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

#ifndef TRIAGENS_SKIP_LISTS_COMPARE_H
#define TRIAGENS_SKIP_LISTS_COMPARE_H 1

#include "BasicsC/common.h"

#include "BasicsC/utf8-helper.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/primary-collection.h"
#include "VocBase/voc-shaper.h"

#define USE_STATIC_SKIPLIST_COMPARE 1

#ifdef __cplusplus
extern "C" {
#endif

static int IndexStaticCopyElementElement (TRI_skiplist_base_t* skiplist,
                                          TRI_skiplist_index_element_t* leftElement,
                                          TRI_skiplist_index_element_t* rightElement) {
  if (leftElement == NULL || rightElement == NULL) {
    return TRI_ERROR_INTERNAL;
  }
    
  leftElement->numFields   = rightElement->numFields;
  leftElement->_document   = rightElement->_document;
  leftElement->collection  = rightElement->collection;
  leftElement->_subObjects = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_sub_t) * leftElement->numFields, false);
  
  if (leftElement->_subObjects == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  memcpy(leftElement->_subObjects, rightElement->_subObjects, sizeof(TRI_shaped_sub_t) * leftElement->numFields);
  
  return TRI_ERROR_NO_ERROR;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an element -- removing any allocated memory within the structure
////////////////////////////////////////////////////////////////////////////////

static void IndexStaticDestroyElement (TRI_skiplist_base_t* skiplist,
                                       TRI_skiplist_index_element_t* element) {

  // ...........................................................................
  // Each 'field' in the hElement->fields is a TRI_shaped_json_t object, this
  // structure has internal structure of its own -- which also has memory
  // allocated -- HOWEVER we DO NOT deallocate this memory here since it
  // is actually part of the document structure. This memory should be deallocated
  // when the document has been destroyed.
  // ...........................................................................
  
  if (element != NULL) {
    if (element->_subObjects != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
    }  
  }  
}

// .............................................................................
// left < right  return -1
// left > right  return  1
// left == right return  0
// .............................................................................

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key and an element
////////////////////////////////////////////////////////////////////////////////

static int CompareKeyElement (TRI_shaped_json_t const* left,
                              TRI_skiplist_index_element_t* right,
                              size_t rightPosition,
                              TRI_shaper_t* leftShaper,
                              TRI_shaper_t* rightShaper) {
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
    
  result = TRI_CompareShapeTypes(NULL,
                                 NULL, 
                                 left,
                                 right->_document,
                                 &right->_subObjects[rightPosition],
                                 NULL,
                                 leftShaper,
                                 rightShaper);
  
  // ............................................................................
  // In the above function CompareShaeTypes we use strcmp which may return
  // an integer greater than 1 or less than -1. From this function we only
  // need to know whether we have equality (0), less than (-1)  or greater than (1)
  // ............................................................................
  
  if (result < 0) { 
    result = -1; 
  }
  else if (result > 0) { 
    result = 1; 
  }
  
  return result;
}  // end of function CompareShapedJsonShapedJson

////////////////////////////////////////////////////////////////////////////////
/// @brief compares elements
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement (TRI_skiplist_index_element_t* left,
                                  size_t leftPosition,
                                  TRI_skiplist_index_element_t* right,
                                  size_t rightPosition,
                                  TRI_shaper_t* leftShaper,
                                  TRI_shaper_t* rightShaper) {
  int result;

  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicographically and within each slot according to these rules.
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
  result = TRI_CompareShapeTypes(left->_document,
                                 &left->_subObjects[leftPosition],
                                 NULL, 
                                 right->_document,
                                 &right->_subObjects[rightPosition],
                                 NULL,
                                 leftShaper,
                                 rightShaper);
  
  // ............................................................................
  // In the above function CompareShapeTypes we use strcmp which may return
  // an integer greater than 1 or less than -1. From this function we only
  // need to know whether we have equality (0), less than (-1)  or greater than (1)
  // ............................................................................

  if (result < 0) {
    result = -1;
  }
  else if (result > 0) {
    result = 1;
  }
  
  return result;
}  // end of function CompareShapedJsonShapedJson

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticCompareElementElement (struct TRI_skiplist_s* skiplist,
                                             TRI_skiplist_index_element_t* leftElement,
                                             TRI_skiplist_index_element_t* rightElement,
                                             int defaultEqual) {

  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................

  int compareResult;
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;

  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicographically and within each slot according to these rules.
  // ............................................................................

  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }

  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }

  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }

  if (leftElement == rightElement) {
    return 0;
  }

  // ............................................................................
  // This call back function is used when we insert and remove unique skip
  // list entries.
  // ............................................................................
  
  if (leftElement->numFields != rightElement->numFields) {
    assert(false);
  }
  
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................

  if (leftElement->_document == rightElement->_document) {
    return 0;
  }    
  
  leftShaper  = ((TRI_primary_collection_t*)(leftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(rightElement->collection))->_shaper;
  
  for (j = 0;  j < leftElement->numFields;  j++) {
    compareResult = CompareElementElement(leftElement,
                                          j,
                                          rightElement,
                                          j,
                                          leftShaper,
                                          rightShaper);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  // ............................................................................
  // This is where the difference between CompareKeyElement (below) and
  // CompareElementElement comes into play. Here if the 'keys' are the same,
  // but the doc ptr is different (which it is since we are here), then
  // we return what was requested to be returned: 0,-1 or 1. What is returned
  // depends on the purpose of calling this callback.
  // ............................................................................

  return defaultEqual;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a key and an element
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticCompareKeyElement (struct TRI_skiplist_s* skiplist,
                                         TRI_skiplist_index_key_t* leftElement,
                                         TRI_skiplist_index_element_t* rightElement,
                                         int defaultEqual) {

  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................

  int compareResult;                               
  size_t numFields;
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;

  // ............................................................................
  // the following order is currently defined for placing an order on documents
  // undef < null < boolean < number < strings < lists < hash arrays
  // note: undefined will be treated as NULL pointer not NULL JSON OBJECT
  // within each type class we have the following order
  // boolean: false < true
  // number: natural order
  // strings: lexicographical
  // lists: lexicorgraphically and within each slot according to these rules.
  // associative array: ordered keys followed by value of key
  // ............................................................................

  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }

  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }

  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }

  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................

  if (leftElement->numFields < rightElement->numFields) {
    numFields = leftElement->numFields;
  }
  else {
    numFields = rightElement->numFields;
  }
  
  leftShaper  = ((TRI_primary_collection_t*)(leftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(rightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    compareResult = CompareKeyElement(&leftElement->fields[j], 
                                      rightElement,
                                      j,
                                      leftShaper,
                                      rightShaper);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  // ............................................................................
  // The 'keys' match -- however, we may only have a partial match in reality
  // if not all keys comprising index have been used.
  // ............................................................................

  return defaultEqual;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Non-unique skiplist
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two elements
////////////////////////////////////////////////////////////////////////////////

static int IndexStaticMultiCompareElementElement (TRI_skiplist_multi_t* multiSkiplist,
                                                  TRI_skiplist_index_element_t* leftElement,
                                                  TRI_skiplist_index_element_t* rightElement,
                                                  int defaultEqual) {
  int compareResult;
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;


  if (leftElement == NULL && rightElement == NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }

  if (leftElement != NULL && rightElement == NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_GREATER;
  }

  if (leftElement == NULL && rightElement != NULL) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_LESS;
  }

  if (leftElement == rightElement) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }

  if (leftElement->numFields != rightElement->numFields) {
    assert(false);
  }

  if (leftElement->_document == rightElement->_document) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }

  leftShaper  = ((TRI_primary_collection_t*)(leftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(rightElement->collection))->_shaper;

  for (j = 0;  j < leftElement->numFields;  j++) {
    compareResult = CompareElementElement(leftElement,
                                          j,
                                          rightElement,
                                          j,
                                          leftShaper,
                                          rightShaper);

    if (compareResult != 0) {

      // ......................................................................
      // The function CompareShaedJsonShapedJson can only return 0, -1, or 1
      // that is, TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL (0)
      // TRI_SKIPLIST_COMPARE_STRICTLY_LESS (-1)
      // TRI_SKIPLIST_COMPARE_STRICTLY_GREATER (1)
      // ......................................................................

      return compareResult;

    }
  }

  return defaultEqual;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static int  IndexStaticMultiCompareKeyElement (TRI_skiplist_multi_t* multiSkiplist,
                                               TRI_skiplist_index_key_t* leftElement,
                                               TRI_skiplist_index_element_t* rightElement,
                                               int defaultEqual) {
  int compareResult;
  size_t numFields;
  TRI_shaper_t* leftShaper;
  TRI_shaper_t* rightShaper;
  size_t j;

  if (leftElement == NULL && rightElement == NULL) {
    return 0;
  }

  if (leftElement != NULL && rightElement == NULL) {
    return 1;
  }

  if (leftElement == NULL && rightElement != NULL) {
    return -1;
  }

  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................
  
  if (leftElement->numFields < rightElement->numFields) {
    numFields = leftElement->numFields;
  }
  else {
    numFields = rightElement->numFields;
  }

  leftShaper  = ((TRI_primary_collection_t*)(leftElement->collection))->_shaper;
  rightShaper = ((TRI_primary_collection_t*)(rightElement->collection))->_shaper;

  for (j = 0; j < numFields; j++) {
    compareResult = CompareKeyElement(&leftElement->fields[j],
                                      rightElement,
                                      j,
                                      leftShaper,
                                      rightShaper);

    if (compareResult != 0) {
      return compareResult;
    }
  }

  return defaultEqual;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static bool IndexStaticMultiEqualElementElement (TRI_skiplist_multi_t* multiSkiplist,
                                                 TRI_skiplist_index_element_t* leftElement,
                                                 TRI_skiplist_index_element_t* rightElement) {
  if (leftElement == rightElement) {
    return true;
  }

  return (leftElement->_document == rightElement->_document);
}

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

