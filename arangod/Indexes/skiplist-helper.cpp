////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index helper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "skiplist-helper.h"
#include "Basics/Utf8Helper.h"
#include "VocBase/document-collection.h"
#include "VocBase/shaped-json.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                           skiplistIndex     common public methods
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Methods Skiplist Indices
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the skiplist and returns iterator
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Tests whether the LeftEndPoint is < than RightEndPoint (-1)
// Tests whether the LeftEndPoint is == to RightEndPoint (0)    [empty]
// Tests whether the LeftEndPoint is > than RightEndPoint (1)   [undefined]
// .............................................................................

static bool skiplistIndex_findHelperIntervalValid (triagens::arango::SkiplistIndex2* skiplistIndex,
                                                   TRI_skiplist_iterator_interval_t const* interval) {
  triagens::basics::SkipListNode* lNode = interval->_leftEndPoint;

  if (lNode == nullptr) {
    return false;
  }
  // Note that the right end point can be nullptr to indicate the end of
  // the index.

  triagens::basics::SkipListNode* rNode = interval->_rightEndPoint;

  if (lNode == rNode) {
    return false;
  }

  if (lNode->nextNode() == rNode) {
    // Interval empty, nothing to do with it.
    return false;
  }

  if (nullptr != rNode && rNode->nextNode() == lNode) {
    // Interval empty, nothing to do with it.
    return false;
  }

  if (skiplistIndex->skiplist->getNrUsed() == 0) {
    return false;
  }

  if ( lNode == skiplistIndex->skiplist->startNode() ||
       nullptr == rNode ) {
    // The index is not empty, the nodes are not neighbours, one of them
    // is at the boundary, so the interval is valid and not empty.
    return true;
  }

  int compareResult = CmpElmElm( skiplistIndex,
                                lNode->document(), 
                                rNode->document(), 
                                triagens::basics::SKIPLIST_CMP_TOTORDER);
  return (compareResult == -1);
  // Since we know that the nodes are not neighbours, we can guarantee
  // at least one document in the interval.
}

static bool skiplistIndex_findHelperIntervalIntersectionValid (
                    SkiplistIndex* skiplistIndex,
                    TRI_skiplist_iterator_interval_t* lInterval,
                    TRI_skiplist_iterator_interval_t* rInterval,
                    TRI_skiplist_iterator_interval_t* interval) {

  triagens::basics::SkipListNode* lNode = lInterval->_leftEndPoint;
  triagens::basics::SkipListNode* rNode = rInterval->_leftEndPoint;

  if (nullptr == lNode || nullptr == rNode) {
    // At least one left boundary is the end, intersection is empty.
    return false;
  }

  int compareResult;
  // Now find the larger of the two start nodes:
  if (lNode == skiplistIndex->skiplist->startNode()) {
    // We take rNode, even if it is the start node as well.
    compareResult = -1;
  }
  else if (rNode == skiplistIndex->skiplist->startNode()) {
    // We take lNode
    compareResult = 1;
  }
  else {
    compareResult = CmpElmElm(skiplistIndex, lNode->document(), 
                              rNode->document(), 
                              triagens::basics::SKIPLIST_CMP_TOTORDER);
  }

  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {
    interval->_leftEndPoint = lNode;
  }

  lNode = lInterval->_rightEndPoint;
  rNode = rInterval->_rightEndPoint;

  // Now find the smaller of the two end nodes:
  if (nullptr == lNode) {
    // We take rNode, even is this also the end node.
    compareResult = 1;
  }
  else if (nullptr == rNode) {
    // We take lNode.
    compareResult = -1;
  }
  else {
    compareResult = CmpElmElm(skiplistIndex, lNode->document(), 
                              rNode->document(),
                              triagens::basics::SKIPLIST_CMP_TOTORDER);
  }

  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {
    interval->_rightEndPoint = rNode;
  }

  return skiplistIndex_findHelperIntervalValid(skiplistIndex, interval);
}

