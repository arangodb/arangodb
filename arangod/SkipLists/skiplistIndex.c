////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
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

#include "skiplistIndex.h"

#include "SkipLists/skiplist.h"
#include "VocBase/document-collection.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Common private methods
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


// .............................................................................
// forward declaration - some helper methods
// .............................................................................

static bool skiplistIndex_findHelperIntervalValid      (SkiplistIndex*, TRI_skiplist_iterator_interval_t*);
static bool multiSkiplistIndex_findHelperIntervalValid (SkiplistIndex*, TRI_skiplist_iterator_interval_t*);


// .............................................................................
// forward declaration of static functions which are used by the query engine
// .............................................................................

static int                   SkiplistIndex_queryMethodCall  (void*, TRI_index_operator_t*, TRI_index_challenge_t*, void*);
static TRI_index_iterator_t* SkiplistIndex_resultMethodCall (void*, TRI_index_operator_t*, void*, bool (*filter) (TRI_index_iterator_t*)); 
static int                   SkiplistIndex_freeMethodCall   (void*, void*);


////////////////////////////////////////////////////////////////////////////////
/// @brief Attempts to determine if there is a next document within an interval - without advancing the iterator.
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistHasNextIterationCallback(TRI_skiplist_iterator_t* iterator) {
  TRI_skiplist_iterator_interval_t* interval;
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
  
  interval =  (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
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
  if (iterator->_index->unique) {
    leftNode = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, leftNode);    
  }
  else {    
    leftNode = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, leftNode);    
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

static bool SkiplistHasPrevIterationCallback(TRI_skiplist_iterator_t* iterator) {
  TRI_skiplist_iterator_interval_t* interval;
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
  
  interval =  (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
  
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
  if (iterator->_index->unique) {
    rightNode = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, rightNode);    
  }
  else {    
    rightNode = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, rightNode);    
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

static void* SkiplistIteration(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  TRI_skiplist_iterator_interval_t* interval;
  TRI_skiplist_node_t* currentNode;
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
 
  currentNode = iterator->_cursor;
  
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
    interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_intervals._length - 1) );    
    
    if (iterator->_index->unique) {
      iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_rightEndPoint);    
    }
    else {
      iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_rightEndPoint);    
    }    
    
    currentNode = iterator->_cursor;

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
    interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), 0) );
    
    if (iterator->_index->unique) {
      iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_leftEndPoint);    
    }  
    else {
      iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_leftEndPoint);    
    }    
    
    currentNode = iterator->_cursor;

    if (currentNode == NULL) {
      return NULL;
    }
    
    if (currentNode == interval->_rightEndPoint) {
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, interval->_leftEndPoint);    
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, interval->_leftEndPoint);    
      }      

      return NULL;
    }
    
    return &(currentNode->_element);
  }
  
  
  // ............................................................................. 
  // Obtain the current interval we are at.
  // ............................................................................. 

  interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
  
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
        interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
        iterator->_cursor = interval->_rightEndPoint;
      }      
  
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }      
      
    } 
    
    if (iterator->_cursor == interval->_leftEndPoint) {
      if (iterator->_currentInterval == 0) {
        return NULL;
      }  
      --iterator->_currentInterval;
      interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_rightEndPoint;
      
      if (iterator->_index->unique) {
        iterator->_cursor = TRI_PrevNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_PrevNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
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
        interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), 
                                                         iterator->_currentInterval) );    
        iterator->_cursor = interval->_leftEndPoint;
      }      

      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }      
    } 
    
    if (iterator->_cursor == interval->_rightEndPoint) {
      if (iterator->_currentInterval == (iterator->_intervals._length - 1)) {
        return NULL;
      }  
      ++iterator->_currentInterval;
      interval = (TRI_skiplist_iterator_interval_t*) ( TRI_AtVector(&(iterator->_intervals), iterator->_currentInterval) );    
      iterator->_cursor = interval->_leftEndPoint;

      if (iterator->_index->unique) {
        iterator->_cursor = TRI_NextNodeSkipList(iterator->_index->skiplist.uniqueSkiplist, iterator->_cursor);
      }
      else {
        iterator->_cursor = TRI_NextNodeSkipListMulti(iterator->_index->skiplist.nonUniqueSkiplist, iterator->_cursor);
      }
      
    }      
  }
  
  currentNode = iterator->_cursor;

  if (currentNode == NULL) {
    return NULL;
  }

  return &(currentNode->_element);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistNextIterationCallback(TRI_skiplist_iterator_t* iterator) {
  return SkiplistIteration(iterator,1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping forward by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistNextsIterationCallback(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistIteration(iterator,jumpSize);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by 1
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistPrevIterationCallback(TRI_skiplist_iterator_t* iterator) {
  return SkiplistIteration(iterator,-1);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief default callback for jumping backwards by jumpSize docs
////////////////////////////////////////////////////////////////////////////////

static void* SkiplistPrevsIterationCallback(TRI_skiplist_iterator_t* iterator, int64_t jumpSize) {
  return SkiplistIteration(iterator,-jumpSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an element to a key
////////////////////////////////////////////////////////////////////////////////

static void FillSkiplistIndexKeyBySkiplistIndexElement (SkiplistIndex* idx,
                                                        TRI_skiplist_index_key_t* key,
                                                        TRI_skiplist_index_element_t* element) {
  char const* ptr;
  size_t n;
  size_t i;

  key->numFields = element->numFields;
  key->collection = element->collection;

  n = element->numFields;
  key->fields = TRI_Allocate(TRI_CORE_MEM_ZONE, n * sizeof(TRI_shaped_json_t), false);

  ptr = (char const*) element->_document->_data;

  for (i = 0;  i < n;  ++i) {
    key->fields[i]._sid = element->_subObjects[i]._sid;
    key->fields[i]._data.length = element->_subObjects[i]._length;
    key->fields[i]._data.data = CONST_CAST(ptr + element->_subObjects[i]._offset);
  }
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

int SkiplistIndex_assignMethod(void* methodHandle, TRI_index_method_assignment_type_e methodType) {
  switch (methodType) {
  
    case TRI_INDEX_METHOD_ASSIGNMENT_FREE : {
      TRI_index_query_free_method_call_t* call = (TRI_index_query_free_method_call_t*)(methodHandle);
      *call = SkiplistIndex_freeMethodCall;
      break;
    }
    
    case TRI_INDEX_METHOD_ASSIGNMENT_QUERY : {
      TRI_index_query_method_call_t* call = (TRI_index_query_method_call_t*)(methodHandle);
      *call = SkiplistIndex_queryMethodCall;
      break;
    }
    
    case TRI_INDEX_METHOD_ASSIGNMENT_RESULT : {
      TRI_index_query_result_method_call_t* call = (TRI_index_query_result_method_call_t*)(methodHandle);
      *call = SkiplistIndex_resultMethodCall;
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

void TRI_FreeSkiplistIterator (TRI_skiplist_iterator_t* const iterator) {
  assert(iterator);

  TRI_DestroyVector(&iterator->_intervals);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, iterator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex_destroy(SkiplistIndex* slIndex) {
  if (slIndex == NULL) {
    return;
  } 
  
  if (slIndex->unique) {
    TRI_FreeSkipList(slIndex->skiplist.uniqueSkiplist);
    slIndex->skiplist.uniqueSkiplist = NULL;
  }
  else {
    TRI_FreeSkipListMulti(slIndex->skiplist.nonUniqueSkiplist);
    slIndex->skiplist.nonUniqueSkiplist = NULL;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex_free(SkiplistIndex* slIndex) {
  if (slIndex == NULL) {
    return;
  }  
  SkiplistIndex_destroy(slIndex);
  TRI_Free(TRI_CORE_MEM_ZONE, slIndex);
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

SkiplistIndex* SkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(SkiplistIndex), true);

  skiplistIndex->unique = true;
  skiplistIndex->skiplist.uniqueSkiplist = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_t), true);
  if (skiplistIndex->skiplist.uniqueSkiplist == NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipList(skiplistIndex->skiplist.uniqueSkiplist,
                   TRI_SKIPLIST_PROB_HALF,
                   40);

  return skiplistIndex;
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

static bool skiplistIndex_findHelperIntervalIntersectionValid (SkiplistIndex* skiplistIndex,
                                                               TRI_skiplist_iterator_interval_t* lInterval, 
                                                               TRI_skiplist_iterator_interval_t* rInterval, 
                                                               TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;

  lNode = lInterval->_leftEndPoint;
  rNode = rInterval->_leftEndPoint;
    
  if (lNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist) || lNode == NULL || 
      rNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist) || rNode == NULL) {
    return false;
  }

  if (lNode == TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = -1;
  }
  else if (rNode == TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = 1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareElementElement(
        skiplistIndex->skiplist.uniqueSkiplist, 
        &(lNode->_element),
        &(rNode->_element),
        0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  
  
  lNode = lInterval->_rightEndPoint;
  rNode = rInterval->_rightEndPoint;
  
  if (lNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareElementElement(
        skiplistIndex->skiplist.uniqueSkiplist, 
        &(lNode->_element),
        &(rNode->_element), 
        0);
  }
  
  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {  
    interval->_rightEndPoint = rNode;
  }  

  return skiplistIndex_findHelperIntervalValid(skiplistIndex, interval); 
}

static bool skiplistIndex_findHelperIntervalValid(SkiplistIndex* skiplistIndex, TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;
  
  
  if ((interval->_leftEndPoint == NULL) || (interval->_rightEndPoint == NULL)) {
    return false;
  }

  if (interval->_leftEndPoint == interval->_rightEndPoint) {
    return false;
  }
    
  if ( (interval->_leftEndPoint  == TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) || 
       (interval->_rightEndPoint == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)))  {
    return true;
  }
  
  lNode = interval->_leftEndPoint;
  rNode = interval->_rightEndPoint;

  compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareElementElement(
      skiplistIndex->skiplist.uniqueSkiplist, 
      &(lNode->_element),
      &(rNode->_element),
      0);
  return (compareResult == -1);                                              
} 


static void SkiplistIndex_findHelper(SkiplistIndex* skiplistIndex, 
                                     TRI_vector_t* shapeList, 
                                     TRI_index_operator_t* indexOperator,
                                     TRI_vector_t* resultIntervalList) {
                                 
  TRI_skiplist_index_key_t          values;
  TRI_vector_t                      leftResult;
  TRI_vector_t                      rightResult;
  TRI_relation_index_operator_t*    relationOperator;
  TRI_logical_index_operator_t*     logicalOperator;
  TRI_skiplist_iterator_interval_t  interval; 
  TRI_skiplist_iterator_interval_t* tempLeftInterval; 
  TRI_skiplist_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  TRI_InitVector(&(rightResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  
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
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_left,&leftResult); 
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_right,&rightResult); 
      
      for (i = 0; i < leftResult._length; ++i) {
        for (j = 0; j < rightResult._length; ++j) {
          tempLeftInterval  =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&leftResult, i));    
          tempRightInterval =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&rightResult, j));    

          if (!skiplistIndex_findHelperIntervalIntersectionValid(skiplistIndex,
                                                                 tempLeftInterval, 
                                                                 tempRightInterval,
                                                                 &interval)) {
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
        interval._leftEndPoint  = TRI_LookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 

        if (interval._leftEndPoint != NULL) {
          interval._rightEndPoint = TRI_NextNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
          interval._leftEndPoint  = TRI_PrevNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
        }        
      }  
      // ............................................................................
      // The index is constructed from n fields and the client has sent us m values  
      // where m < n
      // ............................................................................

      else {
        interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
        interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      }      
      
      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }

      return;    
    }

    
    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
      interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  

      return;
    }  

    
    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
      interval._rightEndPoint = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 

      if (interval._rightEndPoint != TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) { 
        interval._rightEndPoint = TRI_NextNodeSkipList (skiplistIndex->skiplist.uniqueSkiplist, interval._rightEndPoint);      
      }      

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  

      return;
    }  
    

    case TRI_GE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  

      return;
    }  
  
  
    case TRI_GT_INDEX_OPERATOR: {
      interval._leftEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);

      if (interval._leftEndPoint != TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
        interval._leftEndPoint = TRI_PrevNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist, interval._leftEndPoint);      
      }  

      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  

      return;
    }  
    
    default: {
      assert(0);
    }
    
  } // end of switch statement
}

TRI_skiplist_iterator_t* SkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_index_operator_t* indexOperator) {
  TRI_skiplist_iterator_t*         results;
 
  results = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_t), true);
  if (results == NULL) {
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  SkiplistIndex_findHelper(skiplistIndex, shapeList, indexOperator, &(results->_intervals));
  
  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into a unique skip list
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_insert (SkiplistIndex* skiplistIndex, TRI_skiplist_index_element_t* element) {
  TRI_skiplist_index_key_t key;
  int result;

  FillSkiplistIndexKeyBySkiplistIndexElement(skiplistIndex, &key, element);
  result = TRI_InsertKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &key, element, false);
  TRI_Free(TRI_CORE_MEM_ZONE, key.fields);

  return result;
}

