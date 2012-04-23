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
#include "VocBase/document-collection.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Common private methods
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


// forward declaration
static bool skiplistIndex_findHelperIntervalValid(SkiplistIndex*, TRI_skiplist_iterator_interval_t*);
static bool multiSkiplistIndex_findHelperIntervalValid(SkiplistIndex*, TRI_skiplist_iterator_interval_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper method for recursion for CompareShapedJsonShapedJson
////////////////////////////////////////////////////////////////////////////////

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
      /* start oreste: 
        char* shape = (char*)(leftShape);
        uint64_t fixedEntries;
        uint64_t variableEntries;
        uint64_t ssid;
        uint64_t aaid;
        char* name;
        TRI_shape_t* newShape;
        
        shape = shape + sizeof(TRI_shape_t);        
        fixedEntries = *((TRI_shape_size_t*)(shape));
        shape = shape + sizeof(TRI_shape_size_t);
        variableEntries = *((TRI_shape_size_t*)(shape));
        shape = shape + sizeof(TRI_shape_size_t);
        ssid = *((TRI_shape_sid_t*)(shape));
        shape = shape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
        aaid = *((TRI_shape_aid_t*)(shape));
        shape = shape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
        
        name      = leftShaper->lookupAttributeId(leftShaper,aaid);
        newShape  = leftShaper->lookupShapeId(leftShaper, ssid);

        
        printf("%s:%u:_fixedEntries:%u\n",__FILE__,__LINE__,fixedEntries);
        printf("%s:%u:_variableEntries:%u\n",__FILE__,__LINE__,variableEntries);
        printf("%s:%u:_sids[0]:%u\n",__FILE__,__LINE__,ssid);
        printf("%s:%u:_aids[0]:%u\n",__FILE__,__LINE__,aaid);
        printf("%s:%u:name:%s\n",__FILE__,__LINE__,name);
        printf("%s:%u:type:%d\n",__FILE__,__LINE__,newShape->_type);
                         
       end oreste */
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
 
  currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
  
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
    
    currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);

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
    
    currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);

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

      currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
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

  
  currentNode = (TRI_skiplist_node_t*) (iterator->_cursor);
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




