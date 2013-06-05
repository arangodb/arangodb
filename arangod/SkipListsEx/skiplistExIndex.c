////////////////////////////////////////////////////////////////////////////////
/// @brief skiplistEx index
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

#include "skiplistExIndex.h"
#include "VocBase/index-garbage-collector.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Common private methods
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


// .............................................................................
// forward declaration - some helper methods
// .............................................................................

static bool skiplistExIndex_findHelperIntervalValid      (SkiplistExIndex*, TRI_skiplistEx_iterator_interval_t*);
static bool multiSkiplistExIndex_findHelperIntervalValid (SkiplistExIndex*, TRI_skiplistEx_iterator_interval_t*);


// .............................................................................
// forward declaration of static functions which are used by the query engine
// .............................................................................

static int                   SkiplistExIndex_queryMethodCall  (void*, TRI_index_operator_t*, TRI_index_challenge_t*, void*);
static TRI_index_iterator_t* SkiplistExIndex_resultMethodCall (void*, TRI_index_operator_t*, void*, bool (*filter) (TRI_index_iterator_t*)); 
static int                   SkiplistExIndex_freeMethodCall   (void*, void*);


////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a next document within an interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistExHasNextIterationCallback(TRI_skiplistEx_iterator_t* iterator) {
  TRI_skiplistEx_iterator_interval_t* interval;
  void* leftNode;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return false;
  }

  if (iterator->_intervals._length == 0) {
    return false;
  }  

  
  // ............................................................................. 
  // if we have more intervals than the one we are currently working on then of course we have a next doc  
  // ............................................................................. 
  if (iterator->_intervals._length - 1 > iterator->_currentInterval) {
    return true;
  }

  
  // ............................................................................. 
  // Obtain the current interval -- in case we ever use more than one interval
  // ............................................................................. 
  
  interval =  (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
  // ............................................................................. 
  // Obtain the left end point we are currently at
  // ............................................................................. 
  
  if (iterator->_cursor == NULL) {
    leftNode = interval->_leftEndPoint;    
  }  
  else {
    leftNode = iterator->_cursor;
  }


  // ............................................................................. 
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ............................................................................. 
  
  if (leftNode == interval->_rightEndPoint) {
    return false;
  }
    
  // ...........................................................................
  // interval of the type (a,b) -- but nothing between a and b
  // such intervals are optimised out so will not be here
  // ...........................................................................
  if (iterator->_index->_unique) {
    leftNode = TRI_NextNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, leftNode, iterator->_thisTransID);    
  }
  else {    
    leftNode = TRI_NextNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, leftNode, iterator->_thisTransID);    
  }
    

  // ...........................................................................
  // Check various possibilities
  // ...........................................................................
  
  if (leftNode == NULL) {
    return false;
  }
    
  if (leftNode == interval->_rightEndPoint) {
    return false;
  }
     
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a previous document within an interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistExHasPrevIterationCallback(TRI_skiplistEx_iterator_t* iterator) {
  TRI_skiplistEx_iterator_interval_t* interval;
  void* rightNode;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return false;
  }

  if (iterator->_intervals._length == 0) {
    return false;
  }  

  // ............................................................................. 
  // this check is as follows if we have more intervals than the one
  // we are currently working on -- then of course we have a prev doc  
  // ............................................................................. 
  if (iterator->_currentInterval > 0 ) {
    return true;
  }

  
  // ............................................................................. 
  // Obtain the current interval -- in case we ever use more than one interval
  // ............................................................................. 
  
  interval =  (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
  // ............................................................................. 
  // Obtain the left end point we are currently at
  // ............................................................................. 
  
  if (iterator->_cursor == NULL) {
    rightNode = interval->_rightEndPoint;    
  }  
  else {
    rightNode = iterator->_cursor;
  }


  // ............................................................................. 
  // If the left == right end point AND there are no more intervals then we have
  // no next.
  // ............................................................................. 
  
  if (rightNode == interval->_leftEndPoint) {
    return false;
  }
    
  // ...........................................................................
  // interval of the type (a,b) -- but nothing between a and b
  // such intervals are optimised out so will not be here
  // ...........................................................................
  if (iterator->_index->_unique) {
    rightNode = TRI_PrevNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, rightNode, iterator->_thisTransID);    
  }
  else {    
    rightNode = TRI_PrevNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, rightNode, iterator->_thisTransID);    
  }
    

  // ...........................................................................
  // Check various possibilities
  // ...........................................................................
  
  if (rightNode == NULL) {
    return false;
  }
    
  if (rightNode == interval->_leftEndPoint) {
    return false;
  }
     
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Jumps forwards or backwards by jumpSize and returns the document
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistExIteration(TRI_skiplistEx_iterator_t* iterator, int64_t jumpSize) {
  TRI_skiplistEx_iterator_interval_t* interval;
  TRI_skiplistEx_node_t* currentNode;
  int64_t j;
  
  // ............................................................................. 
  // Some simple checks.
  // ............................................................................. 
  
  if (iterator == NULL) {
    return NULL;
  }

  if (iterator->_intervals._length == 0) {
    return NULL;
  }  
 
  currentNode = (TRI_skiplistEx_node_t*) (iterator->_cursor);
  
  if (jumpSize == 0) {
    if (currentNode == NULL) {
      return NULL;
    }
    else {       
      return &(currentNode->_element);
    }    
  }

  
  
  // ............................................................................. 
  // If the current cursor is NULL and jumpSize < 0, then start at the endpoint of
  // the right most interval.
  // ............................................................................. 
  if (currentNode == NULL && jumpSize < 0) {
    interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_intervals._length - 1) );    
    
    if (iterator->_index->_unique) {
      iterator->_cursor = TRI_PrevNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, interval->_rightEndPoint, iterator->_thisTransID);    
    }
    else {
      iterator->_cursor = TRI_PrevNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, interval->_rightEndPoint, iterator->_thisTransID);    
    }    
    
    currentNode = (TRI_skiplistEx_node_t*) (iterator->_cursor);

    if (currentNode == NULL) {
      return NULL;
    }
    
    if (currentNode == interval->_leftEndPoint) {
      return NULL;
    }    

    return &(currentNode->_element);
  }
  
  
  // ............................................................................. 
  // If the current cursor is NULL and jumpSize > 0, then start at the left point of
  // the left most interval.
  // ............................................................................. 
  if (currentNode == NULL && jumpSize > 0) {
    interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), 0) );
    
    if (iterator->_index->_unique) {
      iterator->_cursor = TRI_NextNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, interval->_leftEndPoint, iterator->_thisTransID);    
    }  
    else {
      iterator->_cursor = TRI_NextNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, interval->_leftEndPoint, iterator->_thisTransID);    
    }    
    
    currentNode = (TRI_skiplistEx_node_t*) (iterator->_cursor);

    if (currentNode == NULL) {
      return NULL;
    }
    
    if (currentNode == interval->_rightEndPoint) {
    
      if (iterator->_index->_unique) {
        iterator->_cursor = TRI_NextNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, interval->_leftEndPoint, iterator->_thisTransID);    
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, interval->_leftEndPoint, iterator->_thisTransID);    
      }      

      currentNode = (TRI_skiplistEx_node_t*) (iterator->_cursor);
      return NULL;
    }
    
    return &(currentNode->_element);
  }
  
  
  // ............................................................................. 
  // Obtain the current interval we are at.
  // ............................................................................. 

  interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  // ............................................................................. 
  // use the current cursor and move jumpSize back.
  // ............................................................................. 
  if (jumpSize < 0) {
    jumpSize = -jumpSize;
    for (j = 0; j < jumpSize; ++j) {
      if (iterator->_cursor == interval->_leftEndPoint) {
        if (iterator->_currentInterval == 0) {
          return NULL;
        }  
        --iterator->_currentInterval;
        interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
        iterator->_cursor = interval->_rightEndPoint;
      }      
  
      if (iterator->_index->_unique) {
        iterator->_cursor = TRI_PrevNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }      
      
    } 
    
    if (iterator->_cursor == interval->_leftEndPoint) {
      if (iterator->_currentInterval == 0) {
        return NULL;
      }  
      --iterator->_currentInterval;
      interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_rightEndPoint;
      
      if (iterator->_index->_unique) {
        iterator->_cursor = TRI_PrevNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }      
      
    }  
  }


  // ............................................................................. 
  // use the current cursor and move jumpSize forward.
  // ............................................................................. 
  if (jumpSize > 0) {
  
    for (j = 0; j < jumpSize; ++j) {
      if (iterator->_cursor == interval->_rightEndPoint) {
        if (iterator->_currentInterval == (iterator->_intervals._length - 1)) {
          return NULL;
        }  
        ++iterator->_currentInterval;
        interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
        iterator->_cursor = interval->_leftEndPoint;
      }      

      if (iterator->_index->_unique) {
        iterator->_cursor = TRI_NextNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }      
    } 
    
    if (iterator->_cursor == interval->_rightEndPoint) {
      if (iterator->_currentInterval == (iterator->_intervals._length - 1)) {
        return NULL;
      }  
      ++iterator->_currentInterval;
      interval = (TRI_skiplistEx_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_leftEndPoint;

      if (iterator->_index->_unique) {
        iterator->_cursor = TRI_NextNodeSkipListEx(iterator->_index->_skiplistEx.uniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListExMulti(iterator->_index->_skiplistEx.nonUniqueSkiplistEx, iterator->_cursor, iterator->_thisTransID);
      }
      
    }      
  }

  
  currentNode = (TRI_skiplistEx_node_t*) (iterator->_cursor);
  if (currentNode == NULL) {
    return NULL;
  }
  return &(currentNode->_element);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistExNextIterationCallback(TRI_skiplistEx_iterator_t* iterator) {
  return SkiplistExIteration(iterator,1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistExNextsIterationCallback(TRI_skiplistEx_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistExIteration(iterator,jumpSize);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistExPrevIterationCallback(TRI_skiplistEx_iterator_t* iterator) {
  return SkiplistExIteration(iterator,-1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistExPrevsIterationCallback(TRI_skiplistEx_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistExIteration(iterator,-jumpSize);
}




// -----------------------------------------------------------------------------
// --SECTION--                           skiplistIndex     common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief Assigns a static function call to a function pointer used by Query Engine
////////////////////////////////////////////////////////////////////////////////

int SkiplistExIndex_assignMethod(void* methodHandle, TRI_index_method_assignment_type_e methodType) {
  switch (methodType) {
  
    case TRI_INDEX_METHOD_ASSIGNMENT_FREE : {
      TRI_index_query_free_method_call_t* call = (TRI_index_query_free_method_call_t*)(methodHandle);
      *call = SkiplistExIndex_freeMethodCall;
      break;
    }
    
    case TRI_INDEX_METHOD_ASSIGNMENT_QUERY : {
      TRI_index_query_method_call_t* call = (TRI_index_query_method_call_t*)(methodHandle);
      *call = SkiplistExIndex_queryMethodCall;
      break;
    }
    
    case TRI_INDEX_METHOD_ASSIGNMENT_RESULT : {
      TRI_index_query_result_method_call_t* call = (TRI_index_query_result_method_call_t*)(methodHandle);
      *call = SkiplistExIndex_resultMethodCall;
      break;
    }

    default : {
      assert(false);
    }
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Free a skiplist iterator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistExIterator (TRI_skiplistEx_iterator_t* const iterator) {
  assert(iterator);

  TRI_DestroyVector(&iterator->_intervals);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, iterator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistExIndex_destroy(SkiplistExIndex* slIndex) {
  if (slIndex == NULL) {
    return;
  } 
  
  if (slIndex->_unique) {
    TRI_FreeSkipListEx(slIndex->_skiplistEx.uniqueSkiplistEx);
    slIndex->_skiplistEx.uniqueSkiplistEx = NULL;
  }
  else {
    TRI_FreeSkipListExMulti(slIndex->_skiplistEx.nonUniqueSkiplistEx);
    slIndex->_skiplistEx.nonUniqueSkiplistEx = NULL;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistExIndex_free(SkiplistExIndex* slIndex) {
  if (slIndex == NULL) {
    return;
  }  
  SkiplistExIndex_destroy(slIndex);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, slIndex);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Unique Skiplists 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private Methods Unique Skiplists
//------------------------------------------------------------------------------




//------------------------------------------------------------------------------
// Public Methods Unique Skiplists
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new unique entry skiplist
////////////////////////////////////////////////////////////////////////////////

SkiplistExIndex* SkiplistExIndex_new(TRI_transaction_context_t* transactionContext) {
  SkiplistExIndex* skiplistExIndex;
  int result;
  uint64_t lastKnownTransID = 0;
  
  skiplistExIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(SkiplistExIndex), true);
  if (skiplistExIndex == NULL) {
    return NULL;
  }

  skiplistExIndex->_unique = true;
  skiplistExIndex->_skiplistEx.uniqueSkiplistEx = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_t), true);
  if (skiplistExIndex->_skiplistEx.uniqueSkiplistEx == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex);
    return NULL;
  }    
    
  skiplistExIndex->_transactionContext = transactionContext;
  
  // TODO ORESTE: determine the last know transaction id from the transaction context  
  result = TRI_InitSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx,
                              sizeof(SkiplistExIndexElement),
                              NULL,NULL,TRI_SKIPLIST_EX_PROB_HALF, 40,
                              lastKnownTransID);
                              
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex->_skiplistEx.uniqueSkiplistEx);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex);
    return NULL;
  }  
  
  return skiplistExIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a unique skip list
////////////////////////////////////////////////////////////////////////////////

int SkiplistExIndex_add(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  int result;
  result = TRI_InsertKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, element, element, false, thisTransID);  
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the unique skiplist and returns iterator
//////////////////////////////////////////////////////////////////////////////////

// ...............................................................................
// Tests whether the LeftEndPoint is < than RightEndPoint (-1)
// Tests whether the LeftEndPoint is == to RightEndPoint (0)    [empty]
// Tests whether the LeftEndPoint is > than RightEndPoint (1)   [undefined]
// ...............................................................................
/*
static void debugElement(SkiplistIndex* skiplistIndex, TRI_skiplist_node_t* node) {
  size_t numFields;
  SkiplistIndexElement* element = (SkiplistIndexElement*)(&(node->_element));
  TRI_shaper_t* shaper;
  size_t j;
  
  if (node == NULL) {
    printf("%s:%u:node null\n",__FILE__,__LINE__);
    return;
  }    
  
  if (node == TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    printf("%s:%u:start node\n",__FILE__,__LINE__);
  }
  
  if (node == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    printf("%s:%u:end node\n",__FILE__,__LINE__);
  }
  
  if (element == NULL) {
    printf("%s:%u:element null\n",__FILE__,__LINE__);
    return;
  }    
  
  numFields = element->numFields;
  shaper    = ((TRI_primary_collection_t*)(element->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    printf("%s:%u:!!!:%f:%lu\n",__FILE__,__LINE__,
      *((double*)((j + element->fields)->_data.data)),
      (long unsigned int)(element->data) );
  }
  return;
}
*/

static bool skiplistExIndex_findHelperIntervalIntersectionValid(SkiplistExIndex* skiplistExIndex,
                                                TRI_skiplistEx_iterator_interval_t* lInterval, 
                                                TRI_skiplistEx_iterator_interval_t* rInterval, 
                                                TRI_skiplistEx_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplistEx_node_t* lNode;
  TRI_skiplistEx_node_t* rNode;

  lNode = (TRI_skiplistEx_node_t*)(lInterval->_leftEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(rInterval->_leftEndPoint);
    
  if (lNode == TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx) || lNode == NULL || 
      rNode == TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx) || rNode == NULL) {
    return false;
  }

  if (lNode == TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) {
    compareResult = -1;
  }
  else if (rNode == TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) {
    compareResult = 1;
  }  
  else {
    compareResult = skiplistExIndex->_skiplistEx.uniqueSkiplistEx->compareKeyElement(
                                                  skiplistExIndex->_skiplistEx.uniqueSkiplistEx, 
                                                  &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  


  
  lNode = (TRI_skiplistEx_node_t*)(lInterval->_rightEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(rInterval->_rightEndPoint);
  
  if (lNode == TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistExIndex->_skiplistEx.uniqueSkiplistEx->compareKeyElement(
                                              skiplistExIndex->_skiplistEx.uniqueSkiplistEx, 
                                              &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {  
    interval->_rightEndPoint = rNode;
  }  

  return skiplistExIndex_findHelperIntervalValid(skiplistExIndex, interval); 
}

static bool skiplistExIndex_findHelperIntervalValid(SkiplistExIndex* skiplistExIndex, TRI_skiplistEx_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplistEx_node_t* lNode;
  TRI_skiplistEx_node_t* rNode;
  
  
  if ((interval->_leftEndPoint == NULL) || (interval->_rightEndPoint == NULL)) {
    return false;
  }

  if (interval->_leftEndPoint == interval->_rightEndPoint) {
    return false;
  }
    
  if ( (interval->_leftEndPoint  == TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) || 
       (interval->_rightEndPoint == TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)))  {
    return true;
  }
  
  lNode = (TRI_skiplistEx_node_t*)(interval->_leftEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(interval->_rightEndPoint);


  compareResult = skiplistExIndex->_skiplistEx.uniqueSkiplistEx->compareKeyElement(
                                              skiplistExIndex->_skiplistEx.uniqueSkiplistEx, 
                                              &(lNode->_element), &(rNode->_element), 0);
  return (compareResult == -1);                                              
} 


static void SkiplistExIndex_findHelper(SkiplistExIndex* skiplistExIndex, 
                                     TRI_vector_t* shapeList, 
                                     TRI_index_operator_t* indexOperator,
                                     TRI_vector_t* resultIntervalList, 
                                     uint64_t thisTransID) {
                                 
  SkiplistExIndexElement              values;
  TRI_vector_t                        leftResult;
  TRI_vector_t                        rightResult;
  TRI_relation_index_operator_t*      relationOperator;
  TRI_logical_index_operator_t*       logicalOperator;
  TRI_skiplistEx_iterator_interval_t  interval; 
  TRI_skiplistEx_iterator_interval_t* tempLeftInterval; 
  TRI_skiplistEx_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  TRI_InitVector(&(rightResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  
  relationOperator  = (TRI_relation_index_operator_t*)(indexOperator);
  logicalOperator   = (TRI_logical_index_operator_t*)(indexOperator);
  
  switch (indexOperator->_type) {
    case TRI_EQ_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR: 
    case TRI_LT_INDEX_OPERATOR: 
    case TRI_GE_INDEX_OPERATOR: 
    case TRI_GT_INDEX_OPERATOR: 
    
      values.fields     = relationOperator->_fields;
      values.numFields  = relationOperator->_numFields;
      values.collection = relationOperator->_collection;
      values.data       = 0; // we do not have a document pointer
      
    default: {
      // must not access relationOperator->xxx if the operator is not a relational one
      // otherwise we'll get invalid reads and the prog might crash
    }
  }
  
  switch (indexOperator->_type) {

    /*
    case TRI_SL_OR_OPERATOR: {
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_left,&leftResult); 
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_right,&leftResult); 
      i = 0;
      while (i < leftResult._length - 1) {
        tempLeftInterval  =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&leftResult, i));              
        tempRightInterval =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&leftResult, i + 1));              
        // if intervals intersect, optimise and start again
      }
      assert(0);
    }
    */
    
    case TRI_AND_INDEX_OPERATOR: {
      SkiplistExIndex_findHelper(skiplistExIndex,shapeList,logicalOperator->_left,&leftResult, thisTransID); 
      SkiplistExIndex_findHelper(skiplistExIndex,shapeList,logicalOperator->_right,&rightResult, thisTransID); 
      
      for (i = 0; i < leftResult._length; ++i) {
        for (j = 0; j < rightResult._length; ++j) {
          tempLeftInterval  =  (TRI_skiplistEx_iterator_interval_t*) (TRI_AtVector(&leftResult, i));    
          tempRightInterval =  (TRI_skiplistEx_iterator_interval_t*) (TRI_AtVector(&rightResult, j));    
          if (!skiplistExIndex_findHelperIntervalIntersectionValid(skiplistExIndex, tempLeftInterval, 
                                                                 tempRightInterval, &interval)) {
            continue;
          }
          TRI_PushBackVector(resultIntervalList,&interval);
        }
      }
      TRI_DestroyVector(&leftResult);
      TRI_DestroyVector(&rightResult);
      return;
    }

    
    case TRI_EQ_INDEX_OPERATOR: {
      // ............................................................................
      // The index is constructed from n fields and the client has sent us n values  
      // ............................................................................
      if (relationOperator->_numFields == shapeList->_length) {
        interval._leftEndPoint  = TRI_LookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
        if (interval._leftEndPoint != NULL) {
          interval._rightEndPoint = TRI_NextNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, interval._leftEndPoint, thisTransID);      
          interval._leftEndPoint  = TRI_PrevNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, interval._leftEndPoint, thisTransID);      
        }        
      }  
      // ............................................................................
      // The index is constructed from n fields and the client has sent us m values  
      // where m < n
      // ............................................................................
      else {
        interval._leftEndPoint  = TRI_LeftLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
        interval._rightEndPoint = TRI_RightLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
      }      
      
      if (skiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;    
    }
    
    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx);
      interval._rightEndPoint = TRI_RightLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
      if (skiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  
      return;
    }  
    
    
    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx);
      interval._rightEndPoint = TRI_LeftLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
      if (interval._rightEndPoint != TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) { 
        interval._rightEndPoint = TRI_NextNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, interval._rightEndPoint, thisTransID);      
      }      
      if (skiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }  
      return;
    }  
    

    case TRI_GE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
      interval._rightEndPoint = TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx);
      if (skiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  
      return;
    }  
  
  
    case TRI_GT_INDEX_OPERATOR: {
      interval._leftEndPoint = TRI_RightLookupByKeySkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, &values, thisTransID); 
      interval._rightEndPoint = TRI_EndNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx);
      if (interval._leftEndPoint != TRI_StartNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx)) {
        interval._leftEndPoint = TRI_PrevNodeSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, interval._leftEndPoint, thisTransID);      
      }  
      if (skiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  
      return;
    }  
    
    default: {
      assert(0);
    }
    
  } // end of switch statement

  
}

TRI_skiplistEx_iterator_t* SkiplistExIndex_find(SkiplistExIndex* skiplistExIndex, TRI_vector_t* shapeList, TRI_index_operator_t* indexOperator, uint64_t thisTransID) {
  TRI_skiplistEx_iterator_t* results;
 
  results = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_t), true);
  if (results == NULL) {
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  results->_index = skiplistExIndex;
  TRI_InitVector(&(results->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_thisTransID     = thisTransID;
  results->_hasNext         = SkiplistExHasNextIterationCallback;
  results->_next            = SkiplistExNextIterationCallback;
  results->_nexts           = SkiplistExNextsIterationCallback;
  results->_hasPrev         = SkiplistExHasPrevIterationCallback;
  results->_prev            = SkiplistExPrevIterationCallback;
  results->_prevs           = SkiplistExPrevsIterationCallback;
  
  SkiplistExIndex_findHelper(skiplistExIndex, shapeList, indexOperator, &(results->_intervals), thisTransID);
  
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int SkiplistExIndex_insert(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  return SkiplistExIndex_add(skiplistExIndex,element, thisTransID);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skip list
//////////////////////////////////////////////////////////////////////////////////

static int CollectSkiplistExGarbage(TRI_index_gc_t* indexGCData) {
  int result = TRI_ERROR_NO_ERROR;
  return result;
}


int SkiplistExIndex_remove(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  int result;
  TRI_skiplistEx_node_t* passNode;
  TRI_index_gc_t indexGCData;


  // ............................................................................
  // This has been called from the database so it has a pass level of 1
  // ............................................................................
  
  result = TRI_RemoveElementSkipListEx(skiplistExIndex->_skiplistEx.uniqueSkiplistEx, element, 
                                       NULL, 1, thisTransID, &passNode); 
  if (result == TRI_ERROR_NO_ERROR) {
  
    // ..........................................................................
    // add to garbage collection
    // ..........................................................................
    
    indexGCData._index    = (void*)(skiplistExIndex);
    indexGCData._passes   = 2;
    indexGCData._lastPass = 0; // will be assigned correctly by the GC
    indexGCData._transID  = 0; // will be assigned correctly by the GC
    indexGCData._data     = passNode; // the address of the node in the linked list which will eventually be excised
    indexGCData._collectGarbage = CollectSkiplistExGarbage;
    result = TRI_AddToIndexGC(&indexGCData); // adds an item to the rubbish collection linked list    
  }  
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a skiplist entry
//////////////////////////////////////////////////////////////////////////////////

bool SkiplistExIndex_update(SkiplistExIndex* skiplistExIndex, const SkiplistExIndexElement* beforeElement, const SkiplistExIndexElement* afterElement, uint64_t thisTransID) {
  // updates an entry in the skip list, first removes beforeElement, 
  // then adds the afterElement -- should never be called here
  // call SkiplistIndex_remove first and then SkiplistIndex_add
  assert(false);
  return false; // shuts the VC++ up
}







//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------






//------------------------------------------------------------------------------
// Public Methods Non-Unique Muilti Skiplists
//------------------------------------------------------------------------------






////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new non-uniqe (allows duplicates) multi skiplist
////////////////////////////////////////////////////////////////////////////////

SkiplistExIndex* MultiSkiplistExIndex_new(TRI_transaction_context_t* transactionContext) {
  SkiplistExIndex* skiplistExIndex;
  int result;
  uint64_t lastKnownTransID = 0;
  
  skiplistExIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(SkiplistExIndex), true);
  if (skiplistExIndex == NULL) {
    return NULL;
  }

  skiplistExIndex->_unique = false;
  skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_multi_t), true);
  if (skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex);
    return NULL;
  }    
    
  skiplistExIndex->_transactionContext = transactionContext;
  
  // TODO ORESTE: determine the last know transaction id from the transaction context  
  result = TRI_InitSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx,
                        sizeof(SkiplistExIndexElement),
                        NULL, NULL, NULL,TRI_SKIPLIST_EX_PROB_HALF, 40, 
                        lastKnownTransID);
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistExIndex);
    return NULL;
  }  
  return skiplistExIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a multi skiplist
