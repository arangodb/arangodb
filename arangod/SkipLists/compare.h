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

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief compares elements
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement (TRI_skiplist_index_element_t* left,
                                  size_t leftPosition,
                                  TRI_skiplist_index_element_t* right,
                                  size_t rightPosition,
                                  TRI_shaper_t* shaper) {
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
                                 shaper,
                                 shaper);
  
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
  TRI_shaper_t* shaper;
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
  
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................

  if (leftElement->_document == rightElement->_document) {
    return 0;
  }    
  
  shaper = skiplist->base._collection->_shaper;
  
  for (j = 0;  j < skiplist->base._numFields;  j++) {
    compareResult = CompareElementElement(leftElement,
                                          j,
                                          rightElement,
                                          j,
                                          shaper);

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
                                                  TRI_skiplist_index_element_t* rightElement) {
  int compareResult;
  TRI_shaper_t* shaper;
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

  if (leftElement == rightElement) {
    return 0;
  }

  if (leftElement->_document == rightElement->_document) {
    return 0;
  }

  shaper = multiSkiplist->base._collection->_shaper;

  for (j = 0;  j < multiSkiplist->base._numFields;  j++) {
    compareResult = CompareElementElement(leftElement,
                                          j,
                                          rightElement,
                                          j,
                                          shaper);

    if (compareResult != 0) {

      // ......................................................................
      // The function CompareShapedJsonShapedJson can only return 0, -1, or 1
      // for equal, strictly less than, or strictly greater than.
      // ......................................................................

      return compareResult;

    }
  }
  // We break this tie in the key comparison by looking at the key: 
  compareResult = strcmp(leftElement->_document->_key,rightElement->_document->_key);
  if (compareResult < 0) {
      return -1;
  }
  else if (compareResult > 0) {
      return 1;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