//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skip list
//////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_remove (SkiplistIndex* skiplistIndex, TRI_skiplist_index_element_t* element) {
  int result;

  result = TRI_RemoveElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element, NULL); 

  if (result == TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING) {
    return TRI_ERROR_NO_ERROR;
  }

  return result;
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

SkiplistIndex* MultiSkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(SkiplistIndex), true);

  skiplistIndex->unique = false;
  skiplistIndex->skiplist.nonUniqueSkiplist = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_multi_t), true);
  if (skiplistIndex->skiplist.nonUniqueSkiplist == NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist,
                        TRI_SKIPLIST_PROB_HALF,
                        40);

  return skiplistIndex;
}

//////////////////////////////////////////////////////////////////////////////////
/// @brief Locates one or more ranges within the unique skiplist and returns iterator
//////////////////////////////////////////////////////////////////////////////////
/*
static void debugElementMulti(SkiplistIndex* skiplistIndex, TRI_skiplist_node_t* node) {
  size_t numFields;
  SkiplistIndexElement* element = (SkiplistIndexElement*)(&(node->_element));
  TRI_shaper_t* shaper;
  size_t j;
  
  if (node == NULL) {
    printf("%s:%u:node null\n",__FILE__,__LINE__);
    return;
  }    
  
  if (node == TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    printf("%s:%u:start node\n",__FILE__,__LINE__);
  }
  
  if (node == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
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

static bool multiSkiplistIndex_findHelperIntervalIntersectionValid (SkiplistIndex* skiplistIndex,
                                                                    TRI_skiplist_iterator_interval_t* lInterval, 
                                                                    TRI_skiplist_iterator_interval_t* rInterval, 
                                                                    TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;

  lNode = lInterval->_leftEndPoint;
  rNode = rInterval->_leftEndPoint;
    
  if (lNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist) || lNode == NULL || 
      rNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist) || rNode == NULL) {
    return false;
  }

  if (lNode == TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = -1;
  }
  else if (rNode == TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = 1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareElementElement(
        skiplistIndex->skiplist.nonUniqueSkiplist, 
        &(lNode->_element),
        &(rNode->_element),
        0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  
  
  lNode = lInterval->_rightEndPoint;
  rNode = rInterval->_rightEndPoint;
  
  if (lNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareElementElement(
        skiplistIndex->skiplist.nonUniqueSkiplist, 
        &(lNode->_element),
        &(rNode->_element),
        0);
  }
  
  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {  
    interval->_rightEndPoint = rNode;
  }  

  return multiSkiplistIndex_findHelperIntervalValid(skiplistIndex, interval); 
}



static bool multiSkiplistIndex_findHelperIntervalValid (SkiplistIndex* skiplistIndex,
                                                        TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;
  
  if ((interval->_leftEndPoint == NULL) || (interval->_rightEndPoint == NULL)) {
    return 0;
  }

  if (interval->_leftEndPoint == interval->_rightEndPoint) {
    return 0;
  }
    
  if ( (interval->_leftEndPoint  == TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) || 
       (interval->_rightEndPoint == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)))  {
    return -1;
  }
  
  lNode = interval->_leftEndPoint;
  rNode = interval->_rightEndPoint;

  compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareElementElement(
      skiplistIndex->skiplist.nonUniqueSkiplist, 
      &(lNode->_element),
      &(rNode->_element),
      0);
                                              
  return (compareResult == -1);  
} 



static void MultiSkiplistIndex_findHelper(SkiplistIndex* skiplistIndex, 
                                          TRI_vector_t* shapeList, 
                                          TRI_index_operator_t* indexOperator,
                                          TRI_vector_t* resultIntervalList) {
                                 
  TRI_skiplist_index_key_t          values;
  TRI_vector_t                      leftResult;
  TRI_vector_t                      rightResult;
  TRI_relation_index_operator_t*    relationOperator;
  TRI_logical_index_operator_t*     logicalOperator;
  TRI_skiplist_iterator_interval_t  interval; 
  TRI_skiplist_iterator_interval_t* tempLeftInterval; 
  TRI_skiplist_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  TRI_InitVector(&(rightResult), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  
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
      MultiSkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_left,&leftResult); 
      MultiSkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_right,&rightResult); 
      
      for (i = 0; i < leftResult._length; ++i) {
        for (j = 0; j < rightResult._length; ++j) {
          tempLeftInterval  =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&leftResult, i));    
          tempRightInterval =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&rightResult, j));    
          if (!multiSkiplistIndex_findHelperIntervalIntersectionValid(skiplistIndex,tempLeftInterval, 
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
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;    
    }
        
    
    case TRI_LE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;
    }  
    
    
    case TRI_LT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      interval._rightEndPoint = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      if (interval._rightEndPoint != TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
        interval._rightEndPoint = TRI_NextNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, interval._rightEndPoint);      
      }
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }      
      return;
    }  
    

    case TRI_GE_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;
    }  
  
  
    case TRI_GT_INDEX_OPERATOR: {
      interval._leftEndPoint  = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      if (interval._leftEndPoint != TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
        interval._leftEndPoint = TRI_PrevNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, interval._leftEndPoint);      
      }
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }      
      return;
    }  
    
    default: {
      assert(0);
    }
    
  } // end of switch statement  
}


TRI_skiplist_iterator_t* MultiSkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_index_operator_t* indexOperator) {
  TRI_skiplist_iterator_t* results;
 
  results = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_t), false);    
  if (results == NULL) {
    return NULL;
  }  
  
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  MultiSkiplistIndex_findHelper(skiplistIndex, shapeList, indexOperator, &(results->_intervals));
    
  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into a multi skiplist
////////////////////////////////////////////////////////////////////////////////


int MultiSkiplistIndex_insert(SkiplistIndex* skiplistIndex, TRI_skiplist_index_element_t* element) {
  int result;
  result = TRI_InsertElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, false);  
  return result;
}

//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skiplist
//////////////////////////////////////////////////////////////////////////////////

int MultiSkiplistIndex_remove(SkiplistIndex* skiplistIndex, TRI_skiplist_index_element_t* element) {
  int result;
  result = TRI_RemoveElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, NULL); 
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared query engine callback functions
////////////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndex_queryMethodCall(void* theIndex, TRI_index_operator_t* indexOperator, 
                                         TRI_index_challenge_t* challenge, void* data) {
  SkiplistIndex* slIndex = (SkiplistIndex*)(theIndex);
  if (slIndex == NULL || indexOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  assert(false);
  return TRI_ERROR_NO_ERROR;
}

static TRI_index_iterator_t* SkiplistIndex_resultMethodCall(void* theIndex, TRI_index_operator_t* indexOperator, 
                                          void* data, bool (*filter) (TRI_index_iterator_t*)) {
  SkiplistIndex* slIndex = (SkiplistIndex*)(theIndex);
  if (slIndex == NULL || indexOperator == NULL) {
    return NULL;
  }
  assert(false);
  return NULL;
}

static int SkiplistIndex_freeMethodCall(void* theIndex, void* data) {
  SkiplistIndex* slIndex = (SkiplistIndex*)(theIndex);
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