////////////////////////////////////////////////////////////////////////////////


int MultiSkiplistExIndex_add(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  int result;
  result = TRI_InsertElementSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, element, false, thisTransID);  
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the unique skiplist and returns iterator
//////////////////////////////////////////////////////////////////////////////////

static bool multiSkiplistExIndex_findHelperIntervalIntersectionValid(SkiplistExIndex* skiplistExIndex,
                                                TRI_skiplistEx_iterator_interval_t* lInterval, 
                                                TRI_skiplistEx_iterator_interval_t* rInterval, 
                                                TRI_skiplistEx_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplistEx_node_t* lNode;
  TRI_skiplistEx_node_t* rNode;

  lNode = (TRI_skiplistEx_node_t*)(lInterval->_leftEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(rInterval->_leftEndPoint);
    
  if (lNode == TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx) || lNode == NULL || 
      rNode == TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx) || rNode == NULL) {
    return false;
  }

  if (lNode == TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
    compareResult = -1;
  }
  else if (rNode == TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
    compareResult = 1;
  }  
  else {
    compareResult = skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx->compareKeyElement(
                                                skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, 
                                                &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  


  
  lNode = (TRI_skiplistEx_node_t*)(lInterval->_rightEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(rInterval->_rightEndPoint);
  
  if (lNode == TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx->compareKeyElement(
                                              skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, 
                                              &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {  
    interval->_rightEndPoint = rNode;
  }  

  return multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, interval); 
}



static bool multiSkiplistExIndex_findHelperIntervalValid(SkiplistExIndex* skiplistExIndex, TRI_skiplistEx_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplistEx_node_t* lNode;
  TRI_skiplistEx_node_t* rNode;
  
  
  if ((interval->_leftEndPoint == NULL) || (interval->_rightEndPoint == NULL)) {
    return 0;
  }

  if (interval->_leftEndPoint == interval->_rightEndPoint) {
    return 0;
  }
    
  if ( (interval->_leftEndPoint  == TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) || 
       (interval->_rightEndPoint == TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)))  {
    return -1;
  }
  
  lNode = (TRI_skiplistEx_node_t*)(interval->_leftEndPoint);
  rNode = (TRI_skiplistEx_node_t*)(interval->_rightEndPoint);


  compareResult = skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx->compareKeyElement(
                                              skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, 
                                              &(lNode->_element), &(rNode->_element), 0);
                                              
                                              
  return (compareResult == -1);  
} 



static void MultiSkiplistExIndex_findHelper(SkiplistExIndex* skiplistExIndex, 
                                          TRI_vector_t* shapeList, 
                                          TRI_index_operator_t* indexOperator,
                                          TRI_vector_t* resultIntervalList, 
                                          uint64_t thisTransID) {
                                 
  SkiplistExIndexElement              values;
  TRI_vector_t                        leftResult;
  TRI_vector_t                        rightResult;
  TRI_relation_index_operator_t*      relationOperator;
  TRI_logical_index_operator_t*       logicalOperator;
  TRI_skiplistEx_iterator_interval_t  interval; 
  TRI_skiplistEx_iterator_interval_t* tempLeftInterval; 
  TRI_skiplistEx_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  TRI_InitVector(&(rightResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  
  logicalOperator   = (TRI_logical_index_operator_t*)(indexOperator);
  relationOperator  = (TRI_relation_index_operator_t*)(indexOperator);

  switch (indexOperator->_type) {
    case TRI_EQ_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR: 
    case TRI_LT_INDEX_OPERATOR: 
    case TRI_GE_INDEX_OPERATOR: 
    case TRI_GT_INDEX_OPERATOR: 
    
      values.fields     = relationOperator->_fields;
      values.numFields  = relationOperator->_numFields;
      values.collection = relationOperator->_collection;
      values.data       = 0; // no document pointer available 
      
    default: {
      // must not access relationOperator->xxx if the operator is not a relational one
      // otherwise we'll get invalid reads and the prog might crash
    }
  }
  
  switch (indexOperator->_type) {

    /*
    case TRI_SL_OR_OPERATOR: {
      todo
    }
    */
    
    case TRI_AND_INDEX_OPERATOR: {
      MultiSkiplistExIndex_findHelper(skiplistExIndex,shapeList,logicalOperator->_left,&leftResult, thisTransID); 
      MultiSkiplistExIndex_findHelper(skiplistExIndex,shapeList,logicalOperator->_right,&rightResult, thisTransID); 
      
      for (i = 0; i < leftResult._length; ++i) {
        for (j = 0; j < rightResult._length; ++j) {
          tempLeftInterval  =  (TRI_skiplistEx_iterator_interval_t*) (TRI_AtVector(&leftResult, i));    
          tempRightInterval =  (TRI_skiplistEx_iterator_interval_t*) (TRI_AtVector(&rightResult, j));    
          if (!multiSkiplistExIndex_findHelperIntervalIntersectionValid(skiplistExIndex,tempLeftInterval, 
                                                                      tempRightInterval, &interval)) {
            continue;
          }
          TRI_PushBackVector(resultIntervalList,&interval);
        }
      }
      TRI_DestroyVector(&leftResult);
      TRI_DestroyVector(&rightResult);
      return;
    }

    
    case TRI_EQ_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      interval._rightEndPoint = TRI_RightLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      if (multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;    
    }
        
    
    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx);
      interval._rightEndPoint = TRI_RightLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      if (multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }
      return;
    }  
    
    
    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx);
      interval._rightEndPoint = TRI_LeftLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      if (interval._rightEndPoint != TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
        interval._rightEndPoint = TRI_NextNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, interval._rightEndPoint, thisTransID);      
      }
      if (multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }      
      return;
    }  
    

    case TRI_GE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      interval._rightEndPoint = TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx);
      if (multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;
    }  
  
  
    case TRI_GT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_RightLookupByKeySkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, &values, thisTransID); 
      interval._rightEndPoint = TRI_EndNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx);
      if (interval._leftEndPoint != TRI_StartNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx)) {
        interval._leftEndPoint = TRI_PrevNodeSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, interval._leftEndPoint, thisTransID);      
      }
      if (multiSkiplistExIndex_findHelperIntervalValid(skiplistExIndex, &interval)) {
        TRI_PushBackVector(resultIntervalList, &interval);
      }      
      return;
    }  
    
    default: {
      assert(0);
    }
    
  } // end of switch statement  
}