// -----------------------------------------------------------------------------
// --SECTION--                           skiplistIndex     common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a skiplist iterator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIterator (TRI_skiplist_iterator_t* const iterator) {
  assert(iterator);

  TRI_DestroyVector(&iterator->_intervals);
  TRI_Free(iterator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndexDestroy(SkiplistIndex* slIndex) {
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

void SkiplistIndexFree(SkiplistIndex* slIndex) {
  if (slIndex == NULL) {
    return;
  }  
  SkiplistIndexDestroy(slIndex);
  TRI_Free(slIndex);
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


////////////////////////////////////////////////////////////////////////////////
/// @brief compares two elements in a skip list
////////////////////////////////////////////////////////////////////////////////

static int CompareElementElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {                                
  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                                    
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    assert(false);
  }
  
  
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    

  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < hLeftElement->numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
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

static int CompareKeyElement (struct TRI_skiplist_s* skiplist, void* leftElement, void* rightElement, int defaultEqual) {
  // .............................................................................
  // Compare two elements and determines:
  // left < right   : return -1
  // left == right  : return 0
  // left > right   : return 1
  // .............................................................................
  int compareResult;                               
  size_t numFields;
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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

  if (leftElement == rightElement) {
    return 0;
  }    
    
  // ............................................................................
  // The document could be the same -- so no further comparison is required.
  // ............................................................................
  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    
  
  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................

  
  if (hLeftElement->numFields < hRightElement->numFields) {
    numFields = hLeftElement->numFields;
  }
  else {
    numFields = hRightElement->numFields;
  }
  
  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
  /*
  printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
    *((double*)((j + hLeftElement->fields)->_data.data)),
    *((double*)((j + hRightElement->fields)->_data.data)),
    (uint64_t)(hLeftElement->data),
    (uint64_t)(hRightElement->data)
  );
  */
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), 
                                                (j + hRightElement->fields),
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




//------------------------------------------------------------------------------
// Public Methods Unique Skiplists
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new unique entry skiplist
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex* SkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(sizeof(SkiplistIndex));
  if (skiplistIndex == NULL) {
    return NULL;
  }

  skiplistIndex->unique = true;
  skiplistIndex->skiplist.uniqueSkiplist = TRI_Allocate(sizeof(TRI_skiplist_t));
  if (skiplistIndex->skiplist.uniqueSkiplist == NULL) {
    TRI_Free(skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipList(skiplistIndex->skiplist.uniqueSkiplist,
                   sizeof(SkiplistIndexElement),
                   CompareElementElement, 
                   CompareKeyElement,
                   TRI_SKIPLIST_PROB_HALF, 40);
  
  return skiplistIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a unique skip list
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, element, element, false);  
  if (result) {
   return 0;
  }    
  return -1;
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
  shaper    = ((TRI_doc_collection_t*)(element->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    printf("%s:%u:!!!:%f:%lu\n",__FILE__,__LINE__,
      *((double*)((j + element->fields)->_data.data)),
      (long unsigned int)(element->data) );
  }
  return;
}
*/

static bool skiplistIndex_findHelperIntervalIntersectionValid(SkiplistIndex* skiplistIndex,
                                                TRI_skiplist_iterator_interval_t* lInterval, 
                                                TRI_skiplist_iterator_interval_t* rInterval, 
                                                TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;

  lNode = (TRI_skiplist_node_t*)(lInterval->_leftEndPoint);
  rNode = (TRI_skiplist_node_t*)(rInterval->_leftEndPoint);
    
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
    compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareKeyElement(
                                                skiplistIndex->skiplist.uniqueSkiplist, 
                                                &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  


  
  lNode = (TRI_skiplist_node_t*)(lInterval->_rightEndPoint);
  rNode = (TRI_skiplist_node_t*)(rInterval->_rightEndPoint);
  
  if (lNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareKeyElement(
                                              skiplistIndex->skiplist.uniqueSkiplist, 
                                              &(lNode->_element), &(rNode->_element), 0);
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
  
  lNode = (TRI_skiplist_node_t*)(interval->_leftEndPoint);
  rNode = (TRI_skiplist_node_t*)(interval->_rightEndPoint);


  compareResult = skiplistIndex->skiplist.uniqueSkiplist->compareKeyElement(
                                              skiplistIndex->skiplist.uniqueSkiplist, 
                                              &(lNode->_element), &(rNode->_element), 0);
  return (compareResult == -1);                                              
} 


static void SkiplistIndex_findHelper(SkiplistIndex* skiplistIndex, 
                                     TRI_vector_t* shapeList, 
                                     TRI_sl_operator_t* slOperator,
                                     TRI_vector_t* resultIntervalList) {
                                 
  SkiplistIndexElement        values;
  TRI_vector_t                leftResult;
  TRI_vector_t                rightResult;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_sl_logical_operator_t*  logicalOperator;
  TRI_skiplist_iterator_interval_t interval; 
  TRI_skiplist_iterator_interval_t* tempLeftInterval; 
  TRI_skiplist_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), sizeof(TRI_skiplist_iterator_interval_t));
  TRI_InitVector(&(rightResult), sizeof(TRI_skiplist_iterator_interval_t));
  
  relationOperator  = (TRI_sl_relation_operator_t*)(slOperator);
  logicalOperator   = (TRI_sl_logical_operator_t*)(slOperator);
  
  switch (slOperator->_type) {
    case TRI_SL_EQ_OPERATOR:
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
      values.fields     = relationOperator->_fields;
      values.numFields  = relationOperator->_numFields;
      values.collection = relationOperator->_collection;
    default: {
      // must not access relationOperator->xxx if the operator is not a relational one
      // otherwise we'll get invalid reads and the prog might crash
    }
  }
  
  switch (slOperator->_type) {

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
    
    case TRI_SL_AND_OPERATOR: {
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_left,&leftResult); 
      SkiplistIndex_findHelper(skiplistIndex,shapeList,logicalOperator->_right,&rightResult); 
      
      for (i = 0; i < leftResult._length; ++i) {
        for (j = 0; j < rightResult._length; ++j) {
          tempLeftInterval  =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&leftResult, i));    
          tempRightInterval =  (TRI_skiplist_iterator_interval_t*) (TRI_AtVector(&rightResult, j));    
          if (!skiplistIndex_findHelperIntervalIntersectionValid(skiplistIndex,tempLeftInterval, 
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

    
    case TRI_SL_EQ_OPERATOR: {
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
    
    case TRI_SL_LE_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
      interval._rightEndPoint = TRI_RightLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  
      return;
    }  
    
    
    case TRI_SL_LT_OPERATOR: {
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
    

    case TRI_SL_GE_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipList(skiplistIndex->skiplist.uniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipList(skiplistIndex->skiplist.uniqueSkiplist);
      if (skiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }  
      return;
    }  
  
  
    case TRI_SL_GT_OPERATOR: {
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

TRI_skiplist_iterator_t* SkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_sl_operator_t* slOperator) {
  TRI_skiplist_iterator_t*         results;
 
  results = TRI_Allocate(sizeof(TRI_skiplist_iterator_t));    
  if (results == NULL) {
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  SkiplistIndex_findHelper(skiplistIndex, shapeList, slOperator, &(results->_intervals));
  
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return SkiplistIndex_add(skiplistIndex,element);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skip list
//////////////////////////////////////////////////////////////////////////////////

bool SkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_RemoveElementSkipList(skiplistIndex->skiplist.uniqueSkiplist, element, NULL); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a skiplist entry
//////////////////////////////////////////////////////////////////////////////////

bool SkiplistIndex_update(SkiplistIndex* skiplistIndex, const SkiplistIndexElement* beforeElement, const SkiplistIndexElement* afterElement) {
  // updates an entry in the skip list, first removes beforeElement, 
  // then adds the afterElement -- should never be called here
  // call SkiplistIndex_remove first and then SkiplistIndex_add
  assert(false);
}







//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------



////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two elements
////////////////////////////////////////////////////////////////////////////////

static int MultiCompareElementElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {

  int compareResult;                                    
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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

  if (hLeftElement->numFields != hRightElement->numFields) {
    assert(false);
  }
  
  if (hLeftElement->data == hRightElement->data) {
    return TRI_SKIPLIST_COMPARE_STRICTLY_EQUAL;
  }    


  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < hLeftElement->numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return defaultEqual;  
}



////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static int  MultiCompareKeyElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement, int defaultEqual) {

  int compareResult;                                    
  size_t numFields;
  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);
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
  // The document could be the same -- so no further comparison is required.
  // ............................................................................

  if (hLeftElement->data == hRightElement->data) {
    return 0;
  }    

  
  // ............................................................................
  // This call back function is used when we query the index, as such
  // the number of fields which we are using for the query may be less than
  // the number of fields that the index is defined with.
  // ............................................................................
  
  if (hLeftElement->numFields < hRightElement->numFields) {
    numFields = hLeftElement->numFields;
  }
  else {
    numFields = hRightElement->numFields;
  }
  
  leftShaper  = ((TRI_doc_collection_t*)(hLeftElement->collection))->_shaper;
  rightShaper = ((TRI_doc_collection_t*)(hRightElement->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    compareResult = CompareShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields), leftShaper, rightShaper);
    if (compareResult != 0) {
      return compareResult;
    }
  }
  
  return defaultEqual;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief used to determine the order of two keys
////////////////////////////////////////////////////////////////////////////////

static bool MultiEqualElementElement (TRI_skiplist_multi_t* multiSkiplist, void* leftElement, void* rightElement) {

  SkiplistIndexElement* hLeftElement  = (SkiplistIndexElement*)(leftElement);
  SkiplistIndexElement* hRightElement = (SkiplistIndexElement*)(rightElement);

  if (leftElement == rightElement) {
    return true;
  }    

  /*
  printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
    *((double*)((hLeftElement->fields)->_data.data)),
    *((double*)((hRightElement->fields)->_data.data)),
    (uint64_t)(hLeftElement->data),
    (uint64_t)(hRightElement->data)
  );
  */
  return (hLeftElement->data == hRightElement->data);
}




//------------------------------------------------------------------------------
// Public Methods Non-Unique Muilti Skiplists
//------------------------------------------------------------------------------






////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new non-uniqe (allows duplicates) multi skiplist
////////////////////////////////////////////////////////////////////////////////

SkiplistIndex* MultiSkiplistIndex_new() {
  SkiplistIndex* skiplistIndex;

  skiplistIndex = TRI_Allocate(sizeof(SkiplistIndex));
  if (skiplistIndex == NULL) {
    return NULL;
  }

  skiplistIndex->unique = false;
  skiplistIndex->skiplist.nonUniqueSkiplist = TRI_Allocate(sizeof(TRI_skiplist_multi_t));
  if (skiplistIndex->skiplist.nonUniqueSkiplist == NULL) {
    TRI_Free(skiplistIndex);
    return NULL;
  }    
    
  TRI_InitSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist,
                        sizeof(SkiplistIndexElement),
                        MultiCompareElementElement,
                        MultiCompareKeyElement, 
                        MultiEqualElementElement,
                        TRI_SKIPLIST_PROB_HALF, 40);
                   
  return skiplistIndex;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into a multi skiplist
////////////////////////////////////////////////////////////////////////////////


int MultiSkiplistIndex_add(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_InsertElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, false);  
  if (result) {
    return 0;
  }    
  // failure could be caused by attempting to add the SAME (as in same doc ptr) document
  return -1;
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
  shaper    = ((TRI_doc_collection_t*)(element->collection))->_shaper;
  
  for (j = 0; j < numFields; j++) {
    printf("%s:%u:!!!:%f:%lu\n",__FILE__,__LINE__,
      *((double*)((j + element->fields)->_data.data)),
      (long unsigned int)(element->data) );
  }
  return;
}
*/

static bool multiSkiplistIndex_findHelperIntervalIntersectionValid(SkiplistIndex* skiplistIndex,
                                                TRI_skiplist_iterator_interval_t* lInterval, 
                                                TRI_skiplist_iterator_interval_t* rInterval, 
                                                TRI_skiplist_iterator_interval_t* interval) {
  int compareResult;
  TRI_skiplist_node_t* lNode;
  TRI_skiplist_node_t* rNode;

  lNode = (TRI_skiplist_node_t*)(lInterval->_leftEndPoint);
  rNode = (TRI_skiplist_node_t*)(rInterval->_leftEndPoint);
    
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
    compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareKeyElement(
                                                skiplistIndex->skiplist.nonUniqueSkiplist, 
                                                &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_leftEndPoint = rNode;
  }
  else {  
    interval->_leftEndPoint = lNode;
  }  


  
  lNode = (TRI_skiplist_node_t*)(lInterval->_rightEndPoint);
  rNode = (TRI_skiplist_node_t*)(rInterval->_rightEndPoint);
  
  if (lNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = 1;
  }
  else if (rNode == TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist)) {
    compareResult = -1;
  }  
  else {
    compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareKeyElement(
                                              skiplistIndex->skiplist.nonUniqueSkiplist, 
                                              &(lNode->_element), &(rNode->_element), 0);
  }
  
  if (compareResult < 1) {
    interval->_rightEndPoint = lNode;
  }
  else {  
    interval->_rightEndPoint = rNode;
  }  

  return multiSkiplistIndex_findHelperIntervalValid(skiplistIndex, interval); 
}



static bool multiSkiplistIndex_findHelperIntervalValid(SkiplistIndex* skiplistIndex, TRI_skiplist_iterator_interval_t* interval) {
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
  
  lNode = (TRI_skiplist_node_t*)(interval->_leftEndPoint);
  rNode = (TRI_skiplist_node_t*)(interval->_rightEndPoint);


  compareResult = skiplistIndex->skiplist.nonUniqueSkiplist->compareKeyElement(
                                              skiplistIndex->skiplist.nonUniqueSkiplist, 
                                              &(lNode->_element), &(rNode->_element), 0);
  return (compareResult == -1);  
} 



static void MultiSkiplistIndex_findHelper(SkiplistIndex* skiplistIndex, 
                                          TRI_vector_t* shapeList, 
                                          TRI_sl_operator_t* slOperator,
                                          TRI_vector_t* resultIntervalList) {
                                 
  SkiplistIndexElement        values;
  TRI_vector_t                leftResult;
  TRI_vector_t                rightResult;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_sl_logical_operator_t*  logicalOperator;
  TRI_skiplist_iterator_interval_t interval; 
  TRI_skiplist_iterator_interval_t* tempLeftInterval; 
  TRI_skiplist_iterator_interval_t* tempRightInterval; 
  size_t j;
  size_t i;
  
  TRI_InitVector(&(leftResult), sizeof(TRI_skiplist_iterator_interval_t));
  TRI_InitVector(&(rightResult), sizeof(TRI_skiplist_iterator_interval_t));
  
  logicalOperator   = (TRI_sl_logical_operator_t*)(slOperator);
  relationOperator  = (TRI_sl_relation_operator_t*)(slOperator);

  switch (slOperator->_type) {
    case TRI_SL_EQ_OPERATOR:
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
      values.fields     = relationOperator->_fields;
      values.numFields  = relationOperator->_numFields;
      values.collection = relationOperator->_collection;
    default: {
      // must not access relationOperator->xxx if the operator is not a relational one
      // otherwise we'll get invalid reads and the prog might crash
    }
  }
  
  switch (slOperator->_type) {

    /*
    case TRI_SL_OR_OPERATOR: {
      todo
    }
    */
    
    case TRI_SL_AND_OPERATOR: {
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

    
    case TRI_SL_EQ_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;    
    }
        
    
    case TRI_SL_LE_OPERATOR: {
      interval._leftEndPoint  = TRI_StartNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      interval._rightEndPoint = TRI_RightLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;
    }  
    
    
    case TRI_SL_LT_OPERATOR: {
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
    

    case TRI_SL_GE_OPERATOR: {
      interval._leftEndPoint  = TRI_LeftLookupByKeySkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, &values); 
      interval._rightEndPoint = TRI_EndNodeSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist);
      if (multiSkiplistIndex_findHelperIntervalValid(skiplistIndex,&interval)) {
        TRI_PushBackVector(resultIntervalList,&interval);
      }
      return;
    }  
  
  
    case TRI_SL_GT_OPERATOR: {
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


TRI_skiplist_iterator_t* MultiSkiplistIndex_find(SkiplistIndex* skiplistIndex, TRI_vector_t* shapeList, TRI_sl_operator_t* slOperator) {
  TRI_skiplist_iterator_t* results;
 
  results = TRI_Allocate(sizeof(TRI_skiplist_iterator_t));    
  if (results == NULL) {
    return NULL;
  }  
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  MultiSkiplistIndex_findHelper(skiplistIndex, shapeList, slOperator, &(results->_intervals));
    
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for addIndex 
//////////////////////////////////////////////////////////////////////////////////

int MultiSkiplistIndex_insert(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  return MultiSkiplistIndex_add(skiplistIndex,element);
} 


//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the skiplist
//////////////////////////////////////////////////////////////////////////////////

bool MultiSkiplistIndex_remove(SkiplistIndex* skiplistIndex, SkiplistIndexElement* element) {
  bool result;
  result = TRI_RemoveElementSkipListMulti(skiplistIndex->skiplist.nonUniqueSkiplist, element, NULL); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates and entry in a multi skiplist
//////////////////////////////////////////////////////////////////////////////////

bool MultiSkiplistIndex_update(SkiplistIndex* skiplistIndex, SkiplistIndexElement* beforeElement, SkiplistIndexElement* afterElement) {
  assert(false); // should never be called directly
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