static void SkiplistIndex_findHelper (triagens::arango::SkiplistIndex2* skiplistIndex,
                                      TRI_index_operator_t const* indexOperator,
                                      TRI_vector_t* resultIntervalList) {
  TRI_skiplist_index_key_t          values;
  TRI_vector_t                      leftResult;
  TRI_vector_t                      rightResult;
  TRI_skiplist_iterator_interval_t  interval;
  triagens::basics::SkipListNode*   temp;

  TRI_InitVector(&(leftResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  TRI_InitVector(&(rightResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));

  TRI_relation_index_operator_t* relationOperator  = (TRI_relation_index_operator_t*) indexOperator;
  TRI_logical_index_operator_t*  logicalOperator   = (TRI_logical_index_operator_t*) indexOperator;

  switch (indexOperator->_type) {
    case TRI_EQ_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:

      values._fields     = relationOperator->_fields;
      values._numFields  = relationOperator->_numFields;
      break;   // this is to silence a compiler warning

    default: {
      // must not access relationOperator->xxx if the operator is not a
      // relational one otherwise we'll get invalid reads and the prog
      // might crash
    }
  }

  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR: {
      SkiplistIndex_findHelper(skiplistIndex, logicalOperator->_left, &leftResult);
      SkiplistIndex_findHelper(skiplistIndex, logicalOperator->_right, &rightResult);

      size_t nl = TRI_LengthVector(&leftResult);
      size_t nr = TRI_LengthVector(&rightResult);
      for (size_t i = 0; i < nl; ++i) {
        for (size_t j = 0; j < nr; ++j) {
          auto tempLeftInterval  = static_cast<TRI_skiplist_iterator_interval_t*>(TRI_AddressVector(&leftResult, i));
          auto tempRightInterval = static_cast<TRI_skiplist_iterator_interval_t*>(TRI_AddressVector(&rightResult, j));

          if (skiplistIndex_findHelperIntervalIntersectionValid(
                            skiplistIndex,
                            tempLeftInterval,
                            tempRightInterval,
                            &interval)) {
            TRI_PushBackVector(resultIntervalList, &interval);
          }
        }
      }
      TRI_DestroyVector(&leftResult);
      TRI_DestroyVector(&rightResult);
      return;
    }


    case TRI_EQ_INDEX_OPERATOR: {
      temp = skiplistIndex->skiplist->leftKeyLookup(&values);
      TRI_ASSERT(nullptr != temp);
      interval._leftEndPoint = temp;

      bool const allAttributesCoveredByCondition = (values._numFields == skiplistIndex->_numFields);

      if (skiplistIndex->unique && allAttributesCoveredByCondition) {
        // At most one hit:
        temp = temp->nextNode();
        if (nullptr != temp) {
          if (0 == CmpKeyElm(skiplistIndex, &values, temp->document())) {
            interval._rightEndPoint = temp->nextNode();
            if (skiplistIndex_findHelperIntervalValid(skiplistIndex,
                                                      &interval)) {
              TRI_PushBackVector(resultIntervalList, &interval);
            }
          }
        }
      }
      else {
        temp = skiplistIndex->skiplist->rightKeyLookup(&values);
        interval._rightEndPoint = temp->nextNode();
        if (skiplistIndex_findHelperIntervalValid(skiplistIndex,
                                                  &interval)) {
          TRI_PushBackVector(resultIntervalList, &interval);
        }
      }
      return;
    }

    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint  = skiplistIndex->skiplist->startNode();
      temp = skiplistIndex->skiplist->rightKeyLookup(&values);
      interval._rightEndPoint = temp->nextNode();

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;
    }

    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint  = skiplistIndex->skiplist->startNode();
      temp = skiplistIndex->skiplist->leftKeyLookup(&values);
      interval._rightEndPoint = temp->nextNode();

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;
    }

    case TRI_GE_INDEX_OPERATOR: {
      temp = skiplistIndex->skiplist->leftKeyLookup(&values);
      interval._leftEndPoint = temp;
      interval._rightEndPoint = skiplistIndex->skiplist->endNode();

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;
    }

    case TRI_GT_INDEX_OPERATOR: {
      temp = skiplistIndex->skiplist->rightKeyLookup(&values);
      interval._leftEndPoint = temp;
      interval._rightEndPoint = skiplistIndex->skiplist->endNode();

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;
    }

    default: {
      TRI_ASSERT(false);
    }

  } // end of switch statement
}

TRI_skiplist_iterator_t* SkiplistIndex_find (triagens::arango::SkiplistIndex2* skiplistIndex,
                                             TRI_index_operator_t const* indexOperator,
                                             bool reverse) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