TRI_skiplistEx_iterator_t* MultiSkiplistExIndex_find(SkiplistExIndex* skiplistExIndex, TRI_vector_t* shapeList, TRI_index_operator_t* indexOperator, uint64_t thisTransID) {
  TRI_skiplistEx_iterator_t* results;
 
  results = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_t), false);    
  if (results == NULL) {
    return NULL;
  }  
  
  results->_index = skiplistExIndex;
  TRI_InitVector(&(results->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplistEx_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_thisTransID     = thisTransID;
  results->_hasNext         = SkiplistExHasNextIterationCallback;
  results->_next            = SkiplistExNextIterationCallback;
  results->_nexts           = SkiplistExNextsIterationCallback;
  results->_hasPrev         = SkiplistExHasPrevIterationCallback;
  results->_prev            = SkiplistExPrevIterationCallback;
  results->_prevs           = SkiplistExPrevsIterationCallback;
  
  MultiSkiplistExIndex_findHelper(skiplistExIndex, shapeList, indexOperator, &(results->_intervals), thisTransID);
    
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int MultiSkiplistExIndex_insert(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  return MultiSkiplistExIndex_add(skiplistExIndex, element, thisTransID);
} 


//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skiplist
//////////////////////////////////////////////////////////////////////////////////

static int CollectSkiplistExMultiGarbage(TRI_index_gc_t* indexGCData) {
  int result = TRI_ERROR_NO_ERROR;
  return result;
}


int MultiSkiplistExIndex_remove(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* element, uint64_t thisTransID) {
  int result;
  TRI_skiplistEx_node_t* passNode;
  TRI_index_gc_t indexGCData;
  
  // ............................................................................
  // This has been called from the database so it has a pass level of 1
  // ............................................................................
  
  result = TRI_RemoveElementSkipListExMulti(skiplistExIndex->_skiplistEx.nonUniqueSkiplistEx, 
                                            element, NULL, 1, thisTransID, &passNode); 
  if (result == TRI_ERROR_NO_ERROR) {
  
    // ..........................................................................
    // add to garbage collection
    // ..........................................................................
    
    indexGCData._index    = (void*)(skiplistExIndex);
    indexGCData._passes   = 2;
    indexGCData._lastPass = 0; // will be assigned correctly by the GC
    indexGCData._transID  = 0; // will be assigned correctly by the GC
    indexGCData._data     = passNode; // the address of the node in the linked list which will eventually be excised
    indexGCData._collectGarbage = CollectSkiplistExMultiGarbage;
    result = TRI_AddToIndexGC(&indexGCData); // adds an item to the rubbish collection linked list    
  }  
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates and entry in a multi skiplist
//////////////////////////////////////////////////////////////////////////////////

bool MultiSkiplistExIndex_update(SkiplistExIndex* skiplistExIndex, SkiplistExIndexElement* beforeElement, SkiplistExIndexElement* afterElement, uint64_t thisTransID) {
  assert(false); // should never be called directly
  return false; // shuts the VC++ up
}


////////////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared query engine callback functions
////////////////////////////////////////////////////////////////////////////////////////

static int SkiplistExIndex_queryMethodCall(void* theIndex, TRI_index_operator_t* indexOperator, 
                                         TRI_index_challenge_t* challenge, void* data) {
  SkiplistExIndex* slIndex = (SkiplistExIndex*)(theIndex);
  if (slIndex == NULL || indexOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  assert(false);
  return TRI_ERROR_NO_ERROR;
}

static TRI_index_iterator_t* SkiplistExIndex_resultMethodCall(void* theIndex, TRI_index_operator_t* indexOperator, 
                                          void* data, bool (*filter) (TRI_index_iterator_t*)) {
  SkiplistExIndex* slIndex = (SkiplistExIndex*)(theIndex);
  if (slIndex == NULL || indexOperator == NULL) {
    return NULL;
  }
  assert(false);
  return NULL;
}

static int SkiplistExIndex_freeMethodCall(void* theIndex, void* data) {
  SkiplistExIndex* slIndex = (SkiplistExIndex*)(theIndex);
  if (slIndex == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  assert(false);
  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


